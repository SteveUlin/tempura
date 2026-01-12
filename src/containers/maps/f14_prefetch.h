#pragma once

// ============================================================================
// F14 with Software Prefetching
// ============================================================================
//
// This variant extends F14 with explicit software prefetching to hide memory
// latency during probe sequences.
//
// F14's 64-byte aligned chunk structure makes prefetching particularly effective:
// - Single prefetch covers the entire next chunk (tags + metadata + slots start)
// - No separate ctrl/slots arrays to prefetch
//
// ============================================================================

#include <bit>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <immintrin.h>
#include <memory>
#include <utility>

#include "containers/maps/hasher.h"
#include "containers/maps/map_storage.h"
#include "meta/manual_lifetime.h"
#include "simd/simd.h"

namespace tempura {

namespace f14pf {

inline constexpr std::uint8_t kEmpty = 0xFF;

constexpr auto h2(std::size_t hash) -> std::uint8_t {
  return static_cast<std::uint8_t>(hash & 0x7F);
}

}  // namespace f14pf

// ============================================================================
// F14PrefetchChunk - 14 Slots with Overflow Counting
// ============================================================================

template <typename Key, typename Value>
struct alignas(64) F14PrefetchChunk {
  static constexpr std::size_t kCapacity = 14;
  static constexpr std::size_t kTagBytes = 16;
  static constexpr std::size_t kOverflowIdx = 14;
  static constexpr std::size_t kHostedIdx = 15;
  static constexpr std::uint16_t kSlotMask = 0x3FFF;

  std::uint8_t tags[kTagBytes];

  struct Slot {
    ManualLifetime<MapStorage<Key, Value>> storage;

    auto mutableData() -> std::pair<Key, Value>* {
      return &storage.get().mutable_pair;
    }

    auto data() -> std::pair<const Key, Value>* {
      return &storage.get().const_pair;
    }

    auto data() const -> const std::pair<const Key, Value>* {
      return &storage.get().const_pair;
    }

    template <typename... Args>
    void construct(Args&&... args) {
      storage.construct();
      std::construct_at(&storage.get().mutable_pair,
                        std::forward<Args>(args)...);
    }

    void destroy() {
      std::destroy_at(&storage.get().mutable_pair);
      storage.destruct();
    }
  };

  Slot slots[kCapacity];

  auto match(std::uint8_t tag) const -> std::uint16_t {
    auto v = Vec16i8::load(tags);
    return v.match(tag) & kSlotMask;
  }

  auto matchEmpty() const -> std::uint16_t {
    auto v = Vec16i8::load(tags);
    return v.matchHighBitSet() & kSlotMask;
  }

  auto overflow() const -> std::uint8_t { return tags[kOverflowIdx]; }
  void incOverflow() { ++tags[kOverflowIdx]; }
  void decOverflow() { --tags[kOverflowIdx]; }

  void incHosted() { ++tags[kHostedIdx]; }
  void decHosted() { --tags[kHostedIdx]; }

  void reset() {
    std::memset(tags, f14pf::kEmpty, kCapacity);
    tags[kOverflowIdx] = 0;
    tags[kHostedIdx] = 0;
  }
};

// ============================================================================
// BitMask Iterator
// ============================================================================

class F14PrefetchBitMask {
 public:
  explicit constexpr F14PrefetchBitMask(std::uint16_t mask) : mask_{mask} {}

  constexpr auto operator*() const -> std::size_t {
    return std::countr_zero(mask_);
  }

  constexpr auto operator++() -> F14PrefetchBitMask& {
    mask_ &= (mask_ - 1);
    return *this;
  }

  constexpr explicit operator bool() const { return mask_ != 0; }

 private:
  std::uint16_t mask_;
};

// ============================================================================
// Probe Sequence with nextOffset() for prefetching
// ============================================================================

class F14PrefetchProbeSeq {
 public:
  constexpr F14PrefetchProbeSeq(std::size_t hash, std::size_t mask)
      : mask_{mask}, offset_{hash & mask_} {}

  constexpr auto offset() const -> std::size_t { return offset_; }

  constexpr auto nextOffset() const -> std::size_t {
    std::size_t next_index = index_ + 1;
    return (offset_ + next_index) & mask_;
  }

  constexpr void next() {
    index_ += 1;
    offset_ = (offset_ + index_) & mask_;
  }

 private:
  std::size_t mask_;
  std::size_t offset_;
  std::size_t index_ = 0;
};

// ============================================================================
// F14Prefetch
// ============================================================================

template <typename Key, typename Value, Hasher<Key> Hash = std::hash<Key>,
          typename KeyEqual = std::equal_to<Key>>
class F14Prefetch {
 public:
  using key_type = Key;
  using mapped_type = Value;
  using value_type = std::pair<const Key, Value>;
  using size_type = std::size_t;
  using hasher = Hash;
  using key_equal = KeyEqual;

  using Chunk = F14PrefetchChunk<Key, Value>;
  using Slot = typename Chunk::Slot;

  static constexpr size_type kLoadFactorNumerator = 12;
  static constexpr size_type kLoadFactorDenominator = 14;
  static constexpr size_type kMinChunks = 4;

  // --------------------------------------------------------------------------
  // Constructors
  // --------------------------------------------------------------------------

  constexpr F14Prefetch() : F14Prefetch{kMinChunks * Chunk::kCapacity} {}

  constexpr explicit F14Prefetch(size_type initial_capacity)
      : num_chunks_{normalizeNumChunks(
            (std::max(initial_capacity, kMinChunks * Chunk::kCapacity) +
             Chunk::kCapacity - 1) / Chunk::kCapacity)},
        chunks_{std::make_unique<Chunk[]>(num_chunks_)} {
    resetAllChunks();
  }

  constexpr F14Prefetch(const F14Prefetch& other)
      : num_chunks_{other.num_chunks_},
        size_{other.size_},
        chunks_{std::make_unique<Chunk[]>(num_chunks_)} {
    for (size_type c = 0; c < num_chunks_; ++c) {
      std::memcpy(chunks_[c].tags, other.chunks_[c].tags, Chunk::kTagBytes);
      for (size_type i = 0; i < Chunk::kCapacity; ++i) {
        if (isOccupied(chunks_[c].tags[i])) {
          chunks_[c].slots[i].construct(*other.chunks_[c].slots[i].data());
        }
      }
    }
  }

  constexpr F14Prefetch(F14Prefetch&& other) noexcept
      : num_chunks_{other.num_chunks_},
        size_{other.size_},
        chunks_{std::move(other.chunks_)} {
    other.num_chunks_ = 0;
    other.size_ = 0;
  }

  constexpr auto operator=(const F14Prefetch& other) -> F14Prefetch& {
    if (this != &other) {
      F14Prefetch tmp{other};
      swap(tmp);
    }
    return *this;
  }

  constexpr auto operator=(F14Prefetch&& other) noexcept -> F14Prefetch& {
    if (this != &other) {
      F14Prefetch tmp{std::move(other)};
      swap(tmp);
    }
    return *this;
  }

  constexpr ~F14Prefetch() {
    if (chunks_) {
      for (size_type c = 0; c < num_chunks_; ++c) {
        for (size_type i = 0; i < Chunk::kCapacity; ++i) {
          if (isOccupied(chunks_[c].tags[i])) {
            chunks_[c].slots[i].destroy();
          }
        }
      }
    }
  }

  constexpr void swap(F14Prefetch& other) noexcept {
    std::swap(num_chunks_, other.num_chunks_);
    std::swap(size_, other.size_);
    std::swap(chunks_, other.chunks_);
    std::swap(hasher_, other.hasher_);
    std::swap(equal_, other.equal_);
  }

  // --------------------------------------------------------------------------
  // Capacity
  // --------------------------------------------------------------------------

  constexpr auto size() const noexcept -> size_type { return size_; }
  constexpr auto capacity() const noexcept -> size_type {
    return num_chunks_ * Chunk::kCapacity;
  }
  constexpr auto empty() const noexcept -> bool { return size_ == 0; }

  // --------------------------------------------------------------------------
  // Lookup (with prefetching)
  // --------------------------------------------------------------------------

  constexpr auto find(const Key& key) -> Value* {
    auto [chunk_idx, slot_idx] = findSlot(key);
    if (chunk_idx == num_chunks_) return nullptr;
    return &chunks_[chunk_idx].slots[slot_idx].data()->second;
  }

  constexpr auto find(const Key& key) const -> const Value* {
    auto [chunk_idx, slot_idx] = findSlot(key);
    if (chunk_idx == num_chunks_) return nullptr;
    return &chunks_[chunk_idx].slots[slot_idx].data()->second;
  }

  constexpr auto contains(const Key& key) const -> bool {
    auto [chunk_idx, slot_idx] = findSlot(key);
    return chunk_idx != num_chunks_;
  }

  constexpr auto operator[](const Key& key) -> Value& {
    auto [chunk_idx, slot_idx, inserted] = insertSlot(key);
    return chunks_[chunk_idx].slots[slot_idx].data()->second;
  }

  // --------------------------------------------------------------------------
  // Insertion (with prefetching)
  // --------------------------------------------------------------------------

  constexpr auto insert(const Key& key, const Value& value)
      -> std::pair<Value*, bool> {
    auto [chunk_idx, slot_idx, inserted] = insertSlot(key);
    if (inserted) {
      chunks_[chunk_idx].slots[slot_idx].data()->second = value;
    }
    return {&chunks_[chunk_idx].slots[slot_idx].data()->second, inserted};
  }

  constexpr auto insert(const Key& key, Value&& value)
      -> std::pair<Value*, bool> {
    auto [chunk_idx, slot_idx, inserted] = insertSlot(key);
    if (inserted) {
      chunks_[chunk_idx].slots[slot_idx].data()->second = std::move(value);
    }
    return {&chunks_[chunk_idx].slots[slot_idx].data()->second, inserted};
  }

  constexpr auto insertOrAssign(const Key& key, Value value) -> bool {
    auto [chunk_idx, slot_idx, inserted] = insertSlot(key);
    chunks_[chunk_idx].slots[slot_idx].data()->second = std::move(value);
    return inserted;
  }

  // --------------------------------------------------------------------------
  // Deletion - Uses overflow counting, NO TOMBSTONES
  // --------------------------------------------------------------------------

  constexpr auto erase(const Key& key) -> bool {
    std::size_t hash = hasher_(key);
    std::uint8_t h2 = f14pf::h2(hash);
    F14PrefetchProbeSeq seq{hash, num_chunks_ - 1};

    while (true) {
      // Prefetch next chunk
      size_type next_chunk = seq.nextOffset();
      _mm_prefetch(reinterpret_cast<const char*>(&chunks_[next_chunk]), _MM_HINT_T0);

      size_type chunk_idx = seq.offset();
      Chunk& chunk = chunks_[chunk_idx];

      F14PrefetchBitMask matches{chunk.match(h2)};
      while (matches) {
        size_type slot_idx = *matches;
        if (equal_(chunk.slots[slot_idx].data()->first, key)) {
          chunk.slots[slot_idx].destroy();
          chunk.tags[slot_idx] = f14pf::kEmpty;
          chunk.decHosted();
          --size_;

          // Decrement overflow for chunks we probed past
          F14PrefetchProbeSeq cleanup{hash, num_chunks_ - 1};
          while (cleanup.offset() != chunk_idx) {
            chunks_[cleanup.offset()].decOverflow();
            cleanup.next();
          }

          return true;
        }
        ++matches;
      }

      if (chunk.matchEmpty() != 0 || chunk.overflow() == 0) {
        return false;
      }

      seq.next();
    }
  }

  constexpr void clear() {
    for (size_type c = 0; c < num_chunks_; ++c) {
      for (size_type i = 0; i < Chunk::kCapacity; ++i) {
        if (isOccupied(chunks_[c].tags[i])) {
          chunks_[c].slots[i].destroy();
        }
      }
    }
    size_ = 0;
    resetAllChunks();
  }

  // --------------------------------------------------------------------------
  // Iterator Support
  // --------------------------------------------------------------------------

  class Iterator {
   public:
    using difference_type = std::ptrdiff_t;
    using value_type = std::pair<const Key, Value>;
    using pointer = value_type*;
    using reference = value_type&;

    constexpr Iterator(F14Prefetch* map, size_type chunk_idx, size_type slot_idx)
        : map_{map}, chunk_idx_{chunk_idx}, slot_idx_{slot_idx} {
      advanceToOccupied();
    }

    constexpr auto operator*() const -> reference {
      return *map_->chunks_[chunk_idx_].slots[slot_idx_].data();
    }

    constexpr auto operator->() const -> pointer {
      return map_->chunks_[chunk_idx_].slots[slot_idx_].data();
    }

    constexpr auto operator++() -> Iterator& {
      ++slot_idx_;
      advanceToOccupied();
      return *this;
    }

    constexpr auto operator++(int) -> Iterator {
      Iterator tmp = *this;
      ++*this;
      return tmp;
    }

    constexpr auto operator==(const Iterator& other) const -> bool {
      return chunk_idx_ == other.chunk_idx_ && slot_idx_ == other.slot_idx_;
    }

   private:
    constexpr void advanceToOccupied() {
      while (chunk_idx_ < map_->num_chunks_) {
        while (slot_idx_ < Chunk::kCapacity) {
          if (isOccupied(map_->chunks_[chunk_idx_].tags[slot_idx_])) {
            return;
          }
          ++slot_idx_;
        }
        ++chunk_idx_;
        slot_idx_ = 0;
      }
    }

    F14Prefetch* map_;
    size_type chunk_idx_;
    size_type slot_idx_;
  };

  class ConstIterator {
   public:
    using difference_type = std::ptrdiff_t;
    using value_type = std::pair<const Key, Value>;
    using pointer = const value_type*;
    using reference = const value_type&;

    constexpr ConstIterator(const F14Prefetch* map, size_type chunk_idx,
                            size_type slot_idx)
        : map_{map}, chunk_idx_{chunk_idx}, slot_idx_{slot_idx} {
      advanceToOccupied();
    }

    constexpr auto operator*() const -> reference {
      return *map_->chunks_[chunk_idx_].slots[slot_idx_].data();
    }

    constexpr auto operator->() const -> pointer {
      return map_->chunks_[chunk_idx_].slots[slot_idx_].data();
    }

    constexpr auto operator++() -> ConstIterator& {
      ++slot_idx_;
      advanceToOccupied();
      return *this;
    }

    constexpr auto operator++(int) -> ConstIterator {
      ConstIterator tmp = *this;
      ++*this;
      return tmp;
    }

    constexpr auto operator==(const ConstIterator& other) const -> bool {
      return chunk_idx_ == other.chunk_idx_ && slot_idx_ == other.slot_idx_;
    }

   private:
    constexpr void advanceToOccupied() {
      while (chunk_idx_ < map_->num_chunks_) {
        while (slot_idx_ < Chunk::kCapacity) {
          if (isOccupied(map_->chunks_[chunk_idx_].tags[slot_idx_])) {
            return;
          }
          ++slot_idx_;
        }
        ++chunk_idx_;
        slot_idx_ = 0;
      }
    }

    const F14Prefetch* map_;
    size_type chunk_idx_;
    size_type slot_idx_;
  };

  constexpr auto begin() -> Iterator { return {this, 0, 0}; }
  constexpr auto end() -> Iterator { return {this, num_chunks_, 0}; }
  constexpr auto begin() const -> ConstIterator { return {this, 0, 0}; }
  constexpr auto end() const -> ConstIterator { return {this, num_chunks_, 0}; }

 private:
  // --------------------------------------------------------------------------
  // Internal Helpers
  // --------------------------------------------------------------------------

  static constexpr auto isOccupied(std::uint8_t tag) -> bool {
    return (tag & 0x80) == 0;
  }

  constexpr auto needsResize() const -> bool {
    return size_ * kLoadFactorDenominator >=
           capacity() * kLoadFactorNumerator;
  }

  static constexpr auto normalizeNumChunks(size_type num) -> size_type {
    if (num <= 1) return 1;
    return size_type{1} << (64 - std::countl_zero(num - 1));
  }

  constexpr void resetAllChunks() {
    for (size_type c = 0; c < num_chunks_; ++c) {
      chunks_[c].reset();
    }
  }

  // --------------------------------------------------------------------------
  // findSlot with prefetching and overflow-based early termination
  // --------------------------------------------------------------------------

  constexpr auto findSlot(const Key& key) const
      -> std::pair<size_type, size_type> {
    std::size_t hash = hasher_(key);
    std::uint8_t h2 = f14pf::h2(hash);
    F14PrefetchProbeSeq seq{hash, num_chunks_ - 1};

    while (true) {
      // Prefetch next chunk into L1 cache
      size_type next_chunk = seq.nextOffset();
      _mm_prefetch(reinterpret_cast<const char*>(&chunks_[next_chunk]), _MM_HINT_T0);

      size_type chunk_idx = seq.offset();
      const Chunk& chunk = chunks_[chunk_idx];

      F14PrefetchBitMask matches{chunk.match(h2)};
      while (matches) {
        size_type slot_idx = *matches;
        if (equal_(chunk.slots[slot_idx].data()->first, key)) {
          return {chunk_idx, slot_idx};
        }
        ++matches;
      }

      // Early termination
      if (chunk.matchEmpty() != 0) {
        return {num_chunks_, 0};
      }
      if (chunk.overflow() == 0) {
        return {num_chunks_, 0};
      }

      seq.next();
    }
  }

  // --------------------------------------------------------------------------
  // insertSlot with prefetching and overflow counting
  // --------------------------------------------------------------------------

  constexpr auto insertSlot(const Key& key)
      -> std::tuple<size_type, size_type, bool> {
    if (needsResize()) {
      resize(num_chunks_ * 2);
    }

    std::size_t hash = hasher_(key);
    std::uint8_t h2 = f14pf::h2(hash);
    F14PrefetchProbeSeq seq{hash, num_chunks_ - 1};

    while (true) {
      // Prefetch next chunk
      size_type next_chunk = seq.nextOffset();
      _mm_prefetch(reinterpret_cast<const char*>(&chunks_[next_chunk]), _MM_HINT_T0);

      size_type chunk_idx = seq.offset();
      Chunk& chunk = chunks_[chunk_idx];

      // Check for existing key
      F14PrefetchBitMask matches{chunk.match(h2)};
      while (matches) {
        size_type slot_idx = *matches;
        if (equal_(chunk.slots[slot_idx].data()->first, key)) {
          return {chunk_idx, slot_idx, false};
        }
        ++matches;
      }

      // Find empty slot
      F14PrefetchBitMask empty{chunk.matchEmpty()};
      if (empty) {
        size_type slot_idx = *empty;

        chunk.tags[slot_idx] = h2;
        chunk.slots[slot_idx].construct(key, Value{});
        chunk.incHosted();
        ++size_;

        // Increment overflow for chunks we probed past
        F14PrefetchProbeSeq overflow_seq{hash, num_chunks_ - 1};
        while (overflow_seq.offset() != chunk_idx) {
          chunks_[overflow_seq.offset()].incOverflow();
          overflow_seq.next();
        }

        return {chunk_idx, slot_idx, true};
      }

      seq.next();
    }
  }

  constexpr void resize(size_type new_num_chunks) {
    new_num_chunks = normalizeNumChunks(std::max(new_num_chunks, kMinChunks));

    auto old_chunks = std::move(chunks_);
    size_type old_num_chunks = num_chunks_;

    num_chunks_ = new_num_chunks;
    chunks_ = std::make_unique<Chunk[]>(num_chunks_);
    size_ = 0;
    resetAllChunks();

    for (size_type c = 0; c < old_num_chunks; ++c) {
      for (size_type i = 0; i < Chunk::kCapacity; ++i) {
        if (isOccupied(old_chunks[c].tags[i])) {
          auto& [key, value] = *old_chunks[c].slots[i].mutableData();
          auto [chunk_idx, slot_idx, inserted] = insertSlot(std::move(key));
          chunks_[chunk_idx].slots[slot_idx].data()->second = std::move(value);
        }
      }
    }

    for (size_type c = 0; c < old_num_chunks; ++c) {
      for (size_type i = 0; i < Chunk::kCapacity; ++i) {
        if (isOccupied(old_chunks[c].tags[i])) {
          old_chunks[c].slots[i].destroy();
        }
      }
    }
  }

  // --------------------------------------------------------------------------
  // Member Variables
  // --------------------------------------------------------------------------

  size_type num_chunks_ = kMinChunks;
  size_type size_ = 0;

  std::unique_ptr<Chunk[]> chunks_ = nullptr;

  [[no_unique_address]] Hash hasher_{};
  [[no_unique_address]] KeyEqual equal_{};
};

}  // namespace tempura
