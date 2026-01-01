#pragma once

// ============================================================================
// Cuckoo Hash Map
// ============================================================================
//
// Cuckoo hashing achieves O(1) WORST-CASE lookup by using two hash functions
// and allowing elements to be displaced between two possible locations.
//
// THE CUCKOO PRINCIPLE
// --------------------
// Each key has exactly two possible homes: h1(key) and h2(key).
// To find a key, check both locations - that's it. No probing.
//
// During insertion:
//   1. Try h1(key) - if empty, insert there
//   2. Try h2(key) - if empty, insert there
//   3. If both occupied, kick out the occupant of h1(key) and insert there
//   4. The displaced element moves to its OTHER location
//   5. Repeat until an empty slot is found or we detect a cycle
//
// EXAMPLE: Insert key C with h1(C)=2, h2(C)=5
//
//   Table: [_, A, B, _, _, D, _]  where A is at h1(A), B at h2(B), D at h1(D)
//
//   Slot 2 has B, slot 5 has D. Kick B from slot 2:
//   Table: [_, A, C, _, _, D, _]  B needs new home
//
//   B's other location is h1(B)=4 (was at h2(B)=2):
//   Table: [_, A, C, _, B, D, _]  Done!
//
// WHY IT WORKS
// ------------
// Lookup: O(1) worst case - just check 2 slots
// Insert: O(1) amortized - displacement chains are short on average
//
// The "cuckoo graph" connects slots by (h1, h2) edges. Insertion fails
// only when a connected component has more edges than vertices (a cycle).
// With load factor < 50%, cycles are exponentially rare.
//
// CYCLE DETECTION
// ---------------
// If we displace the same element twice, we're in a cycle. At that point
// we must rehash with new hash functions or larger table.
//
// We use a maximum displacement count as a simpler cycle detection.
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

// Secondary hash function - must be independent of primary
// Uses golden ratio mixing for good distribution
template <typename Key>
struct SecondaryHash {
  auto operator()(const Key& key) const -> std::size_t {
    std::size_t h = std::hash<Key>{}(key);
    // Mix with golden ratio constant to create independent hash
    h ^= h >> 33;
    h *= 0xff51afd7ed558ccdULL;
    h ^= h >> 33;
    h *= 0xc4ceb9fe1a85ec53ULL;
    h ^= h >> 33;
    return h;
  }
};

template <typename Key, typename Value, Hasher<Key> Hash1 = std::hash<Key>,
          Hasher<Key> Hash2 = SecondaryHash<Key>,
          typename KeyEqual = std::equal_to<Key>>
class CuckooMap {
 public:
  using key_type = Key;
  using mapped_type = Value;
  using value_type = std::pair<const Key, Value>;
  using size_type = std::size_t;
  using hasher = Hash1;
  using key_equal = KeyEqual;

  // --------------------------------------------------------------------------
  // Slot Structure
  // --------------------------------------------------------------------------

  struct Slot {
    enum class State : std::uint8_t {
      kEmpty,
      kOccupied,
    };

    State state = State::kEmpty;
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
  // Constants
  // --------------------------------------------------------------------------

  // Cuckoo works best at < 50% load factor per table
  // With 2 tables, effective load factor is ~50% max
  static constexpr size_type kLoadFactorNumerator = 45;
  static constexpr size_type kLoadFactorDenominator = 100;
  static constexpr size_type kMinCapacity = 8;
  static constexpr size_type kMaxDisplacements = 500;  // Cycle detection

  // --------------------------------------------------------------------------
  // Constructors
  // --------------------------------------------------------------------------

  constexpr CuckooMap() : CuckooMap{kMinCapacity} {}

  constexpr explicit CuckooMap(size_type initial_capacity)
      : capacity_{std::max(initial_capacity, kMinCapacity)},
        slots_{std::make_unique<Slot[]>(capacity_)} {}

  constexpr CuckooMap(const CuckooMap& other)
      : capacity_{other.capacity_},
        size_{other.size_},
        slots_{std::make_unique<Slot[]>(capacity_)} {
    for (size_type i = 0; i < capacity_; ++i) {
      if (other.slots_[i].isOccupied()) {
        slots_[i].construct(*other.slots_[i].data());
      }
    }
  }

  constexpr CuckooMap(CuckooMap&& other) noexcept
      : capacity_{other.capacity_},
        size_{other.size_},
        slots_{std::move(other.slots_)} {
    other.size_ = 0;
    other.capacity_ = 0;
  }

  constexpr auto operator=(const CuckooMap& other) -> CuckooMap& {
    if (this != &other) {
      CuckooMap tmp{other};
      swap(tmp);
    }
    return *this;
  }

  constexpr auto operator=(CuckooMap&& other) noexcept -> CuckooMap& {
    if (this != &other) {
      CuckooMap tmp{std::move(other)};
      swap(tmp);
    }
    return *this;
  }

  constexpr ~CuckooMap() {
    if (slots_) {
      for (size_type i = 0; i < capacity_; ++i) {
        slots_[i].destroy();
      }
    }
  }

  constexpr void swap(CuckooMap& other) noexcept {
    std::swap(capacity_, other.capacity_);
    std::swap(size_, other.size_);
    std::swap(slots_, other.slots_);
    std::swap(hasher1_, other.hasher1_);
    std::swap(hasher2_, other.hasher2_);
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
  // Lookup - O(1) worst case!
  // --------------------------------------------------------------------------
  // Just check both possible locations. No probing, no chains.

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

    slots_[idx].destroy();
    --size_;
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

    constexpr Iterator(CuckooMap* map, size_type idx) : map_{map}, idx_{idx} {
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

    CuckooMap* map_;
    size_type idx_;
  };

  class ConstIterator {
   public:
    using difference_type = std::ptrdiff_t;
    using value_type = std::pair<const Key, Value>;
    using pointer = const value_type*;
    using reference = const value_type&;

    constexpr ConstIterator(const CuckooMap* map, size_type idx)
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

    const CuckooMap* map_;
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

  constexpr auto hash1(const Key& key) const -> size_type {
    return hasher1_(key) % capacity_;
  }

  constexpr auto hash2(const Key& key) const -> size_type {
    return hasher2_(key) % capacity_;
  }

  // O(1) lookup - check exactly 2 slots
  constexpr auto findSlot(const Key& key) const -> size_type {
    size_type idx1 = hash1(key);
    if (slots_[idx1].isOccupied() && equal_(slots_[idx1].data()->first, key)) {
      return idx1;
    }

    size_type idx2 = hash2(key);
    if (slots_[idx2].isOccupied() && equal_(slots_[idx2].data()->first, key)) {
      return idx2;
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

    return insertInternal(key, Value{});
  }

  // The cuckoo insertion algorithm
  constexpr auto insertInternal(Key key, Value value)
      -> std::pair<size_type, bool> {
    for (size_type attempt = 0; attempt < kMaxDisplacements; ++attempt) {
      size_type idx1 = hash1(key);

      // Try first location
      if (slots_[idx1].isEmpty()) {
        slots_[idx1].construct(std::move(key), std::move(value));
        ++size_;
        return {idx1, true};
      }

      size_type idx2 = hash2(key);

      // Try second location
      if (slots_[idx2].isEmpty()) {
        slots_[idx2].construct(std::move(key), std::move(value));
        ++size_;
        return {idx2, true};
      }

      // Both occupied - kick out occupant of first slot
      std::swap(key, slots_[idx1].mutableData()->first);
      std::swap(value, slots_[idx1].mutableData()->second);
      // The displaced key will try its other location next iteration
    }

    // Cycle detected - must rehash
    resize(capacity_ * 2);
    return insertInternal(std::move(key), std::move(value));
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
        insertInternal(std::move(key), std::move(value));
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

  [[no_unique_address]] Hash1 hasher1_{};
  [[no_unique_address]] Hash2 hasher2_{};
  [[no_unique_address]] KeyEqual equal_{};
};

}  // namespace tempura
