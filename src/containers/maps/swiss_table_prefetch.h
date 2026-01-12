#pragma once

// ============================================================================
// Swiss Table with Software Prefetching - Hiding Memory Latency
// ============================================================================
//
// This variant extends the standard Swiss Table with explicit software
// prefetching to hide memory latency during probe sequences.
//
// Modern CPUs have ~200 cycle memory latency to DRAM. When probing multiple
// groups, each group access is a potential cache miss. By prefetching the
// NEXT group while processing the CURRENT group, we overlap computation with
// memory access, hiding latency.
//
// ┌─────────────────────────────────────────────────────────────────────────┐
// │  MEMORY LATENCY PROBLEM                                                 │
// └─────────────────────────────────────────────────────────────────────────┘
//
// Without prefetching (sequential access):
//
//   Time →
//   ├─────────┬─────────┬─────────┬─────────┐
//   │ Load G0 │ Load G1 │ Load G2 │ Load G3 │
//   │ (200c)  │ (200c)  │ (200c)  │ (200c)  │
//   └─────────┴─────────┴─────────┴─────────┘
//   Total: ~800 cycles for 4 groups
//
// With prefetching (overlapped access):
//
//   Time →
//   ├─────────┬─────────┬─────────┬─────────┐
//   │ Load G0 │ Proc G0 │ Proc G1 │ Proc G2 │  ← Computation
//   │ Pref G1 │ Load G1 │ Load G2 │ Load G3 │  ← Memory access
//   └─────────┴────┬────┴────┬────┴────┬────┘
//              overlap   overlap   overlap
//   Total: ~220 cycles for 4 groups (load G0 + 3*processing)
//
//
// ┌─────────────────────────────────────────────────────────────────────────┐
// │  PREFETCH STRATEGY                                                      │
// └─────────────────────────────────────────────────────────────────────────┘
//
// At each probe step:
//
//   1. Prefetch NEXT group's control bytes (_MM_HINT_T0 = L1 cache)
//   2. Prefetch NEXT group's first slot (likely accessed if H2 matches)
//   3. Process CURRENT group (SIMD match + key comparisons)
//   4. Advance to next probe position
//
// This overlaps memory latency with computation, improving performance when:
//   - Hash table is larger than L3 cache (>8MB on modern CPUs)
//   - Probe sequences are long (high load factor, many collisions)
//   - Keys are large or comparisons are expensive
//
//
// ┌─────────────────────────────────────────────────────────────────────────┐
// │  PREFETCH HINTS                                                         │
// └─────────────────────────────────────────────────────────────────────────┘
//
// _MM_HINT_T0: Prefetch to L1 cache (most aggressive, lowest latency)
//              Best for data accessed very soon (next iteration)
//
// _MM_HINT_T1: Prefetch to L2 cache (moderate)
// _MM_HINT_T2: Prefetch to L3 cache (least aggressive)
// _MM_HINT_NTA: Non-temporal (bypass cache, for streaming)
//
// We use T0 because we expect to access the next group within ~10-50 cycles.
//
//
// ┌─────────────────────────────────────────────────────────────────────────┐
// │  PERFORMANCE TRADEOFFS                                                  │
// └─────────────────────────────────────────────────────────────────────────┘
//
// Pros:
//   - 2-3x speedup on large tables (>L3 cache) with long probe sequences
//   - No algorithmic changes - drop-in replacement for SwissTable
//   - Hardware prefetching alone is often insufficient for pointer-chasing
//
// Cons:
//   - Prefetch instructions have small overhead (~1-2 cycles each)
//   - Can pollute cache if prefetch is never used (mispredicted probes)
//   - No benefit for small tables (everything in L1/L2)
//   - May slightly hurt performance on very small tables (<1KB)
//
// Recommendation: Use for hash tables expected to exceed L3 cache size.
//
// ============================================================================

#include <bit>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <immintrin.h>  // _mm_prefetch
#include <memory>
#include <utility>

#include "containers/maps/hasher.h"
#include "containers/maps/map_storage.h"
#include "meta/manual_lifetime.h"
#include "simd/simd.h"

namespace tempura {

// ============================================================================
// Control Byte Constants (same as SwissTable)
// ============================================================================

inline constexpr std::uint8_t kEmptyPrefetch = 0b1111'1111;     // 0xFF
inline constexpr std::uint8_t kDeletedPrefetch = 0b1111'1110;   // 0xFE
inline constexpr std::uint8_t kSentinelPrefetch = 0b1000'0000;  // 0x80

constexpr auto h2Prefetch(std::size_t hash) -> std::uint8_t {
  return static_cast<std::uint8_t>(hash & 0x7F);
}

// ============================================================================
// Group (same as SwissTable)
// ============================================================================

class alignas(16) GroupPrefetch {
 public:
  static constexpr std::size_t kWidth = 16;

  constexpr GroupPrefetch() = default;

  static auto load(const std::uint8_t* ctrl) -> GroupPrefetch {
    GroupPrefetch g;
    g.data_ = Vec16i8::load(ctrl);
    return g;
  }

  auto match(std::uint8_t byte) const -> std::uint16_t {
    return data_.match(byte);
  }

  auto matchEmpty() const -> std::uint16_t {
    return data_.matchHighBitSet();
  }

  auto matchEmptyOrDeleted() const -> std::uint16_t {
    return matchEmpty();
  }

  auto countLeadingEmpty() const -> std::size_t {
    std::uint16_t empty = matchEmpty();
    if (empty == 0xFFFF) return kWidth;
    return std::countr_one(empty);
  }

 private:
  Vec16i8 data_{};
};

// ============================================================================
// BitMask (same as SwissTable)
// ============================================================================

class BitMaskPrefetch {
 public:
  explicit constexpr BitMaskPrefetch(std::uint16_t mask) : mask_{mask} {}

  constexpr auto operator*() const -> std::size_t {
    return std::countr_zero(mask_);
  }

  constexpr auto operator++() -> BitMaskPrefetch& {
    mask_ &= (mask_ - 1);
    return *this;
  }

  constexpr explicit operator bool() const { return mask_ != 0; }

 private:
  std::uint16_t mask_;
};

// ============================================================================
// ProbeSeqPrefetch - Extended with nextOffset() for prefetching
// ============================================================================

class ProbeSeqPrefetch {
 public:
  constexpr ProbeSeqPrefetch(std::size_t hash, std::size_t mask)
      : mask_{mask}, offset_{hash & mask_} {}

  constexpr auto offset() const -> std::size_t { return offset_; }

  constexpr auto offset(std::size_t i) const -> std::size_t {
    return (offset_ + i) & mask_;
  }

  // Return the next probe offset WITHOUT advancing the sequence
  // Used for prefetching the next group while processing current group
  constexpr auto nextOffset() const -> std::size_t {
    std::size_t next_index = index_ + GroupPrefetch::kWidth;
    return (offset_ + next_index) & mask_;
  }

  constexpr void next() {
    index_ += GroupPrefetch::kWidth;
    offset_ += index_;
    offset_ &= mask_;
  }

 private:
  std::size_t mask_;
  std::size_t offset_;
  std::size_t index_ = 0;
};

// ============================================================================
// SwissTablePrefetch
// ============================================================================

template <typename Key, typename Value, Hasher<Key> Hash = std::hash<Key>,
          typename KeyEqual = std::equal_to<Key>>
class SwissTablePrefetch {
 public:
  using key_type = Key;
  using mapped_type = Value;
  using value_type = std::pair<const Key, Value>;
  using size_type = std::size_t;
  using hasher = Hash;
  using key_equal = KeyEqual;

  // --------------------------------------------------------------------------
  // Slot Structure
  // --------------------------------------------------------------------------

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

  // --------------------------------------------------------------------------
  // Constants
  // --------------------------------------------------------------------------

  static constexpr size_type kLoadFactorNumerator = 7;
  static constexpr size_type kLoadFactorDenominator = 8;
  static constexpr size_type kMinCapacity = 16;

  // --------------------------------------------------------------------------
  // Constructors
  // --------------------------------------------------------------------------

  constexpr SwissTablePrefetch() : SwissTablePrefetch{kMinCapacity} {}

  constexpr explicit SwissTablePrefetch(size_type initial_capacity)
      : capacity_{normalizeCapacity(std::max(initial_capacity, kMinCapacity))},
        ctrl_{allocateControls(capacity_)},
        slots_{std::make_unique<Slot[]>(capacity_)} {
    resetControls();
  }

  constexpr SwissTablePrefetch(const SwissTablePrefetch& other)
      : capacity_{other.capacity_},
        size_{other.size_},
        deleted_{other.deleted_},
        ctrl_{allocateControls(capacity_)},
        slots_{std::make_unique<Slot[]>(capacity_)} {
    std::memcpy(ctrl_.get(), other.ctrl_.get(), capacity_ + GroupPrefetch::kWidth);
    for (size_type i = 0; i < capacity_; ++i) {
      if (isOccupied(ctrl_[i])) {
        slots_[i].construct(*other.slots_[i].data());
      }
    }
  }

  constexpr SwissTablePrefetch(SwissTablePrefetch&& other) noexcept
      : capacity_{other.capacity_},
        size_{other.size_},
        deleted_{other.deleted_},
        ctrl_{std::move(other.ctrl_)},
        slots_{std::move(other.slots_)} {
    other.capacity_ = 0;
    other.size_ = 0;
    other.deleted_ = 0;
  }

  constexpr auto operator=(const SwissTablePrefetch& other) -> SwissTablePrefetch& {
    if (this != &other) {
      SwissTablePrefetch tmp{other};
      swap(tmp);
    }
    return *this;
  }

  constexpr auto operator=(SwissTablePrefetch&& other) noexcept -> SwissTablePrefetch& {
    if (this != &other) {
      SwissTablePrefetch tmp{std::move(other)};
      swap(tmp);
    }
    return *this;
  }

  constexpr ~SwissTablePrefetch() {
    if (slots_) {
      for (size_type i = 0; i < capacity_; ++i) {
        if (isOccupied(ctrl_[i])) {
          slots_[i].destroy();
        }
      }
    }
  }

  constexpr void swap(SwissTablePrefetch& other) noexcept {
    std::swap(capacity_, other.capacity_);
    std::swap(size_, other.size_);
    std::swap(deleted_, other.deleted_);
    std::swap(ctrl_, other.ctrl_);
    std::swap(slots_, other.slots_);
    std::swap(hasher_, other.hasher_);
    std::swap(equal_, other.equal_);
  }

  // --------------------------------------------------------------------------
  // Capacity
  // --------------------------------------------------------------------------

  constexpr auto size() const noexcept -> size_type { return size_; }
  constexpr auto capacity() const noexcept -> size_type { return capacity_; }
  constexpr auto empty() const noexcept -> bool { return size_ == 0; }

  constexpr auto loadFactor() const noexcept -> double {
    return static_cast<double>(size_) / static_cast<double>(capacity_);
  }

  // --------------------------------------------------------------------------
  // Lookup (with prefetching)
  // --------------------------------------------------------------------------

  constexpr auto find(const Key& key) -> Value* {
    size_type idx = findSlot(key);
    if (idx == capacity_) {
      return nullptr;
    }
    return &slots_[idx].data()->second;
  }

  constexpr auto find(const Key& key) const -> const Value* {
    size_type idx = findSlot(key);
    if (idx == capacity_) {
      return nullptr;
    }
    return &slots_[idx].data()->second;
  }

  constexpr auto contains(const Key& key) const -> bool {
    return findSlot(key) != capacity_;
  }

  constexpr auto operator[](const Key& key) -> Value& {
    auto [idx, inserted] = insertSlot(key);
    return slots_[idx].data()->second;
  }

  // --------------------------------------------------------------------------
  // Insertion (with prefetching)
  // --------------------------------------------------------------------------

  constexpr auto insert(const Key& key, const Value& value)
      -> std::pair<Value*, bool> {
    auto [idx, inserted] = insertSlot(key);
    if (inserted) {
      slots_[idx].data()->second = value;
    }
    return {&slots_[idx].data()->second, inserted};
  }

  constexpr auto insert(const Key& key, Value&& value)
      -> std::pair<Value*, bool> {
    auto [idx, inserted] = insertSlot(key);
    if (inserted) {
      slots_[idx].data()->second = std::move(value);
    }
    return {&slots_[idx].data()->second, inserted};
  }

  constexpr auto insertOrAssign(const Key& key, Value value) -> bool {
    auto [idx, inserted] = insertSlot(key);
    slots_[idx].data()->second = std::move(value);
    return inserted;
  }

  // --------------------------------------------------------------------------
  // Deletion
  // --------------------------------------------------------------------------

  constexpr auto erase(const Key& key) -> bool {
    size_type idx = findSlot(key);
    if (idx == capacity_) {
      return false;
    }

    slots_[idx].destroy();
    ctrl_[idx] = kDeletedPrefetch;
    --size_;
    ++deleted_;

    return true;
  }

  constexpr void clear() {
    for (size_type i = 0; i < capacity_; ++i) {
      if (isOccupied(ctrl_[i])) {
        slots_[i].destroy();
      }
    }
    size_ = 0;
    deleted_ = 0;
    resetControls();
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

    constexpr Iterator(SwissTablePrefetch* map, size_type idx)
        : map_{map}, idx_{idx} {
      advanceToOccupied();
    }

    constexpr auto operator*() const -> reference {
      return *map_->slots_[idx_].data();
    }

    constexpr auto operator->() const -> pointer {
      return map_->slots_[idx_].data();
    }

    constexpr auto operator++() -> Iterator& {
      ++idx_;
      advanceToOccupied();
      return *this;
    }

    constexpr auto operator++(int) -> Iterator {
      Iterator tmp = *this;
      ++*this;
      return tmp;
    }

    constexpr auto operator==(const Iterator& other) const -> bool {
      return idx_ == other.idx_;
    }

   private:
    constexpr void advanceToOccupied() {
      while (idx_ < map_->capacity_ && !isOccupied(map_->ctrl_[idx_])) {
        ++idx_;
      }
    }

    SwissTablePrefetch* map_;
    size_type idx_;
  };

  class ConstIterator {
   public:
    using difference_type = std::ptrdiff_t;
    using value_type = std::pair<const Key, Value>;
    using pointer = const value_type*;
    using reference = const value_type&;

    constexpr ConstIterator(const SwissTablePrefetch* map, size_type idx)
        : map_{map}, idx_{idx} {
      advanceToOccupied();
    }

    constexpr auto operator*() const -> reference {
      return *map_->slots_[idx_].data();
    }

    constexpr auto operator->() const -> pointer {
      return map_->slots_[idx_].data();
    }

    constexpr auto operator++() -> ConstIterator& {
      ++idx_;
      advanceToOccupied();
      return *this;
    }

    constexpr auto operator++(int) -> ConstIterator {
      ConstIterator tmp = *this;
      ++*this;
      return tmp;
    }

    constexpr auto operator==(const ConstIterator& other) const -> bool {
      return idx_ == other.idx_;
    }

   private:
    constexpr void advanceToOccupied() {
      while (idx_ < map_->capacity_ && !isOccupied(map_->ctrl_[idx_])) {
        ++idx_;
      }
    }

    const SwissTablePrefetch* map_;
    size_type idx_;
  };

  constexpr auto begin() -> Iterator { return {this, 0}; }
  constexpr auto end() -> Iterator { return {this, capacity_}; }
  constexpr auto begin() const -> ConstIterator { return {this, 0}; }
  constexpr auto end() const -> ConstIterator { return {this, capacity_}; }

 private:
  // --------------------------------------------------------------------------
  // Internal Helpers
  // --------------------------------------------------------------------------

  static constexpr auto isOccupied(std::uint8_t ctrl) -> bool {
    return (ctrl & 0x80) == 0;
  }

  constexpr auto needsResize() const -> bool {
    return (size_ + deleted_) * kLoadFactorDenominator >=
           capacity_ * kLoadFactorNumerator;
  }

  static constexpr auto normalizeCapacity(size_type cap) -> size_type {
    if (cap <= GroupPrefetch::kWidth) return GroupPrefetch::kWidth;
    return size_type{1} << (64 - std::countl_zero(cap - 1));
  }

  static auto allocateControls(size_type capacity)
      -> std::unique_ptr<std::uint8_t[]> {
    return std::make_unique<std::uint8_t[]>(capacity + GroupPrefetch::kWidth);
  }

  constexpr void resetControls() {
    std::memset(ctrl_.get(), kEmptyPrefetch, capacity_ + GroupPrefetch::kWidth);
  }

  // --------------------------------------------------------------------------
  // findSlot with prefetching
  // --------------------------------------------------------------------------

  constexpr auto findSlot(const Key& key) const -> size_type {
    std::size_t hash = hasher_(key);
    std::uint8_t h2_byte = h2Prefetch(hash);
    ProbeSeqPrefetch seq{hash, capacity_ - 1};

    while (true) {
      // Prefetch NEXT group's control bytes into L1 cache
      size_type next_off = seq.nextOffset();
      _mm_prefetch(reinterpret_cast<const char*>(&ctrl_[next_off]), _MM_HINT_T0);

      // Prefetch NEXT group's first slot (likely accessed if H2 matches)
      _mm_prefetch(reinterpret_cast<const char*>(&slots_[next_off]), _MM_HINT_T0);

      // Process CURRENT group
      GroupPrefetch g = GroupPrefetch::load(&ctrl_[seq.offset()]);
      BitMaskPrefetch matches{g.match(h2_byte)};

      while (matches) {
        size_type idx = seq.offset(*matches);
        if (equal_(slots_[idx].data()->first, key)) {
          return idx;
        }
        ++matches;
      }

      if (g.matchEmpty()) {
        return capacity_;
      }

      seq.next();
    }
  }

  // --------------------------------------------------------------------------
  // insertSlot with prefetching
  // --------------------------------------------------------------------------

  constexpr auto insertSlot(const Key& key) -> std::pair<size_type, bool> {
    if (needsResize()) {
      constexpr size_type kMaxCapacity = size_type{1} << 60;
      size_type new_cap = capacity_ * 2;
      assert(new_cap > capacity_ && "insertSlot: capacity overflow");
      assert(new_cap <= kMaxCapacity && "insertSlot: capacity exceeds maximum");
      resize(new_cap);
    }

    std::size_t hash = hasher_(key);
    std::uint8_t h2_byte = h2Prefetch(hash);
    ProbeSeqPrefetch seq{hash, capacity_ - 1};

    while (true) {
      // Prefetch NEXT group while processing CURRENT group
      size_type next_off = seq.nextOffset();
      _mm_prefetch(reinterpret_cast<const char*>(&ctrl_[next_off]), _MM_HINT_T0);
      _mm_prefetch(reinterpret_cast<const char*>(&slots_[next_off]), _MM_HINT_T0);

      GroupPrefetch g = GroupPrefetch::load(&ctrl_[seq.offset()]);

      // Check for existing key
      BitMaskPrefetch matches{g.match(h2_byte)};
      while (matches) {
        size_type idx = seq.offset(*matches);
        if (equal_(slots_[idx].data()->first, key)) {
          return {idx, false};
        }
        ++matches;
      }

      // Find first empty or deleted slot
      BitMaskPrefetch available{g.matchEmptyOrDeleted()};
      if (available) {
        size_type idx = seq.offset(*available);

        if (ctrl_[idx] == kDeletedPrefetch) {
          --deleted_;
        }

        ctrl_[idx] = h2_byte;
        slots_[idx].construct(key, Value{});
        ++size_;
        return {idx, true};
      }

      seq.next();
    }
  }

  constexpr void resize(size_type new_capacity) {
    new_capacity = normalizeCapacity(std::max(new_capacity, kMinCapacity));

    constexpr size_type kMaxCapacity = size_type{1} << 60;
    assert(new_capacity <= kMaxCapacity && "resize: capacity exceeds maximum");

    auto old_ctrl = std::move(ctrl_);
    auto old_slots = std::move(slots_);
    size_type old_capacity = capacity_;

    capacity_ = new_capacity;
    ctrl_ = allocateControls(capacity_);
    slots_ = std::make_unique<Slot[]>(capacity_);
    size_ = 0;
    deleted_ = 0;
    resetControls();

    // Reinsert all elements
    for (size_type i = 0; i < old_capacity; ++i) {
      if (isOccupied(old_ctrl[i])) {
        auto& [key, value] = *old_slots[i].mutableData();
        insertWithValue(std::move(key), std::move(value));
      }
    }

    // Clean up old slots
    for (size_type i = 0; i < old_capacity; ++i) {
      if (isOccupied(old_ctrl[i])) {
        old_slots[i].destroy();
      }
    }
  }

  constexpr void insertWithValue(Key key, Value value) {
    std::size_t hash = hasher_(key);
    std::uint8_t h2_byte = h2Prefetch(hash);
    ProbeSeqPrefetch seq{hash, capacity_ - 1};

    while (true) {
      GroupPrefetch g = GroupPrefetch::load(&ctrl_[seq.offset()]);
      BitMaskPrefetch available{g.matchEmptyOrDeleted()};

      if (available) {
        size_type idx = seq.offset(*available);
        ctrl_[idx] = h2_byte;
        slots_[idx].construct(std::move(key), std::move(value));
        ++size_;
        return;
      }

      seq.next();
    }
  }

  // --------------------------------------------------------------------------
  // Member Variables
  // --------------------------------------------------------------------------

  size_type capacity_ = kMinCapacity;
  size_type size_ = 0;
  size_type deleted_ = 0;

  std::unique_ptr<std::uint8_t[]> ctrl_ = nullptr;
  std::unique_ptr<Slot[]> slots_ = nullptr;

  [[no_unique_address]] Hash hasher_{};
  [[no_unique_address]] KeyEqual equal_{};
};

}  // namespace tempura
