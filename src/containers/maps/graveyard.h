#pragma once

// ============================================================================
// Graveyard Hashing
// ============================================================================
//
// Graveyard Hashing is a variant of linear probing that strategically maintains
// tombstones at ~50% of free slots to combat primary clustering.
//
// THE PROBLEM WITH LINEAR PROBING
// --------------------------------
// Linear probing suffers from "primary clustering": long runs of occupied slots
// form and grow over time. Once a cluster forms, any key that hashes into or
// near it must probe through the entire cluster, making operations slow.
//
// Example of primary clustering:
//   [A][B][C][D][E][F][G][ ][ ]...
//
// Any new key hashing to slots 0-6 must probe past all occupied slots.
// The cluster grows monotonically - it can only get longer, never shorter.
//
// THE GRAVEYARD INSIGHT
// ---------------------
// Tombstones break up clusters! Consider:
//   [A][B][†][D][E][†][G][ ][ ]...
//
// Now keys hashing to slots 2 or 5 can stop their probe early when inserting.
// The tombstones act as "speed bumps" that prevent clusters from coalescing
// into one giant cluster.
//
// Key insight from the paper: maintain tombstones at ~50% of free slots.
// This is the sweet spot - enough tombstones to break clusters, but not so
// many that they slow down searches (tombstones must be skipped during search).
//
// STRATEGY
// --------
// 1. On DELETE: mark slot as tombstone (natural tombstone creation)
// 2. On INSERT reusing tombstone: just reuse it, don't create new tombstones
// 3. During REBUILD: place tombstones at evenly spaced "primitive positions"
//    - Target ~50% of free slots as tombstones
//    - Evenly spaced distribution prevents clustering
// 4. When tombstones exceed 60% of free slots: rehash at same capacity
//    - This resets the distribution without growing the table
//
// WHY THIS WORKS
// --------------
// Traditional linear probing: tombstones accumulate until resize
// Graveyard hashing: tombstones are actively maintained as a cluster-busting tool
//
// The ~50% target is based on:
//   - Too few tombstones: clusters still form
//   - Too many tombstones: search slows down (must skip many tombstones)
//   - ~50%: optimal tradeoff between cluster prevention and search speed
//
// PERFORMANCE
// -----------
// Compared to standard linear probing:
//   - INSERT: similar or better (tombstones break up clusters)
//   - SEARCH: similar (tombstones are skipped)
//   - DELETE: slightly more work (may create synthetic tombstone)
//
// The key win is preventing worst-case scenarios where clusters dominate
// the table and probe sequences become extremely long.
//
// ============================================================================

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <utility>

#include "containers/maps/hasher.h"
#include "containers/maps/map_storage.h"
#include "meta/manual_lifetime.h"

namespace tempura {

// ============================================================================
// GraveyardMap
// ============================================================================

template <typename Key, typename Value, Hasher<Key> Hash = std::hash<Key>,
          typename KeyEqual = std::equal_to<Key>>
class GraveyardMap {
 public:
  // --------------------------------------------------------------------------
  // Type Aliases
  // --------------------------------------------------------------------------

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
    enum class State : std::uint8_t {
      kEmpty,      // Never used, or cleaned during resize
      kOccupied,   // Contains a valid key-value pair
      kTombstone,  // Deleted slot - breaks up clusters
    };

    State state = State::kEmpty;
    ManualLifetime<MapStorage<Key, Value>> storage;

    auto isEmpty() const -> bool { return state == State::kEmpty; }
    auto isOccupied() const -> bool { return state == State::kOccupied; }
    auto isTombstone() const -> bool { return state == State::kTombstone; }

    // Internal access (mutable) - for moves during resize
    auto mutableData() -> std::pair<Key, Value>* {
      return &storage.get().mutable_pair;
    }

    // User-facing access (const key) - prevents key mutation
    auto data() -> std::pair<const Key, Value>* {
      return &storage.get().const_pair;
    }
    auto data() const -> const std::pair<const Key, Value>* {
      return &storage.get().const_pair;
    }

    template <typename... Args>
    void construct(Args&&... args) {
      storage.construct();
      std::construct_at(&storage.get().mutable_pair, std::forward<Args>(args)...);
      state = State::kOccupied;
    }

    void destroy() {
      if (state == State::kOccupied) {
        std::destroy_at(&storage.get().mutable_pair);
        storage.destruct();
      }
      state = State::kEmpty;
    }

    void markTombstone() {
      if (state == State::kOccupied) {
        std::destroy_at(&storage.get().mutable_pair);
        storage.destruct();
      }
      state = State::kTombstone;
    }
  };

  // --------------------------------------------------------------------------
  // Constants
  // --------------------------------------------------------------------------

  // Load factor threshold: resize when (size + tombstones) * 10 >= capacity * 7
  static constexpr size_type kLoadFactorNumerator = 7;
  static constexpr size_type kLoadFactorDenominator = 10;

  // Tombstone target: aim for deleted ≈ (capacity - size) / 2
  // Lower bound: deleted * 10 < free * 4 (40%)
  // Upper bound: deleted * 10 > free * 6 (60%)
  static constexpr size_type kTombstoneLowNumerator = 4;
  static constexpr size_type kTombstoneHighNumerator = 6;
  static constexpr size_type kTombstoneDenominator = 10;

  static constexpr size_type kMinCapacity = 8;

  // --------------------------------------------------------------------------
  // Constructors
  // --------------------------------------------------------------------------

  constexpr GraveyardMap() : GraveyardMap{kMinCapacity} {}

  constexpr explicit GraveyardMap(size_type initial_capacity)
      : capacity_{std::max(initial_capacity, kMinCapacity)},
        slots_{std::make_unique<Slot[]>(capacity_)} {}

  // Copy constructor: deep copy all occupied slots
  constexpr GraveyardMap(const GraveyardMap& other)
      : capacity_{other.capacity_},
        size_{other.size_},
        tombstone_count_{other.tombstone_count_},
        slots_{std::make_unique<Slot[]>(capacity_)} {
    for (size_type i = 0; i < capacity_; ++i) {
      if (other.slots_[i].isOccupied()) {
        slots_[i].construct(*other.slots_[i].data());
      } else {
        slots_[i].state = other.slots_[i].state;
      }
    }
  }

  // Move constructor: steal resources
  constexpr GraveyardMap(GraveyardMap&& other) noexcept
      : capacity_{other.capacity_},
        size_{other.size_},
        tombstone_count_{other.tombstone_count_},
        slots_{std::move(other.slots_)} {
    other.size_ = 0;
    other.capacity_ = 0;
    other.tombstone_count_ = 0;
  }

  // Copy assignment using copy-and-swap
  constexpr auto operator=(const GraveyardMap& other) -> GraveyardMap& {
    if (this != &other) {
      GraveyardMap tmp{other};
      swap(tmp);
    }
    return *this;
  }

  // Move assignment using swap
  constexpr auto operator=(GraveyardMap&& other) noexcept -> GraveyardMap& {
    if (this != &other) {
      GraveyardMap tmp{std::move(other)};
      swap(tmp);
    }
    return *this;
  }

  constexpr ~GraveyardMap() {
    if (slots_) {
      for (size_type i = 0; i < capacity_; ++i) {
        slots_[i].destroy();
      }
    }
  }

  constexpr void swap(GraveyardMap& other) noexcept {
    std::swap(capacity_, other.capacity_);
    std::swap(size_, other.size_);
    std::swap(tombstone_count_, other.tombstone_count_);
    std::swap(slots_, other.slots_);
    std::swap(hasher_, other.hasher_);
    std::swap(equal_, other.equal_);
  }

  // --------------------------------------------------------------------------
  // Capacity
  // --------------------------------------------------------------------------

  constexpr auto size() const noexcept -> size_type {
    return size_;
  }

  constexpr auto capacity() const noexcept -> size_type {
    return capacity_;
  }

  constexpr auto empty() const noexcept -> bool {
    return size_ == 0;
  }

  constexpr auto loadFactor() const noexcept -> double {
    return static_cast<double>(size_ + tombstone_count_) /
           static_cast<double>(capacity_);
  }

  constexpr auto tombstoneCount() const noexcept -> size_type {
    return tombstone_count_;
  }

  // --------------------------------------------------------------------------
  // Lookup
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
  // Insertion
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
      return false;  // Key not found
    }

    slots_[idx].markTombstone();
    --size_;
    ++tombstone_count_;

    // Check if we need more tombstones to maintain target ratio
    // This should rarely trigger since deletion naturally creates tombstones
    maintainTombstones();

    return true;
  }

  constexpr void clear() {
    for (size_type i = 0; i < capacity_; ++i) {
      slots_[i].destroy();
    }
    size_ = 0;
    tombstone_count_ = 0;
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

    constexpr Iterator(GraveyardMap* map, size_type idx)
        : map_{map}, idx_{idx} {
      advanceToOccupied();
    }

    constexpr auto operator*() const -> reference { return *map_->slots_[idx_].data(); }

    constexpr auto operator->() const -> pointer { return map_->slots_[idx_].data(); }

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
      while (idx_ < map_->capacity_ && !map_->slots_[idx_].isOccupied()) {
        ++idx_;
      }
    }

    GraveyardMap* map_;
    size_type idx_;
  };

  class ConstIterator {
   public:
    using difference_type = std::ptrdiff_t;
    using value_type = std::pair<const Key, Value>;
    using pointer = const value_type*;
    using reference = const value_type&;

    constexpr ConstIterator(const GraveyardMap* map, size_type idx)
        : map_{map}, idx_{idx} {
      advanceToOccupied();
    }

    constexpr auto operator*() const -> reference { return *map_->slots_[idx_].data(); }

    constexpr auto operator->() const -> pointer { return map_->slots_[idx_].data(); }

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
      while (idx_ < map_->capacity_ && !map_->slots_[idx_].isOccupied()) {
        ++idx_;
      }
    }

    const GraveyardMap* map_;
    size_type idx_;
  };

  constexpr auto begin() -> Iterator { return {this, 0}; }

  constexpr auto end() -> Iterator { return {this, capacity_}; }

  constexpr auto begin() const -> ConstIterator {
    return {this, 0};
  }

  constexpr auto end() const -> ConstIterator {
    return {this, capacity_};
  }

 private:
  // --------------------------------------------------------------------------
  // Internal Helpers
  // --------------------------------------------------------------------------

  constexpr auto needsResize() const -> bool {
    return (size_ + tombstone_count_) * kLoadFactorDenominator >=
           capacity_ * kLoadFactorNumerator;
  }

  constexpr auto hashIndex(const Key& key) const -> size_type {
    return hasher_(key) % capacity_;
  }

  constexpr auto findSlot(const Key& key) const -> size_type {
    size_type idx = hashIndex(key);
    size_type probes = 0;

    while (probes < capacity_) {
      const Slot& slot = slots_[idx];

      if (slot.isEmpty()) {
        return capacity_;
      }

      if (slot.isOccupied() && equal_(slot.data()->first, key)) {
        return idx;
      }

      // Collision or tombstone: continue probing
      idx = (idx + 1) % capacity_;
      ++probes;
    }

    return capacity_;
  }

  constexpr auto insertSlot(const Key& key) -> std::pair<size_type, bool> {
    if (needsResize()) {
      constexpr size_type kMaxCapacity = size_type{1} << 60;
      size_type new_cap = capacity_ * 2;
      assert(new_cap > capacity_ && "insertSlot: capacity overflow");
      assert(new_cap <= kMaxCapacity && "insertSlot: capacity exceeds maximum");
      resize(new_cap);
    }

    size_type idx = hashIndex(key);
    size_type first_tombstone = capacity_;
    size_type probes = 0;

    while (probes < capacity_) {
      Slot& slot = slots_[idx];

      if (slot.isEmpty()) {
        size_type insert_idx = (first_tombstone != capacity_) ? first_tombstone : idx;

        if (first_tombstone != capacity_) {
          // Reusing a tombstone - decrement count
          --tombstone_count_;
        }

        slots_[insert_idx].construct(key, Value{});
        ++size_;

        maintainTombstones();
        return {insert_idx, true};
      }

      if (slot.isTombstone() && first_tombstone == capacity_) {
        first_tombstone = idx;
      }

      if (slot.isOccupied() && equal_(slot.data()->first, key)) {
        return {idx, false};
      }

      idx = (idx + 1) % capacity_;
      ++probes;
    }

    assert(false && "insertSlot: table full despite load factor check");
    std::unreachable();
  }

  // Place tombstones at evenly spaced primitive positions
  constexpr void placePrimitiveTombstones() {
    size_type free_slots = capacity_ - size_;
    size_type target_tombstones = free_slots / 2;

    if (target_tombstones == 0) return;

    size_type spacing = capacity_ / target_tombstones;
    size_type placed = 0;

    for (size_type pos = spacing / 2; pos < capacity_ && placed < target_tombstones; pos += spacing) {
      if (slots_[pos].isEmpty()) {
        slots_[pos].markTombstone();
        ++tombstone_count_;
        ++placed;
      }
    }
  }

  // Maintain tombstone count at target level
  constexpr void maintainTombstones() {
    size_type free_slots = capacity_ - size_;

    // If we have too many tombstones (> 60% of free slots), rehash
    // Rehashing at same capacity clears tombstones without growing
    if (tombstone_count_ * kTombstoneDenominator >
        free_slots * kTombstoneHighNumerator) {
      rehashSameCapacity();
    }
  }

  // Rehash at same capacity to clean up tombstones
  constexpr void rehashSameCapacity() {
    auto old_slots = std::move(slots_);
    size_type old_capacity = capacity_;

    slots_ = std::make_unique<Slot[]>(capacity_);
    size_ = 0;
    tombstone_count_ = 0;

    // Re-insert all occupied elements
    for (size_type i = 0; i < old_capacity; ++i) {
      if (old_slots[i].isOccupied()) {
        auto& [key, value] = *old_slots[i].mutableData();
        size_type idx = insertSlotNoResize(key);
        slots_[idx].mutableData()->second = std::move(value);
      }
    }

    // Clean up old slots
    for (size_type i = 0; i < old_capacity; ++i) {
      old_slots[i].destroy();
    }
  }

  constexpr void resize(size_type new_capacity) {
    new_capacity = std::max(new_capacity, kMinCapacity);

    constexpr size_type kMaxCapacity = size_type{1} << 60;
    assert(new_capacity <= kMaxCapacity && "resize: capacity exceeds maximum");

    auto old_slots = std::move(slots_);
    size_type old_capacity = capacity_;

    capacity_ = new_capacity;
    slots_ = std::make_unique<Slot[]>(capacity_);
    size_ = 0;
    tombstone_count_ = 0;

    // Re-insert all occupied elements
    for (size_type i = 0; i < old_capacity; ++i) {
      if (old_slots[i].isOccupied()) {
        auto& [key, value] = *old_slots[i].mutableData();
        size_type idx = insertSlotNoResize(key);
        slots_[idx].mutableData()->second = std::move(value);
      }
    }

    // Clean up old slots
    for (size_type i = 0; i < old_capacity; ++i) {
      old_slots[i].destroy();
    }

    // Place tombstones at evenly spaced positions
    placePrimitiveTombstones();
  }

  constexpr auto insertSlotNoResize(const Key& key) -> size_type {
    size_type idx = hashIndex(key);

    for (size_type probes = 0; probes < capacity_; ++probes) {
      Slot& slot = slots_[idx];
      if (slot.isEmpty()) {
        slot.construct(key, Value{});
        ++size_;
        return idx;
      }
      idx = (idx + 1) % capacity_;
    }

    std::unreachable();
  }

  // --------------------------------------------------------------------------
  // Member Variables
  // --------------------------------------------------------------------------

  size_type capacity_ = kMinCapacity;
  size_type size_ = 0;
  size_type tombstone_count_ = 0;
  std::unique_ptr<Slot[]> slots_ = nullptr;

  [[no_unique_address]] Hash hasher_{};
  [[no_unique_address]] KeyEqual equal_{};
};

}  // namespace tempura
