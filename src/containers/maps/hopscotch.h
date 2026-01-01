#pragma once

// ============================================================================
// Hopscotch Hash Map
// ============================================================================
//
// Hopscotch hashing combines the cache-friendliness of linear probing with
// bounded worst-case lookup time. Each bucket has a "neighborhood" - a bitmap
// tracking which of the next H slots contain elements that hash to this bucket.
//
// THE HOPSCOTCH PRINCIPLE
// -----------------------
// - Each bucket has a neighborhood of H slots (typically 32, matching word size)
// - An element MUST reside within H slots of its ideal bucket
// - A bitmap in each bucket tracks which nearby slots belong to it
//
// EXAMPLE: H=4, insert key with hash=2
//
//   Buckets:   [0]    [1]    [2]    [3]    [4]    [5]
//   Bitmap:    0000   0000   0110   0000   0000   0000
//   Data:      _      _      A      B      _      _
//
//   Bucket 2's bitmap is 0110 (binary), meaning:
//     - Slot 2+1=3 has an element from bucket 2 (the "1" at position 1)
//     - Slot 2+2=4 would have one too (but it's empty in this example)
//
// INSERTION ALGORITHM
// -------------------
// 1. Find the first empty slot at or after hash(key)
// 2. If it's within H slots of the ideal bucket, insert and set bitmap bit
// 3. If too far, "hop" an element closer: find an element in an intermediate
//    bucket that can move forward, freeing up a closer slot
// 4. Repeat until either we get within H or no hops are possible (rehash)
//
// WHY IT WORKS
// ------------
// Lookup: Check bitmap, then probe only the indicated slots (at most H checks)
// Insert: Amortized O(1), worst case may require hopping or rehash
// Delete: O(1) - just clear the element and bitmap bit
//
// The bitmap makes lookup very fast - we skip slots that don't belong to us.
// The bounded neighborhood ensures cache-friendly access patterns.
//
// ============================================================================

#include <bit>
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
class HopscotchMap {
 public:
  using key_type = Key;
  using mapped_type = Value;
  using value_type = std::pair<const Key, Value>;
  using size_type = std::size_t;
  using hasher = Hash;
  using key_equal = KeyEqual;

  // --------------------------------------------------------------------------
  // Constants
  // --------------------------------------------------------------------------

  // Neighborhood size - how many slots each bucket can claim
  // 32 is optimal: fits in uint32_t bitmap, good cache line coverage
  static constexpr size_type kNeighborhoodSize = 32;
  using NeighborhoodBitmap = std::uint32_t;

  static constexpr size_type kLoadFactorNumerator = 7;
  static constexpr size_type kLoadFactorDenominator = 10;
  static constexpr size_type kMinCapacity = kNeighborhoodSize;

  // --------------------------------------------------------------------------
  // Slot Structure
  // --------------------------------------------------------------------------

  struct Slot {
    enum class State : std::uint8_t {
      kEmpty,
      kOccupied,
    };

    State state = State::kEmpty;
    NeighborhoodBitmap neighborhood = 0;  // Bitmap of elements hashing here
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
    void construct(Args&&... args) {
      storage.construct();
      std::construct_at(&storage.get().mutable_pair,
                        std::forward<Args>(args)...);
      state = State::kOccupied;
    }

    void destroy() {
      if (state == State::kOccupied) {
        std::destroy_at(&storage.get().mutable_pair);
        storage.destruct();
      }
      state = State::kEmpty;
    }
  };

  // --------------------------------------------------------------------------
  // Constructors
  // --------------------------------------------------------------------------

  constexpr HopscotchMap() : HopscotchMap{kMinCapacity} {}

  constexpr explicit HopscotchMap(size_type initial_capacity)
      : capacity_{std::max(initial_capacity, kMinCapacity)},
        slots_{std::make_unique<Slot[]>(capacity_)} {}

  constexpr HopscotchMap(const HopscotchMap& other)
      : capacity_{other.capacity_},
        size_{other.size_},
        slots_{std::make_unique<Slot[]>(capacity_)} {
    for (size_type i = 0; i < capacity_; ++i) {
      slots_[i].neighborhood = other.slots_[i].neighborhood;
      if (other.slots_[i].isOccupied()) {
        slots_[i].construct(*other.slots_[i].data());
      }
    }
  }

  constexpr HopscotchMap(HopscotchMap&& other) noexcept
      : capacity_{other.capacity_},
        size_{other.size_},
        slots_{std::move(other.slots_)} {
    other.size_ = 0;
    other.capacity_ = 0;
  }

  constexpr auto operator=(const HopscotchMap& other) -> HopscotchMap& {
    if (this != &other) {
      HopscotchMap tmp{other};
      swap(tmp);
    }
    return *this;
  }

  constexpr auto operator=(HopscotchMap&& other) noexcept -> HopscotchMap& {
    if (this != &other) {
      HopscotchMap tmp{std::move(other)};
      swap(tmp);
    }
    return *this;
  }

  constexpr ~HopscotchMap() {
    if (slots_) {
      for (size_type i = 0; i < capacity_; ++i) {
        slots_[i].destroy();
      }
    }
  }

  constexpr void swap(HopscotchMap& other) noexcept {
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
  // Check the neighborhood bitmap, then only probe indicated slots.
  // At most kNeighborhoodSize checks.

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
  // O(1) - just clear the element and the bitmap bit in its home bucket.

  constexpr auto erase(const Key& key) -> bool {
    size_type ideal = hashIndex(key);
    NeighborhoodBitmap bitmap = slots_[ideal].neighborhood;

    while (bitmap != 0) {
      int offset = std::countr_zero(bitmap);
      size_type idx = (ideal + offset) % capacity_;

      if (slots_[idx].isOccupied() && equal_(slots_[idx].data()->first, key)) {
        slots_[idx].destroy();
        slots_[ideal].neighborhood &= ~(NeighborhoodBitmap{1} << offset);
        --size_;
        return true;
      }

      bitmap &= bitmap - 1;  // Clear lowest set bit
    }

    return false;
  }

  constexpr void clear() {
    for (size_type i = 0; i < capacity_; ++i) {
      slots_[i].destroy();
      slots_[i].neighborhood = 0;
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

    constexpr Iterator(HopscotchMap* map, size_type idx)
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

    HopscotchMap* map_;
    size_type idx_;
  };

  class ConstIterator {
   public:
    using difference_type = std::ptrdiff_t;
    using value_type = std::pair<const Key, Value>;
    using pointer = const value_type*;
    using reference = const value_type&;

    constexpr ConstIterator(const HopscotchMap* map, size_type idx)
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

    const HopscotchMap* map_;
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

  constexpr auto needsResize() const -> bool {
    return size_ * kLoadFactorDenominator >= capacity_ * kLoadFactorNumerator;
  }

  constexpr auto hashIndex(const Key& key) const -> size_type {
    return hasher_(key) % capacity_;
  }

  // Lookup using neighborhood bitmap - only check slots indicated by bitmap
  constexpr auto findSlot(const Key& key) const -> size_type {
    size_type ideal = hashIndex(key);
    NeighborhoodBitmap bitmap = slots_[ideal].neighborhood;

    while (bitmap != 0) {
      int offset = std::countr_zero(bitmap);
      size_type idx = (ideal + offset) % capacity_;

      if (slots_[idx].isOccupied() && equal_(slots_[idx].data()->first, key)) {
        return idx;
      }

      bitmap &= bitmap - 1;  // Clear lowest set bit
    }

    return capacity_;  // Not found
  }

  constexpr auto insertSlot(const Key& key) -> std::pair<size_type, bool> {
    // Check if already exists
    size_type existing = findSlot(key);
    if (existing != capacity_) {
      return {existing, false};
    }

    if (needsResize()) {
      resize(capacity_ * 2);
    }

    size_type ideal = hashIndex(key);

    // Find first empty slot starting from ideal
    size_type empty_slot = ideal;
    size_type distance = 0;
    while (distance < capacity_ && slots_[empty_slot].isOccupied()) {
      empty_slot = (empty_slot + 1) % capacity_;
      ++distance;
    }

    if (distance >= capacity_) {
      // Table is completely full (shouldn't happen with load factor check)
      resize(capacity_ * 2);
      return insertSlot(key);
    }

    // If empty slot is within neighborhood, we're done
    while (distance >= kNeighborhoodSize) {
      // Need to hop: find an element in [ideal, empty_slot) that can move
      if (!hopCloser(ideal, empty_slot, distance)) {
        // Can't hop - must resize
        resize(capacity_ * 2);
        return insertSlot(key);
      }
    }

    // Insert at empty_slot
    slots_[empty_slot].construct(key, Value{});
    slots_[ideal].neighborhood |= (NeighborhoodBitmap{1} << distance);
    ++size_;

    return {empty_slot, true};
  }

  // Try to move an element closer to create space in the neighborhood
  // Returns true if successful, false if no hop possible
  constexpr auto hopCloser(size_type ideal, size_type& empty_slot,
                           size_type& distance) -> bool {
    // Look for a bucket whose element at empty_slot-H+1..empty_slot-1 can hop
    // to empty_slot, bringing the empty slot closer to ideal

    for (size_type hop_dist = kNeighborhoodSize - 1; hop_dist > 0; --hop_dist) {
      // The bucket that could own the element at (empty_slot - hop_dist)
      size_type candidate_slot =
          (empty_slot + capacity_ - hop_dist) % capacity_;
      size_type candidate_home =
          (empty_slot + capacity_ - hop_dist) % capacity_;

      // We need to find the home bucket of the element at candidate_slot
      // and check if that element can be moved to empty_slot

      if (!slots_[candidate_slot].isOccupied()) {
        continue;
      }

      // Find the home bucket of the element at candidate_slot
      size_type element_home = hashIndex(slots_[candidate_slot].data()->first);

      // Can this element move to empty_slot?
      // It can if empty_slot is within kNeighborhoodSize of element_home
      size_type new_offset = (empty_slot + capacity_ - element_home) % capacity_;

      if (new_offset < kNeighborhoodSize) {
        // Move element from candidate_slot to empty_slot
        size_type old_offset =
            (candidate_slot + capacity_ - element_home) % capacity_;

        slots_[empty_slot].construct(
            std::move(*slots_[candidate_slot].mutableData()));
        slots_[candidate_slot].destroy();

        // Update bitmaps
        slots_[element_home].neighborhood &=
            ~(NeighborhoodBitmap{1} << old_offset);
        slots_[element_home].neighborhood |=
            (NeighborhoodBitmap{1} << new_offset);

        // Now empty_slot is at candidate_slot, which is closer to ideal
        empty_slot = candidate_slot;
        distance = (empty_slot + capacity_ - ideal) % capacity_;

        return true;
      }
    }

    return false;  // No hop possible
  }

  constexpr void resize(size_type new_capacity) {
    new_capacity = std::max(new_capacity, kMinCapacity);

    auto old_slots = std::move(slots_);
    size_type old_capacity = capacity_;

    capacity_ = new_capacity;
    slots_ = std::make_unique<Slot[]>(capacity_);
    size_ = 0;

    for (size_type i = 0; i < old_capacity; ++i) {
      if (old_slots[i].isOccupied()) {
        auto& [key, value] = *old_slots[i].mutableData();
        auto [idx, _] = insertSlot(key);
        slots_[idx].data()->second = std::move(value);
      }
    }

    for (size_type i = 0; i < old_capacity; ++i) {
      old_slots[i].destroy();
    }
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
