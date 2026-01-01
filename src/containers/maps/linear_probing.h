#pragma once

// ============================================================================
// Linear Probing Hash Map
// ============================================================================
//
// A hash map is an associative container that maps keys to values. The key
// insight is that we can use a hash function to convert keys into array
// indices, giving us O(1) average-case lookup, insertion, and deletion.
//
// This implementation uses "open addressing" with "linear probing" as its
// collision resolution strategy. Let's understand what this means:
//
// THE BASIC IDEA
// --------------
// 1. We maintain an array of "slots" (buckets)
// 2. To insert key K: compute hash(K) % capacity to get the target slot
// 3. If that slot is empty, store the key-value pair there
// 4. If occupied (a "collision"), probe linearly: try slot+1, slot+2, etc.
// 5. To find a key: start at hash(K) % capacity, probe until found or empty
//
// EXAMPLE: Inserting keys with hash values 5, 12, 5, 6 into capacity=7
//
//   Insert hash=5: slot 5 empty → store at [5]
//   Insert hash=12: 12%7=5, slot 5 occupied → try [6] → store at [6]
//   Insert hash=5: slot 5 occupied → try [6] occupied → try [0] → store at [0]
//   Insert hash=6: slot 6 occupied → try [0] occupied → try [1] → store at [1]
//
//   Array: [ K3 | K4 | ∅ | ∅ | ∅ | K1 | K2 ]
//           [0]  [1]  [2] [3] [4] [5]  [6]
//
// WHY LINEAR PROBING?
// -------------------
// Pros:
//   - Cache-friendly: sequential memory access during probing
//   - Simple to implement and reason about
//   - No extra memory for linked lists (unlike chaining)
//
// Cons:
//   - "Primary clustering": long runs of occupied slots form, degrading perf
//   - Deletion is tricky (we use tombstones, explained below)
//   - Performance degrades rapidly as load factor approaches 1.0
//
// LOAD FACTOR
// -----------
// Load factor α = size / capacity. It measures how "full" the table is.
//
//   α = 0.0: Empty table, O(1) operations
//   α = 0.5: ~1.5 probes on average for successful search
//   α = 0.7: ~2.2 probes on average (our resize threshold)
//   α = 0.9: ~5.5 probes on average (getting slow)
//   α → 1.0: Probe count → ∞ (catastrophic)
//
// We resize when α > 0.7 to maintain good performance.
//
// DELETION AND TOMBSTONES
// -----------------------
// Naive deletion breaks the probe chain. Consider:
//
//   [A][B][C][ ]  where B was inserted after A due to collision
//
// If we delete A by marking it empty:
//
//   [ ][B][C][ ]
//
// Now searching for B starts at A's old slot, sees empty, and wrongly
// concludes B doesn't exist! Solution: mark deleted slots as "tombstones":
//
//   [†][B][C][ ]
//
// Tombstones are skipped during search but can be reused during insertion.
// Too many tombstones degrade performance, so we clean them during resize.
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
// LinearProbingMap
// ============================================================================

template <typename Key, typename Value, Hasher<Key> Hash = std::hash<Key>,
          typename KeyEqual = std::equal_to<Key>>
class LinearProbingMap {
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
  // Each slot stores its state and the key-value data. We use ManualLifetime
  // to defer construction, and MapStorage to handle the const key problem
  // (see map_storage.h for details on the union trick).

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

  // Load factor threshold: resize when (size + tombstones) * 10 >= capacity * 7
  // This avoids floating-point math in the hot path while achieving ~0.7 threshold.
  static constexpr size_type kLoadFactorNumerator = 7;
  static constexpr size_type kLoadFactorDenominator = 10;

  // Minimum capacity. Must be > 0. Small tables have high overhead anyway.
  static constexpr size_type kMinCapacity = 8;

  // --------------------------------------------------------------------------
  // Constructors
  // --------------------------------------------------------------------------

  constexpr LinearProbingMap() : LinearProbingMap{kMinCapacity} {}

  constexpr explicit LinearProbingMap(size_type initial_capacity)
      : capacity_{std::max(initial_capacity, kMinCapacity)},
        slots_{std::make_unique<Slot[]>(capacity_)} {}

  // Copy constructor: deep copy all occupied slots
  constexpr LinearProbingMap(const LinearProbingMap& other)
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
  constexpr LinearProbingMap(LinearProbingMap&& other) noexcept
      : capacity_{other.capacity_},
        size_{other.size_},
        tombstone_count_{other.tombstone_count_},
        slots_{std::move(other.slots_)} {
    other.size_ = 0;
    other.capacity_ = 0;
    other.tombstone_count_ = 0;
  }

  // Copy assignment using copy-and-swap for exception safety
  constexpr auto operator=(const LinearProbingMap& other) -> LinearProbingMap& {
    if (this != &other) {
      LinearProbingMap tmp{other};
      swap(tmp);
    }
    return *this;
  }

  // Move assignment using swap
  constexpr auto operator=(LinearProbingMap&& other) noexcept
      -> LinearProbingMap& {
    if (this != &other) {
      LinearProbingMap tmp{std::move(other)};
      swap(tmp);
    }
    return *this;
  }

  constexpr ~LinearProbingMap() {
    if (slots_) {
      for (size_type i = 0; i < capacity_; ++i) {
        slots_[i].destroy();
      }
    }
  }

  // Swap two maps (used by copy-and-swap idiom)
  constexpr void swap(LinearProbingMap& other) noexcept {
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

  // Find a key and return pointer to its value, or nullptr if not found.
  //
  // Algorithm:
  //   1. Compute starting slot: hash(key) % capacity
  //   2. Probe linearly until we find:
  //      - The key itself → return pointer to value
  //      - An empty slot → key doesn't exist, return nullptr
  //      - A tombstone → keep probing (deleted key, chain continues)
  //      - Different key → keep probing (collision)

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

  // Operator[] with auto-insertion of default value (like std::map)
  constexpr auto operator[](const Key& key) -> Value& {
    auto [idx, inserted] = insertSlot(key);
    return slots_[idx].data()->second;
  }

  // --------------------------------------------------------------------------
  // Insertion
  // --------------------------------------------------------------------------

  // Insert a key-value pair. Returns {pointer_to_value, true} if inserted,
  // {pointer_to_value, false} if key already existed.
  //
  // Algorithm:
  //   1. Check load factor; resize if necessary
  //   2. Find insertion point:
  //      - If key exists, return existing slot
  //      - Otherwise, find first empty or tombstone slot
  //   3. Insert at that slot

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

  // Insert or update: always sets the value, returns true if new insertion
  constexpr auto insertOrAssign(const Key& key, Value value) -> bool {
    auto [idx, inserted] = insertSlot(key);
    slots_[idx].data()->second = std::move(value);
    return inserted;
  }

  // --------------------------------------------------------------------------
  // Deletion
  // --------------------------------------------------------------------------

  // Erase a key. Returns true if the key was found and removed.
  //
  // We use tombstones rather than actually clearing the slot:
  //   - Mark state as kTombstone (destroys the data)
  //   - Increment tombstone count
  //
  // Tombstones preserve probe chain integrity but degrade performance over
  // time. We clean them up during resize operations.

  constexpr auto erase(const Key& key) -> bool {
    size_type idx = findSlot(key);
    if (idx == capacity_) {
      return false;  // Key not found
    }

    slots_[idx].markTombstone();
    --size_;
    ++tombstone_count_;

    return true;
  }

  // Clear all elements
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
  // We provide a forward iterator that skips empty and tombstone slots.
  // The Slot::data() method returns pair<const Key, Value>* directly via the
  // union trick (see Slot documentation), so no casts are needed here.

  class Iterator {
   public:
    using difference_type = std::ptrdiff_t;
    using value_type = std::pair<const Key, Value>;
    using pointer = value_type*;
    using reference = value_type&;

    constexpr Iterator(LinearProbingMap* map, size_type idx)
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

    LinearProbingMap* map_;
    size_type idx_;
  };

  class ConstIterator {
   public:
    using difference_type = std::ptrdiff_t;
    using value_type = std::pair<const Key, Value>;
    using pointer = const value_type*;
    using reference = const value_type&;

    constexpr ConstIterator(const LinearProbingMap* map, size_type idx)
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

    const LinearProbingMap* map_;
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

  // Check if resize is needed using integer arithmetic.
  // Equivalent to: (size + tombstones) / capacity >= 0.7
  // Rewritten as: (size + tombstones) * 10 >= capacity * 7
  constexpr auto needsResize() const -> bool {
    return (size_ + tombstone_count_) * kLoadFactorDenominator >=
           capacity_ * kLoadFactorNumerator;
  }

  // Compute the starting slot index for a key
  constexpr auto hashIndex(const Key& key) const -> size_type {
    return hasher_(key) % capacity_;
  }

  // Find the slot containing a key, or capacity_ if not found.
  //
  // This is the core linear probing loop:
  //   - Start at hash(key) % capacity
  //   - Probe: idx = (idx + 1) % capacity (wraps around)
  //   - Stop when: empty slot (not found) or key matches (found)
  //   - Skip: tombstones and other occupied slots

  constexpr auto findSlot(const Key& key) const -> size_type {
    size_type idx = hashIndex(key);
    size_type probes = 0;

    // We must stop after capacity_ probes to avoid infinite loop on full table
    while (probes < capacity_) {
      const Slot& slot = slots_[idx];

      if (slot.isEmpty()) {
        // Empty slot: key definitely not in table
        return capacity_;
      }

      if (slot.isOccupied() && equal_(slot.data()->first, key)) {
        // Found the key
        return idx;
      }

      // Collision or tombstone: continue probing
      idx = (idx + 1) % capacity_;
      ++probes;
    }

    return capacity_;  // Table is full (shouldn't happen with proper resizing)
  }

  // Find slot for insertion, potentially resizing first.
  // Returns {slot_index, was_new_insertion}

  constexpr auto insertSlot(const Key& key) -> std::pair<size_type, bool> {
    // Check if resize is needed using integer math
    if (needsResize()) {
      constexpr size_type kMaxCapacity = size_type{1} << 60;
      size_type new_cap = capacity_ * 2;
      // Assert on overflow or exceeding max - silent failure is unacceptable
      assert(new_cap > capacity_ && "insertSlot: capacity overflow");
      assert(new_cap <= kMaxCapacity && "insertSlot: capacity exceeds maximum");
      resize(new_cap);
    }

    size_type idx = hashIndex(key);
    size_type first_tombstone = capacity_;  // Track first tombstone for reuse
    size_type probes = 0;

    // Bounded loop for safety (shouldn't hit limit with proper load factor)
    while (probes < capacity_) {
      Slot& slot = slots_[idx];

      if (slot.isEmpty()) {
        // Empty slot: insert here (or at earlier tombstone)
        size_type insert_idx = (first_tombstone != capacity_) ? first_tombstone : idx;

        if (first_tombstone != capacity_) {
          // Reusing a tombstone
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

    // Should never reach here - load factor check guarantees empty slots exist
    assert(false && "insertSlot: table full despite load factor check - bad hash?");
    std::unreachable();
  }

  // Resize the table to new_capacity.
  //
  // This is where we clean up tombstones:
  //   1. Allocate new, larger array (all slots empty)
  //   2. Re-insert only occupied slots (skip tombstones)
  //   3. Delete old array
  //
  // After resize, tombstone_count = 0 and all slots are either empty or
  // occupied. This "garbage collection" is essential for maintaining
  // performance when there are many deletions.

  constexpr void resize(size_type new_capacity) {
    new_capacity = std::max(new_capacity, kMinCapacity);

    // Sanity check: capacity must be reasonable
    // (also helps static analyzers understand bounds)
    constexpr size_type kMaxCapacity = size_type{1} << 60;  // ~1 exabyte
    assert(new_capacity <= kMaxCapacity && "resize: capacity exceeds maximum");

    auto old_slots = std::move(slots_);
    size_type old_capacity = capacity_;

    // Allocate new array
    capacity_ = new_capacity;
    slots_ = std::make_unique<Slot[]>(capacity_);
    size_ = 0;
    tombstone_count_ = 0;

    // Re-insert all occupied elements (no resize check needed - we just grew)
    // Use mutableData() to allow moving keys efficiently
    for (size_type i = 0; i < old_capacity; ++i) {
      if (old_slots[i].isOccupied()) {
        auto& [key, value] = *old_slots[i].mutableData();
        size_type idx = insertSlotNoResize(key);
        slots_[idx].mutableData()->second = std::move(value);
      }
    }

    // Clean up old slots (destroy ManualLifetime contents)
    for (size_type i = 0; i < old_capacity; ++i) {
      old_slots[i].destroy();
    }
    // old_slots automatically freed when unique_ptr goes out of scope
  }

  // Insert without resize check - used during resize when we know there's room
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

  size_type capacity_ = kMinCapacity;       // Number of slots in the array
  size_type size_ = 0;                      // Number of occupied slots
  size_type tombstone_count_ = 0;           // Number of tombstone slots
  std::unique_ptr<Slot[]> slots_ = nullptr; // The actual storage

  [[no_unique_address]] Hash hasher_{};       // Hash function object
  [[no_unique_address]] KeyEqual equal_{};    // Key equality function object
};

}  // namespace tempura
