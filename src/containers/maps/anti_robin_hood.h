#pragma once

// ============================================================================
// Anti-Robin Hood Hash Map
// ============================================================================
//
// The opposite of Robin Hood hashing. Where Robin Hood "steals from the rich
// to give to the poor," anti-Robin Hood "steals from the poor to give to the
// rich." This is intentionally pathological - included for educational
// comparison to demonstrate WHY Robin Hood works.
//
// THE ANTI-ROBIN HOOD PRINCIPLE
// -----------------------------
// During insertion, if the inserting element has traveled LESS than the
// current occupant (smaller displacement), we swap them. Elements close to
// home take priority over those already suffering.
//
// EXAMPLE:
//   Inserting element with displacement 2
//   Occupant has displacement 5
//   2 < 5 → inserter wins, occupant (already suffering) gets displaced
//   Occupant now has displacement 6 and continues probing...
//
// WHY IT'S TERRIBLE
// -----------------
// This causes displacement to compound:
//   - Elements already far from home get pushed even further
//   - A few elements accumulate enormous displacements
//   - Maximum probe length becomes O(n), worse than standard linear probing
//
// The variance in probe lengths is MAXIMIZED instead of minimized.
//
// COMPARISON
// ----------
//                    | Max Probe  | Variance  | Fairness
//   Linear Probing   | O(log n)   | Medium    | Some suffer
//   Robin Hood       | O(log log n)| Low      | Equal suffering
//   Anti-Robin Hood  | O(n)       | High      | Rich get richer, poor get poorer
//
// NO EARLY TERMINATION
// --------------------
// Unlike Robin Hood, we cannot terminate early during lookup. An element
// with small displacement doesn't tell us anything about larger-displacement
// elements that might come later.
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
class AntiRobinHoodMap {
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
    enum class State : std::uint8_t {
      kEmpty,
      kOccupied,
    };

    State state = State::kEmpty;
    size_type displacement = 0;
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

  constexpr AntiRobinHoodMap() : AntiRobinHoodMap{kMinCapacity} {}

  constexpr explicit AntiRobinHoodMap(size_type initial_capacity)
      : capacity_{std::max(initial_capacity, kMinCapacity)},
        slots_{std::make_unique<Slot[]>(capacity_)} {}

  constexpr AntiRobinHoodMap(const AntiRobinHoodMap& other)
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

  constexpr AntiRobinHoodMap(AntiRobinHoodMap&& other) noexcept
      : capacity_{other.capacity_},
        size_{other.size_},
        slots_{std::move(other.slots_)} {
    other.size_ = 0;
    other.capacity_ = 0;
  }

  constexpr auto operator=(const AntiRobinHoodMap& other) -> AntiRobinHoodMap& {
    if (this != &other) {
      AntiRobinHoodMap tmp{other};
      swap(tmp);
    }
    return *this;
  }

  constexpr auto operator=(AntiRobinHoodMap&& other) noexcept
      -> AntiRobinHoodMap& {
    if (this != &other) {
      AntiRobinHoodMap tmp{std::move(other)};
      swap(tmp);
    }
    return *this;
  }

  constexpr ~AntiRobinHoodMap() {
    if (slots_) {
      for (size_type i = 0; i < capacity_; ++i) {
        slots_[i].destroy();
      }
    }
  }

  constexpr void swap(AntiRobinHoodMap& other) noexcept {
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
  // No early termination possible - must probe until empty or found.

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
  // Use backward-shift deletion like Robin Hood.

  constexpr auto erase(const Key& key) -> bool {
    size_type idx = findSlot(key);
    if (idx == capacity_) {
      return false;
    }

    slots_[idx].destroy();
    --size_;

    size_type hole = idx;
    size_type next = (hole + 1) % capacity_;

    while (slots_[next].isOccupied() && slots_[next].displacement > 0) {
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

    constexpr Iterator(AntiRobinHoodMap* map, size_type idx)
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

    AntiRobinHoodMap* map_;
    size_type idx_;
  };

  class ConstIterator {
   public:
    using difference_type = std::ptrdiff_t;
    using value_type = std::pair<const Key, Value>;
    using pointer = const value_type*;
    using reference = const value_type&;

    constexpr ConstIterator(const AntiRobinHoodMap* map, size_type idx)
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

    const AntiRobinHoodMap* map_;
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

  // No early termination - must probe until empty or found
  constexpr auto findSlot(const Key& key) const -> size_type {
    size_type ideal = hashIndex(key);
    size_type idx = ideal;
    size_type probes = 0;

    while (probes < capacity_) {
      const Slot& slot = slots_[idx];

      if (slot.isEmpty()) {
        return capacity_;
      }

      if (equal_(slot.data()->first, key)) {
        return idx;
      }

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

    size_type ideal = hashIndex(key);
    size_type idx = ideal;
    size_type disp = 0;

    Key current_key = key;
    Value current_value{};
    bool is_original = true;

    while (disp < capacity_) {
      Slot& slot = slots_[idx];

      if (slot.isEmpty()) {
        slot.construct(disp, std::move(current_key), std::move(current_value));
        ++size_;
        if (is_original) {
          return {idx, true};
        }
        return {findSlot(key), true};
      }

      if (is_original && equal_(slot.data()->first, current_key)) {
        return {idx, false};
      }

      // ANTI-Robin Hood: swap if we have SMALLER displacement (rich take from poor)
      if (disp < slot.displacement) {
        std::swap(current_key, slot.mutableData()->first);
        std::swap(current_value, slot.mutableData()->second);
        std::swap(disp, slot.displacement);
        is_original = false;
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

      // ANTI-Robin Hood: swap if we have SMALLER displacement
      if (disp < slot.displacement) {
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
