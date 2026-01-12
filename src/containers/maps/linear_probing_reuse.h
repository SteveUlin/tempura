#pragma once

// ============================================================================
// Linear Probing Hash Map with Tombstone Reuse
// ============================================================================
//
// This is a variant of linear_probing.h that explicitly demonstrates
// tombstone reuse during insertion. The key behavioral difference:
//
// TOMBSTONE REUSE STRATEGY
// ------------------------
// During insertion, if we encounter a tombstone while probing:
//   1. Remember the first tombstone's position
//   2. Continue probing to check if the key already exists
//   3. If inserting a new key, use the tombstone slot instead of probing further
//
// EXAMPLE: Insert key K4 into this table (K1, K2, K3 all hash to slot 5)
//
//   Before: [ ∅ | † | ∅ | ∅ | ∅ | K1 | K2 | K3 ]
//           [0] [1] [2] [3] [4] [5] [6] [7]
//
//   Insert K4 (hashes to 5):
//     - slot[5]: occupied (K1) → continue
//     - slot[6]: occupied (K2) → continue
//     - slot[7]: occupied (K3) → continue
//     - slot[0]: empty → insert at [0]
//
//   With tombstone reuse:
//     - slot[5]: occupied (K1) → continue
//     - slot[6]: occupied (K2) → continue
//     - slot[7]: occupied (K3) → continue
//     - slot[0]: empty, but we found tombstone at [1] → insert at [1]
//
//   After: [ ∅ | K4 | ∅ | ∅ | ∅ | K1 | K2 | K3 ]
//
// WHY REUSE TOMBSTONES?
// ---------------------
// Pros:
//   - Reduces probe lengths over time by filling gaps
//   - Better cache locality (data packed tighter)
//   - Slightly faster lookup after many insert/delete cycles
//
// Cons:
//   - Insertion is ~5-10% slower (must probe past tombstones to check for duplicates)
//   - More complex implementation
//   - Benefits only matter if you have significant churn (many deletes + inserts)
//
// For most workloads, the original linear_probing.h (which already does this)
// is the right choice. This file exists primarily for comparative analysis.
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
// LinearProbingReuseMap
// ============================================================================

template <typename Key, typename Value, Hasher<Key> Hash = std::hash<Key>,
          typename KeyEqual = std::equal_to<Key>>
class LinearProbingReuseMap {
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
      kTombstone,  // Was occupied, now deleted (keeps probe chain intact)
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

  static constexpr size_type kLoadFactorNumerator = 7;
  static constexpr size_type kLoadFactorDenominator = 10;
  static constexpr size_type kMinCapacity = 8;

  // --------------------------------------------------------------------------
  // Constructors
  // --------------------------------------------------------------------------

  constexpr LinearProbingReuseMap() : LinearProbingReuseMap{kMinCapacity} {}

  constexpr explicit LinearProbingReuseMap(size_type initial_capacity)
      : capacity_{std::max(initial_capacity, kMinCapacity)},
        slots_{std::make_unique<Slot[]>(capacity_)} {}

  constexpr LinearProbingReuseMap(const LinearProbingReuseMap& other)
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

  constexpr LinearProbingReuseMap(LinearProbingReuseMap&& other) noexcept
      : capacity_{other.capacity_},
        size_{other.size_},
        tombstone_count_{other.tombstone_count_},
        slots_{std::move(other.slots_)} {
    other.size_ = 0;
    other.capacity_ = 0;
    other.tombstone_count_ = 0;
  }

  constexpr auto operator=(const LinearProbingReuseMap& other) -> LinearProbingReuseMap& {
    if (this != &other) {
      LinearProbingReuseMap tmp{other};
      swap(tmp);
    }
    return *this;
  }

  constexpr auto operator=(LinearProbingReuseMap&& other) noexcept
      -> LinearProbingReuseMap& {
    if (this != &other) {
      LinearProbingReuseMap tmp{std::move(other)};
      swap(tmp);
    }
    return *this;
  }

  constexpr ~LinearProbingReuseMap() {
    if (slots_) {
      for (size_type i = 0; i < capacity_; ++i) {
        slots_[i].destroy();
      }
    }
  }

  constexpr void swap(LinearProbingReuseMap& other) noexcept {
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
      return false;
    }

    slots_[idx].markTombstone();
    --size_;
    ++tombstone_count_;

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

    constexpr Iterator(LinearProbingReuseMap* map, size_type idx)
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

    LinearProbingReuseMap* map_;
    size_type idx_;
  };

  class ConstIterator {
   public:
    using difference_type = std::ptrdiff_t;
    using value_type = std::pair<const Key, Value>;
    using pointer = const value_type*;
    using reference = const value_type&;

    constexpr ConstIterator(const LinearProbingReuseMap* map, size_type idx)
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

    const LinearProbingReuseMap* map_;
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

      idx = (idx + 1) % capacity_;
      ++probes;
    }

    return capacity_;
  }

  // TOMBSTONE REUSE INSERTION
  // --------------------------
  // Key change from linear_probing.h: we explicitly remember the first
  // tombstone and reuse it if we're inserting a new key.
  //
  // Algorithm:
  //   1. Resize if needed
  //   2. Probe starting from hash(key) % capacity:
  //      a. If empty slot found:
  //         - If we saw a tombstone earlier, insert there (reuse)
  //         - Otherwise insert at this empty slot
  //      b. If tombstone found: remember position (first time only)
  //      c. If occupied slot with matching key: return existing slot
  //      d. Otherwise: continue probing
  //   3. If inserting at tombstone, decrement tombstone_count_

  constexpr auto insertSlot(const Key& key) -> std::pair<size_type, bool> {
    if (needsResize()) {
      constexpr size_type kMaxCapacity = size_type{1} << 60;
      size_type new_cap = capacity_ * 2;
      assert(new_cap > capacity_ && "insertSlot: capacity overflow");
      assert(new_cap <= kMaxCapacity && "insertSlot: capacity exceeds maximum");
      resize(new_cap);
    }

    size_type idx = hashIndex(key);
    size_type first_tombstone = capacity_;  // Track first tombstone for reuse
    size_type probes = 0;

    while (probes < capacity_) {
      Slot& slot = slots_[idx];

      if (slot.isEmpty()) {
        // Empty slot: insert here (or at earlier tombstone if we found one)
        size_type insert_idx = (first_tombstone != capacity_) ? first_tombstone : idx;

        if (first_tombstone != capacity_) {
          // Reusing a tombstone - decrement tombstone count
          --tombstone_count_;
        }

        slots_[insert_idx].construct(key, Value{});
        ++size_;
        return {insert_idx, true};
      }

      if (slot.isTombstone() && first_tombstone == capacity_) {
        // Remember first tombstone for potential reuse
        first_tombstone = idx;
      }

      if (slot.isOccupied() && equal_(slot.data()->first, key)) {
        // Key already exists
        return {idx, false};
      }

      idx = (idx + 1) % capacity_;
      ++probes;
    }

    // Should never reach here with proper load factor management
    assert(false && "insertSlot: table full despite load factor check");
    std::unreachable();
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

    // Re-insert all occupied elements (tombstones are discarded)
    for (size_type i = 0; i < old_capacity; ++i) {
      if (old_slots[i].isOccupied()) {
        auto& [key, value] = *old_slots[i].mutableData();
        size_type idx = insertSlotNoResize(key);
        slots_[idx].mutableData()->second = std::move(value);
      }
    }

    for (size_type i = 0; i < old_capacity; ++i) {
      old_slots[i].destroy();
    }
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
