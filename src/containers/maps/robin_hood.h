#pragma once

// ============================================================================
// Robin Hood Hash Map
// ============================================================================
//
// Robin Hood hashing is a refinement of linear probing that dramatically
// reduces probe length variance. The key insight: "steal from the rich,
// give to the poor."
//
// THE ROBIN HOOD PRINCIPLE
// ------------------------
// Each element has a "displacement" - how far it is from its ideal position.
// An element at its ideal slot has displacement 0, one slot away has 1, etc.
//
// During insertion, if the inserting element has traveled FURTHER than the
// current occupant (larger displacement), we swap them. The displaced element
// continues probing. This ensures no element gets TOO far from home.
//
// EXAMPLE: Insert key with hash=5, table has occupant at slot 6 with disp=0
//
//   Inserting at slot 5: occupied, our disp=0, theirs=? → probe to 6
//   Inserting at slot 6: occupied, our disp=1, theirs=0 → 1 > 0, SWAP!
//   Now we're inserting the displaced element (disp=1) at slot 7...
//
// WHY IT WORKS
// ------------
// Standard linear probing: some elements have displacement 0, others 10+
// Robin Hood: everyone has displacement ~2-3 (variance minimized)
//
// Expected maximum probe length:
//   Linear probing: O(log n) average, O(n) worst case
//   Robin Hood: O(log log n) expected maximum!
//
// EARLY TERMINATION
// -----------------
// During lookup, if we see an element with displacement LESS than what ours
// would be at that position, we know our key isn't in the table. It would
// have stolen that slot during insertion.
//
// BACKWARD-SHIFT DELETION
// -----------------------
// Robin Hood doesn't use tombstones. Instead, when deleting:
//   1. Remove the element, leaving a hole
//   2. Shift subsequent elements backward if they have displacement > 0
//   3. Stop when we hit an empty slot or element at its ideal position
//
// This keeps the table clean but makes deletion O(n) worst case.
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

template <typename Key, typename Value, Hasher<Key> Hash = std::hash<Key>,
          typename KeyEqual = std::equal_to<Key>>
class RobinHoodMap {
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
  // Each slot tracks its displacement from ideal position. This enables the
  // Robin Hood swap decision and early termination during lookup.

  struct Slot {
    enum class State : std::uint8_t {
      kEmpty,
      kOccupied,
    };

    State state = State::kEmpty;
    size_type displacement = 0;  // Distance from ideal position
    ManualLifetime<MapStorage<Key, Value>> storage;

    auto isEmpty() const -> bool { return state == State::kEmpty; }
    auto isOccupied() const -> bool { return state == State::kOccupied; }

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
    void construct(size_type disp, Args&&... args) {
      storage.construct();
      std::construct_at(&storage.get().mutable_pair,
                        std::forward<Args>(args)...);
      displacement = disp;
      state = State::kOccupied;
    }

    void destroy() {
      if (state == State::kOccupied) {
        std::destroy_at(&storage.get().mutable_pair);
        storage.destruct();
      }
      state = State::kEmpty;
      displacement = 0;
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

  constexpr RobinHoodMap() : RobinHoodMap{kMinCapacity} {}

  constexpr explicit RobinHoodMap(size_type initial_capacity)
      : capacity_{std::max(initial_capacity, kMinCapacity)},
        slots_{std::make_unique<Slot[]>(capacity_)} {}

  constexpr RobinHoodMap(const RobinHoodMap& other)
      : capacity_{other.capacity_},
        size_{other.size_},
        slots_{std::make_unique<Slot[]>(capacity_)} {
    for (size_type i = 0; i < capacity_; ++i) {
      if (other.slots_[i].isOccupied()) {
        slots_[i].construct(other.slots_[i].displacement,
                            *other.slots_[i].data());
      }
    }
  }

  constexpr RobinHoodMap(RobinHoodMap&& other) noexcept
      : capacity_{other.capacity_},
        size_{other.size_},
        slots_{std::move(other.slots_)} {
    other.size_ = 0;
    other.capacity_ = 0;
  }

  constexpr auto operator=(const RobinHoodMap& other) -> RobinHoodMap& {
    if (this != &other) {
      RobinHoodMap tmp{other};
      swap(tmp);
    }
    return *this;
  }

  constexpr auto operator=(RobinHoodMap&& other) noexcept -> RobinHoodMap& {
    if (this != &other) {
      RobinHoodMap tmp{std::move(other)};
      swap(tmp);
    }
    return *this;
  }

  constexpr ~RobinHoodMap() {
    if (slots_) {
      for (size_type i = 0; i < capacity_; ++i) {
        slots_[i].destroy();
      }
    }
  }

  constexpr void swap(RobinHoodMap& other) noexcept {
    std::swap(capacity_, other.capacity_);
    std::swap(size_, other.size_);
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
  // Lookup
  // --------------------------------------------------------------------------
  // Robin Hood enables early termination: if we see an element with
  // displacement less than what ours would be, our key can't be in the table.

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
  // Robin Hood uses backward-shift deletion instead of tombstones.
  // After removing an element, we shift subsequent elements back if they
  // have displacement > 0 (meaning they'd prefer to be closer to home).

  constexpr auto erase(const Key& key) -> bool {
    size_type idx = findSlot(key);
    if (idx == capacity_) {
      return false;
    }

    // Destroy the element
    slots_[idx].destroy();
    --size_;

    // Backward-shift: move subsequent elements back toward their ideal position
    size_type hole = idx;
    size_type next = (hole + 1) % capacity_;

    while (slots_[next].isOccupied() && slots_[next].displacement > 0) {
      // Move element from 'next' to 'hole', decrementing its displacement
      slots_[hole].construct(slots_[next].displacement - 1,
                             std::move(*slots_[next].mutableData()));
      slots_[next].destroy();

      hole = next;
      next = (next + 1) % capacity_;
    }

    return true;
  }

  constexpr void clear() {
    for (size_type i = 0; i < capacity_; ++i) {
      slots_[i].destroy();
    }
    size_ = 0;
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

    constexpr Iterator(RobinHoodMap* map, size_type idx)
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
      while (idx_ < map_->capacity_ && !map_->slots_[idx_].isOccupied()) {
        ++idx_;
      }
    }

    RobinHoodMap* map_;
    size_type idx_;
  };

  class ConstIterator {
   public:
    using difference_type = std::ptrdiff_t;
    using value_type = std::pair<const Key, Value>;
    using pointer = const value_type*;
    using reference = const value_type&;

    constexpr ConstIterator(const RobinHoodMap* map, size_type idx)
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
      while (idx_ < map_->capacity_ && !map_->slots_[idx_].isOccupied()) {
        ++idx_;
      }
    }

    const RobinHoodMap* map_;
    size_type idx_;
  };

  constexpr auto begin() -> Iterator { return {this, 0}; }
  constexpr auto end() -> Iterator { return {this, capacity_}; }
  constexpr auto begin() const -> ConstIterator { return {this, 0}; }
  constexpr auto end() const -> ConstIterator { return {this, capacity_}; }

  // --------------------------------------------------------------------------
  // Probe Statistics (for analysis)
  // --------------------------------------------------------------------------

  constexpr auto maxDisplacement() const -> size_type {
    size_type max_disp = 0;
    for (size_type i = 0; i < capacity_; ++i) {
      if (slots_[i].isOccupied()) {
        max_disp = std::max(max_disp, slots_[i].displacement);
      }
    }
    return max_disp;
  }

  constexpr auto totalDisplacement() const -> size_type {
    size_type total = 0;
    for (size_type i = 0; i < capacity_; ++i) {
      if (slots_[i].isOccupied()) {
        total += slots_[i].displacement;
      }
    }
    return total;
  }

 private:
  // --------------------------------------------------------------------------
  // Internal Helpers
  // --------------------------------------------------------------------------

  constexpr auto needsResize() const -> bool {
    return size_ * kLoadFactorDenominator >= capacity_ * kLoadFactorNumerator;
  }

  constexpr auto hashIndex(const Key& key) const -> size_type {
    return hasher_(key) % capacity_;
  }

  // Find slot with early termination.
  // If we see an element with displacement < what ours would be, stop.
  constexpr auto findSlot(const Key& key) const -> size_type {
    size_type ideal = hashIndex(key);
    size_type idx = ideal;
    size_type disp = 0;

    while (disp < capacity_) {
      const Slot& slot = slots_[idx];

      if (slot.isEmpty()) {
        return capacity_;  // Not found
      }

      // Early termination: this element is "richer" than we would be,
      // so we would have stolen its slot if we existed
      if (slot.displacement < disp) {
        return capacity_;
      }

      if (equal_(slot.data()->first, key)) {
        return idx;
      }

      idx = (idx + 1) % capacity_;
      ++disp;
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

    size_type ideal = hashIndex(key);
    size_type idx = ideal;
    size_type disp = 0;

    // The key and value we're currently trying to place
    Key current_key = key;
    Value current_value{};
    bool is_original = true;  // Track if we're still looking for the original key

    while (disp < capacity_) {
      Slot& slot = slots_[idx];

      if (slot.isEmpty()) {
        // Found empty slot - insert here
        slot.construct(disp, std::move(current_key), std::move(current_value));
        ++size_;
        // Return the index of the original key (might not be this slot if we swapped)
        if (is_original) {
          return {idx, true};
        }
        // Find where the original key ended up
        return {findSlot(key), true};
      }

      // Check if this is the key we're looking for (only for original key)
      if (is_original && equal_(slot.data()->first, current_key)) {
        return {idx, false};  // Key already exists
      }

      // Robin Hood: if we've traveled further than the occupant, swap
      if (disp > slot.displacement) {
        // Swap our current key/value with the occupant
        std::swap(current_key, slot.mutableData()->first);
        std::swap(current_value, slot.mutableData()->second);
        std::swap(disp, slot.displacement);
        is_original = false;  // Now inserting a displaced element
      }

      idx = (idx + 1) % capacity_;
      ++disp;
    }

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

    for (size_type i = 0; i < old_capacity; ++i) {
      if (old_slots[i].isOccupied()) {
        auto& [key, value] = *old_slots[i].mutableData();
        insertWithValue(std::move(key), std::move(value));
      }
    }

    for (size_type i = 0; i < old_capacity; ++i) {
      old_slots[i].destroy();
    }
  }

  // Insert with a known value (used during resize)
  constexpr void insertWithValue(Key key, Value value) {
    size_type ideal = hashIndex(key);
    size_type idx = ideal;
    size_type disp = 0;

    while (disp < capacity_) {
      Slot& slot = slots_[idx];

      if (slot.isEmpty()) {
        slot.construct(disp, std::move(key), std::move(value));
        ++size_;
        return;
      }

      if (disp > slot.displacement) {
        std::swap(key, slot.mutableData()->first);
        std::swap(value, slot.mutableData()->second);
        std::swap(disp, slot.displacement);
      }

      idx = (idx + 1) % capacity_;
      ++disp;
    }

    std::unreachable();
  }

  // --------------------------------------------------------------------------
  // Member Variables
  // --------------------------------------------------------------------------

  size_type capacity_ = kMinCapacity;
  size_type size_ = 0;
  std::unique_ptr<Slot[]> slots_ = nullptr;

  [[no_unique_address]] Hash hasher_{};
  [[no_unique_address]] KeyEqual equal_{};
};

}  // namespace tempura
