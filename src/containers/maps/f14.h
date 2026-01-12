#pragma once

// ============================================================================
// F14 Hash Map - Facebook's Cache-Optimized Design
// ============================================================================
//
// F14 (named for its 14 slots per chunk) was developed by Facebook/Meta.
// Key innovations:
//   1. 14 slots per chunk with colocated metadata
//   2. OVERFLOW COUNTING instead of tombstones
//   3. Self-cleaning on erase - no tombstone accumulation
//
//
// ┌─────────────────────────────────────────────────────────────────────────┐
// │  CHUNK STRUCTURE (16 bytes metadata + slots)                            │
// └─────────────────────────────────────────────────────────────────────────┘
//
//   Byte:  0  1  2  3  4  5  6  7  8  9 10 11 12 13 | 14      | 15
//         [T₀|T₁|T₂|T₃|T₄|T₅|T₆|T₇|T₈|T₉|T₁₀|T₁₁|T₁₂|T₁₃|overflow|control]
//          └─────────── 14 tags (7-bit H2) ──────────┘
//
//   Tags: 0x00-0x7F = occupied (H2 hash), 0xFF = empty
//   Overflow: Count of keys that probed PAST this chunk
//   Control: Hosted count / other metadata
//
//
// ┌─────────────────────────────────────────────────────────────────────────┐
// │  OVERFLOW COUNTING - The Key F14 Innovation                             │
// └─────────────────────────────────────────────────────────────────────────┘
//
// Problem with tombstones: They accumulate over time, slowing lookups.
// Rehashing clears them but is expensive.
//
// F14's solution: OVERFLOW COUNTING
//
// Insert(key) at chunk C₀:
//   - If C₀ is full, increment C₀.overflow and probe to C₁
//   - If C₁ is full, increment C₁.overflow and probe to C₂
//   - When we find a slot, we've incremented overflow for all passed chunks
//
// Find(key) at chunk C₀:
//   - If no H2 match AND C₀.overflow == 0 → key not in table (EARLY EXIT!)
//   - Otherwise, keep probing
//
// Erase(key) found at chunk Cₙ (started at C₀):
//   - Decrement overflow for C₀, C₁, ..., Cₙ₋₁
//   - Mark slot as empty (not deleted!)
//   - No tombstones accumulate!
//
// This is SELF-CLEANING: erases undo the overflow increments from inserts.
//
//
// ┌─────────────────────────────────────────────────────────────────────────┐
// │  WHY 14 SLOTS?                                                          │
// └─────────────────────────────────────────────────────────────────────────┘
//
// SSE2 processes 16 bytes at once. With 14 tags + 2 metadata bytes:
//   - Load 16 bytes, SIMD match against H2
//   - Mask off bits 14-15 (metadata, not tags)
//   - Check remaining bits for matches
//
// 14 slots at 12/14 load factor = 85.7% utilization, similar to Swiss Tables.
//
//
// ┌─────────────────────────────────────────────────────────────────────────┐
// │  PERFORMANCE                                                            │
// └─────────────────────────────────────────────────────────────────────────┘
//
// At max load factor (12/14 ≈ 85.7%):
//   - Successful lookup: avg 1.04 probes
//   - Failed lookup: avg 1.275 probes, P99 = 4
//   - <1% of keys require searching beyond 3 chunks
//
// No tombstone cleanup needed - performance stays consistent over time.
//
//
// ┌─────────────────────────────────────────────────────────────────────────┐
// │  MISSING FEATURES (vs Folly F14)                                        │
// └─────────────────────────────────────────────────────────────────────────┘
//
// - Heterogeneous lookup: find/contains/erase with transparent comparators
//   (e.g., find string_view in map<string, V> without allocation)
//
// - Chunk occupancy tracking: Folly tracks which chunk slots are occupied
//   in a bitmask for faster iteration
//
// - Reserve/rehash: explicit capacity management
//
// IMPLEMENTED:
// - Prehash/HashToken API: prehash(key) returns token, find(token, key)
//
//
// ┌─────────────────────────────────────────────────────────────────────────┐
// │  REFERENCES                                                             │
// └─────────────────────────────────────────────────────────────────────────┘
//
// Facebook F14 announcement:
//   https://engineering.fb.com/2019/04/25/developer-tools/f14/
//
// Folly F14 implementation:
//   https://github.com/facebook/folly/blob/main/folly/container/F14.md
//
// CppCon 2019 - Nathan Bronson "F14: A High-Performance Hash Table"
//
// ============================================================================

#include <bit>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <memory>
#include <utility>

#include "containers/maps/hasher.h"
#include "containers/maps/map_storage.h"
#include "meta/manual_lifetime.h"
#include "simd/simd.h"

namespace tempura {

namespace f14 {

// Tag encoding: 0x00-0x7F = occupied (H2 hash), 0xFF = empty
inline constexpr std::uint8_t kEmpty = 0xFF;

// H2 hash: lower 7 bits of hash (range [0, 127])
constexpr auto h2(std::size_t hash) -> std::uint8_t {
  return static_cast<std::uint8_t>(hash & 0x7F);
}

}  // namespace f14

// ============================================================================
// F14Chunk - 14 Slots with Overflow Counting
// ============================================================================

template <typename Key, typename Value>
struct alignas(64) F14Chunk {
  // 14 slots - the "14" in F14
  static constexpr std::size_t kCapacity = 14;
  static constexpr std::size_t kTagBytes = 16;  // For SIMD alignment

  // Metadata layout: [14 tags][overflow count][hosted count]
  static constexpr std::size_t kOverflowIdx = 14;
  static constexpr std::size_t kHostedIdx = 15;

  // Mask for valid tag positions (bits 0-13)
  static constexpr std::uint16_t kSlotMask = 0x3FFF;

  std::uint8_t tags[kTagBytes];  // 14 tags + overflow + hosted

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

  // SIMD match - returns bitmask of matching tag positions (masked to 14 bits)
  auto match(std::uint8_t tag) const -> std::uint16_t {
    auto v = Vec16i8::load(tags);
    return v.match(tag) & kSlotMask;
  }

  // Match empty slots (0xFF has high bit set)
  auto matchEmpty() const -> std::uint16_t {
    auto v = Vec16i8::load(tags);
    return v.matchHighBitSet() & kSlotMask;
  }

  auto overflow() const -> std::uint8_t { return tags[kOverflowIdx]; }
  void setOverflow(std::uint8_t val) { tags[kOverflowIdx] = val; }

  // Saturate at 255 to prevent wrap-around. Once saturated, overflow stays
  // permanent - this is rare (requires 255+ keys probing past one chunk)
  // but prevents correctness bugs from counter overflow.
  void incOverflow() {
    if (tags[kOverflowIdx] < 255) ++tags[kOverflowIdx];
  }
  void decOverflow() {
    if (tags[kOverflowIdx] < 255) --tags[kOverflowIdx];
  }

  auto hostedCount() const -> std::uint8_t { return tags[kHostedIdx]; }
  void setHostedCount(std::uint8_t val) { tags[kHostedIdx] = val; }
  void incHosted() { ++tags[kHostedIdx]; }
  void decHosted() { --tags[kHostedIdx]; }

  void reset() {
    std::memset(tags, f14::kEmpty, kCapacity);
    tags[kOverflowIdx] = 0;
    tags[kHostedIdx] = 0;
  }

  auto isFull() const -> bool {
    return hostedCount() >= kCapacity;
  }
};

// ============================================================================
// BitMask Iterator
// ============================================================================

class F14BitMask {
 public:
  explicit constexpr F14BitMask(std::uint16_t mask) : mask_{mask} {}

  constexpr auto operator*() const -> std::size_t {
    return std::countr_zero(mask_);
  }

  constexpr auto operator++() -> F14BitMask& {
    mask_ &= (mask_ - 1);
    return *this;
  }

  constexpr explicit operator bool() const { return mask_ != 0; }

 private:
  std::uint16_t mask_;
};

// ============================================================================
// F14 Probe Sequence (Quadratic)
// ============================================================================

class F14ProbeSeq {
 public:
  constexpr F14ProbeSeq(std::size_t hash, std::size_t mask)
      : mask_{mask}, offset_{hash & mask_} {}

  constexpr auto offset() const -> std::size_t { return offset_; }
  constexpr auto index() const -> std::size_t { return index_; }

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
// F14Map
// ============================================================================

template <typename Key, typename Value, Hasher<Key> Hash = std::hash<Key>,
          typename KeyEqual = std::equal_to<Key>>
class F14Map {
 public:
  using key_type = Key;
  using mapped_type = Value;
  using value_type = std::pair<const Key, Value>;
  using size_type = std::size_t;
  using hasher = Hash;
  using key_equal = KeyEqual;

  using Chunk = F14Chunk<Key, Value>;
  using Slot = typename Chunk::Slot;

  // Load factor 12/14 ≈ 85.7% (per Folly's design)
  static constexpr size_type kLoadFactorNumerator = 12;
  static constexpr size_type kLoadFactorDenominator = 14;
  static constexpr size_type kMinChunks = 4;

  // --------------------------------------------------------------------------
  // Constructors
  // --------------------------------------------------------------------------

  constexpr F14Map() : F14Map{kMinChunks * Chunk::kCapacity} {}

  constexpr explicit F14Map(size_type initial_capacity)
      : num_chunks_{normalizeNumChunks(
            (std::max(initial_capacity, kMinChunks * Chunk::kCapacity) +
             Chunk::kCapacity - 1) / Chunk::kCapacity)},
        chunks_{std::make_unique<Chunk[]>(num_chunks_)} {
    resetAllChunks();
  }

  constexpr F14Map(const F14Map& other)
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

  constexpr F14Map(F14Map&& other) noexcept
      : num_chunks_{other.num_chunks_},
        size_{other.size_},
        chunks_{std::move(other.chunks_)} {
    other.num_chunks_ = 0;
    other.size_ = 0;
  }

  constexpr auto operator=(const F14Map& other) -> F14Map& {
    if (this != &other) {
      F14Map tmp{other};
      swap(tmp);
    }
    return *this;
  }

  constexpr auto operator=(F14Map&& other) noexcept -> F14Map& {
    if (this != &other) {
      F14Map tmp{std::move(other)};
      swap(tmp);
    }
    return *this;
  }

  constexpr ~F14Map() {
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

  constexpr void swap(F14Map& other) noexcept {
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
  // Prehash API
  // --------------------------------------------------------------------------
  // Compute hash once, reuse for multiple lookups. Useful when probing
  // multiple tables with the same key, or doing find-then-insert patterns.

  struct HashToken {
    std::size_t hash;
    std::uint8_t h2;
  };

  constexpr auto prehash(const Key& key) const -> HashToken {
    std::size_t hash = hasher_(key);
    return HashToken{hash, f14::h2(hash)};
  }

  // --------------------------------------------------------------------------
  // Lookup
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

  // Lookup with precomputed hash token
  constexpr auto find(HashToken token, const Key& key) -> Value* {
    auto [chunk_idx, slot_idx] = findSlotWithHash(token.hash, token.h2, key);
    if (chunk_idx == num_chunks_) return nullptr;
    return &chunks_[chunk_idx].slots[slot_idx].data()->second;
  }

  constexpr auto find(HashToken token, const Key& key) const -> const Value* {
    auto [chunk_idx, slot_idx] = findSlotWithHash(token.hash, token.h2, key);
    if (chunk_idx == num_chunks_) return nullptr;
    return &chunks_[chunk_idx].slots[slot_idx].data()->second;
  }

  constexpr auto contains(const Key& key) const -> bool {
    auto [chunk_idx, slot_idx] = findSlot(key);
    return chunk_idx != num_chunks_;
  }

  constexpr auto contains(HashToken token, const Key& key) const -> bool {
    auto [chunk_idx, slot_idx] = findSlotWithHash(token.hash, token.h2, key);
    return chunk_idx != num_chunks_;
  }

  constexpr auto operator[](const Key& key) -> Value& {
    auto [chunk_idx, slot_idx, inserted] = insertSlot(key);
    return chunks_[chunk_idx].slots[slot_idx].data()->second;
  }

  // --------------------------------------------------------------------------
  // Insertion
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
    std::uint8_t h2 = f14::h2(hash);
    F14ProbeSeq seq{hash, num_chunks_ - 1};

    while (true) {
      size_type chunk_idx = seq.offset();
      Chunk& chunk = chunks_[chunk_idx];

      F14BitMask matches{chunk.match(h2)};
      while (matches) {
        size_type slot_idx = *matches;
        if (equal_(chunk.slots[slot_idx].data()->first, key)) {
          // Found it! Destroy and mark empty
          chunk.slots[slot_idx].destroy();
          chunk.tags[slot_idx] = f14::kEmpty;
          chunk.decHosted();
          --size_;

          // Decrement overflow counts for all chunks we probed past
          // This is the self-cleaning magic of F14!
          F14ProbeSeq cleanup{hash, num_chunks_ - 1};
          while (cleanup.offset() != chunk_idx) {
            chunks_[cleanup.offset()].decOverflow();
            cleanup.next();
          }

          return true;
        }
        ++matches;
      }

      // F14's key optimization: if no empty slots AND overflow == 0,
      // the key cannot be in the table (terminate early!)
      if (chunk.matchEmpty() != 0 || chunk.overflow() == 0) {
        // Either we found empty (key not here) or overflow is 0 (nothing probed past)
        if (chunk.matchEmpty() != 0) {
          return false;  // Definitely not in table
        }
        if (chunk.overflow() == 0) {
          return false;  // Nothing probed past, so key isn't further
        }
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

    constexpr Iterator(F14Map* map, size_type chunk_idx, size_type slot_idx)
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

    F14Map* map_;
    size_type chunk_idx_;
    size_type slot_idx_;
  };

  class ConstIterator {
   public:
    using difference_type = std::ptrdiff_t;
    using value_type = std::pair<const Key, Value>;
    using pointer = const value_type*;
    using reference = const value_type&;

    constexpr ConstIterator(const F14Map* map, size_type chunk_idx,
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

    const F14Map* map_;
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
    return (tag & 0x80) == 0;  // H2 values are [0, 127]
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
  // Find with overflow-based early termination
  // --------------------------------------------------------------------------

  constexpr auto findSlot(const Key& key) const
      -> std::pair<size_type, size_type> {
    std::size_t hash = hasher_(key);
    std::uint8_t h2 = f14::h2(hash);
    F14ProbeSeq seq{hash, num_chunks_ - 1};

    while (true) {
      size_type chunk_idx = seq.offset();
      const Chunk& chunk = chunks_[chunk_idx];

      F14BitMask matches{chunk.match(h2)};
      while (matches) {
        size_type slot_idx = *matches;
        if (equal_(chunk.slots[slot_idx].data()->first, key)) {
          return {chunk_idx, slot_idx};
        }
        ++matches;
      }

      // F14's key optimization: early termination
      // If chunk has empty slots, key would be here if it existed
      if (chunk.matchEmpty() != 0) {
        return {num_chunks_, 0};
      }
      // If overflow == 0, nothing probed past this chunk, so key isn't further
      if (chunk.overflow() == 0) {
        return {num_chunks_, 0};
      }

      seq.next();
    }
  }

  // Find with precomputed hash (for prehash API)
  constexpr auto findSlotWithHash(std::size_t hash, std::uint8_t h2,
                                   const Key& key) const
      -> std::pair<size_type, size_type> {
    F14ProbeSeq seq{hash, num_chunks_ - 1};

    while (true) {
      size_type chunk_idx = seq.offset();
      const Chunk& chunk = chunks_[chunk_idx];

      F14BitMask matches{chunk.match(h2)};
      while (matches) {
        size_type slot_idx = *matches;
        if (equal_(chunk.slots[slot_idx].data()->first, key)) {
          return {chunk_idx, slot_idx};
        }
        ++matches;
      }

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
  // Insert with overflow counting
  // --------------------------------------------------------------------------

  constexpr auto insertSlot(const Key& key)
      -> std::tuple<size_type, size_type, bool> {
    if (needsResize()) {
      resize(num_chunks_ * 2);
    }

    std::size_t hash = hasher_(key);
    std::uint8_t h2 = f14::h2(hash);
    F14ProbeSeq seq{hash, num_chunks_ - 1};

    // Track chunks we probe past (for overflow counting)
    size_type home_chunk = seq.offset();

    while (true) {
      size_type chunk_idx = seq.offset();
      Chunk& chunk = chunks_[chunk_idx];

      // Check for existing key
      F14BitMask matches{chunk.match(h2)};
      while (matches) {
        size_type slot_idx = *matches;
        if (equal_(chunk.slots[slot_idx].data()->first, key)) {
          return {chunk_idx, slot_idx, false};
        }
        ++matches;
      }

      // Find empty slot in this chunk
      F14BitMask empty{chunk.matchEmpty()};
      if (empty) {
        size_type slot_idx = *empty;

        // Insert here
        chunk.tags[slot_idx] = h2;
        chunk.slots[slot_idx].construct(key, Value{});
        chunk.incHosted();
        ++size_;

        // Increment overflow for all chunks we probed past
        F14ProbeSeq overflow_seq{hash, num_chunks_ - 1};
        while (overflow_seq.offset() != chunk_idx) {
          chunks_[overflow_seq.offset()].incOverflow();
          overflow_seq.next();
        }

        return {chunk_idx, slot_idx, true};
      }

      // Chunk is full, continue probing
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

    // Reinsert all elements
    for (size_type c = 0; c < old_num_chunks; ++c) {
      for (size_type i = 0; i < Chunk::kCapacity; ++i) {
        if (isOccupied(old_chunks[c].tags[i])) {
          auto& [key, value] = *old_chunks[c].slots[i].mutableData();
          auto [chunk_idx, slot_idx, inserted] = insertSlot(std::move(key));
          chunks_[chunk_idx].slots[slot_idx].data()->second = std::move(value);
        }
      }
    }

    // Clean up old slots
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
  // No deleted_ counter needed - F14 doesn't use tombstones!

  std::unique_ptr<Chunk[]> chunks_ = nullptr;

  [[no_unique_address]] Hash hasher_{};
  [[no_unique_address]] KeyEqual equal_{};
};

}  // namespace tempura
