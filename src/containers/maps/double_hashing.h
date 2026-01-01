#pragma once

// ============================================================================
// Double Hashing Hash Map
// ============================================================================
//
// A hash map using "open addressing" with "double hashing" as its collision
// resolution strategy. Double hashing uses TWO hash functions to compute the
// probe sequence, eliminating the clustering problems of simpler methods.
//
// THE BASIC IDEA
// --------------
// Unlike linear probing which steps by 1 on each collision:
//   slot, slot+1, slot+2, slot+3, ...
//
// Double hashing uses a key-dependent step size computed by a second hash:
//   h1(key), h1(key)+h2(key), h1(key)+2*h2(key), h1(key)+3*h2(key), ...
//
// The formula is: (h1(key) + i * h2(key)) % capacity, where i = 0, 1, 2, ...
//
// WHY TWO HASH FUNCTIONS?
// -----------------------
// Linear probing suffers from "primary clustering": keys that hash to nearby
// positions follow overlapping probe sequences, creating long chains.
//
// Quadratic probing (slot+1, slot+4, slot+9, ...) reduces primary clustering
// but creates "secondary clustering": all keys with the same h1 value follow
// the same probe sequence.
//
// Double hashing eliminates BOTH forms of clustering: even if two keys have
// the same h1(key), they will (likely) have different h2(key) values, giving
// them completely different probe sequences.
//
// EXAMPLE: Inserting keys A, B, C with h1(A)=5, h1(B)=5, h1(C)=5
//          and h2(A)=3, h2(B)=2, h2(C)=5 into capacity=7
//
//   Insert A: slot 5 empty → store at [5]
//   Insert B: slot 5 occupied → try (5+2)%7=0 → store at [0]
//   Insert C: slot 5 occupied → try (5+5)%7=3 → store at [3]
//
//   Array: [ B | ∅ | ∅ | C | ∅ | A | ∅ ]
//           [0] [1] [2] [3] [4] [5] [6]
//
//   Compare with linear probing where all three would cluster at [5][6][0].
//
// THE h2(key) REQUIREMENTS
// ------------------------
// Critical: h2(key) must satisfy two constraints:
//
//   1. h2(key) ≠ 0: Otherwise we'd check slot h1(key) forever (infinite loop)
//   2. h2(key) must be coprime with capacity: Otherwise we won't visit all
//      slots before repeating. Example: h2=4, capacity=8 gives cycle:
//      0→4→0→4→... (only visits 2 of 8 slots)
//
// Solution: Use PRIME table sizes. Every h2 value in [1, capacity-1] is
// automatically coprime with a prime capacity.
//
// Common h2 formula: PRIME - (key % PRIME), where PRIME < capacity
// This guarantees h2 ∈ [1, PRIME], avoiding zero.
//
// PROS AND CONS
// -------------
// Pros:
//   ✓ Eliminates both primary and secondary clustering
//   ✓ More uniform probe distribution than linear/quadratic probing
//   ✓ Better worst-case performance under adversarial inputs
//   ✓ Theoretical probe count: 1/(1-α) for unsuccessful search (α = load factor)
//
// Cons:
//   ✗ Requires two hash function evaluations per probe (more computation)
//   ✗ Needs good, independent hash functions to realize benefits
//   ✗ Less cache-friendly than linear probing (non-sequential access)
//   ✗ Requires prime table sizes for correctness guarantees
//
// WHEN TO USE DOUBLE HASHING
// --------------------------
// - When you need predictable worst-case performance
// - When keys may have patterns that cause clustering with simpler probing
// - When table utilization is high (α > 0.6)
// - When cache locality is less important than avoiding pathological cases
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
// Default Second Hash Function
// ============================================================================
// For keys convertible to size_t, we use the formula:
//   h2(key) = PRIME - (hash(key) % PRIME)
//
// where PRIME is a constant smaller than any reasonable capacity.
// This guarantees h2 ∈ [1, PRIME], never zero.

template <typename Key>
struct DefaultHash2 {
  // Use a prime smaller than our minimum capacity
  static constexpr std::size_t kPrime = 7;

  constexpr auto operator()(const Key& key) const -> std::size_t {
    // Apply std::hash first, then compute the secondary step
    std::size_t h = std::hash<Key>{}(key);
    return kPrime - (h % kPrime);  // Result in [1, kPrime]
  }
};

// ============================================================================
// Prime Table Sizes
// ============================================================================
// We use prime capacities to ensure h2(key) is always coprime with capacity.
// Each size is roughly double the previous for amortized O(1) resize.

constexpr std::size_t kPrimeSizes[] = {
    11,        23,        47,        97,         193,        389,
    769,       1543,      3079,      6151,       12289,      24593,
    49157,     98317,     196613,    393241,     786433,     1572869,
    3145739,   6291469,   12582917,  25165843,   50331653,   100663319,
    201326611, 402653189, 805306457, 1610612741,
};

constexpr std::size_t kNumPrimeSizes =
    sizeof(kPrimeSizes) / sizeof(kPrimeSizes[0]);

// Find the smallest prime >= n
constexpr auto nextPrime(std::size_t n) -> std::size_t {
  for (std::size_t i = 0; i < kNumPrimeSizes; ++i) {
    if (kPrimeSizes[i] >= n) {
      return kPrimeSizes[i];
    }
  }
  // Fallback for very large tables (unlikely in practice)
  return kPrimeSizes[kNumPrimeSizes - 1];
}

// ============================================================================
// DoubleHashingMap
// ============================================================================

template <typename Key, typename Value, Hasher<Key> Hash1 = std::hash<Key>,
          Hasher<Key> Hash2 = DefaultHash2<Key>,
          typename KeyEqual = std::equal_to<Key>>
class DoubleHashingMap {
 public:
  // --------------------------------------------------------------------------
  // Type Aliases
  // --------------------------------------------------------------------------

  using key_type = Key;
  using mapped_type = Value;
  using value_type = std::pair<const Key, Value>;
  using size_type = std::size_t;
  using hasher = Hash1;
  using second_hasher = Hash2;
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
  // This avoids floating-point math in the hot path while achieving ~0.7
  // threshold.
  static constexpr size_type kLoadFactorNumerator = 7;
  static constexpr size_type kLoadFactorDenominator = 10;

  // Minimum capacity (smallest prime in our list)
  static constexpr size_type kMinCapacity = kPrimeSizes[0];  // 11

  // --------------------------------------------------------------------------
  // Constructors
  // --------------------------------------------------------------------------

  constexpr DoubleHashingMap() : DoubleHashingMap{kMinCapacity} {}

  constexpr explicit DoubleHashingMap(size_type initial_capacity)
      : capacity_{nextPrime(std::max(initial_capacity, kMinCapacity))},
        slots_{std::make_unique<Slot[]>(capacity_)} {}

  // Copy constructor: deep copy all occupied slots
  constexpr DoubleHashingMap(const DoubleHashingMap& other)
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
  constexpr DoubleHashingMap(DoubleHashingMap&& other) noexcept
      : capacity_{other.capacity_},
        size_{other.size_},
        tombstone_count_{other.tombstone_count_},
        slots_{std::move(other.slots_)} {
    other.size_ = 0;
    other.capacity_ = 0;
    other.tombstone_count_ = 0;
  }

  // Copy assignment using copy-and-swap for exception safety
  constexpr auto operator=(const DoubleHashingMap& other) -> DoubleHashingMap& {
    if (this != &other) {
      DoubleHashingMap tmp{other};
      swap(tmp);
    }
    return *this;
  }

  // Move assignment using swap
  constexpr auto operator=(DoubleHashingMap&& other) noexcept
      -> DoubleHashingMap& {
    if (this != &other) {
      DoubleHashingMap tmp{std::move(other)};
      swap(tmp);
    }
    return *this;
  }

  constexpr ~DoubleHashingMap() {
    if (slots_) {
      for (size_type i = 0; i < capacity_; ++i) {
        slots_[i].destroy();
      }
    }
  }

  // Swap two maps (used by copy-and-swap idiom)
  constexpr void swap(DoubleHashingMap& other) noexcept {
    std::swap(capacity_, other.capacity_);
    std::swap(size_, other.size_);
    std::swap(tombstone_count_, other.tombstone_count_);
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
    return static_cast<double>(size_ + tombstone_count_) /
           static_cast<double>(capacity_);
  }

  // --------------------------------------------------------------------------
  // Lookup
  // --------------------------------------------------------------------------

  // Find a key and return pointer to its value, or nullptr if not found.
  //
  // Algorithm:
  //   1. Compute h1(key) for starting slot, h2(key) for step size
  //   2. Probe with double hashing: idx = (h1 + i*h2) % capacity
  //   3. Stop when: empty slot (not found) or key matches (found)
  //   4. Skip: tombstones and other occupied slots

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
  //   2. Find insertion point using double hashing probe
  //   3. Insert at first empty/tombstone slot after verifying key doesn't exist

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
  // We use tombstones rather than actually clearing the slot to preserve
  // probe chain integrity. Tombstones are cleaned up during resize.

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
  //
  // The Slot::data() method returns pair<const Key, Value>* directly via the
  // union trick (see Slot documentation), so no casts are needed here.

  class Iterator {
   public:
    using difference_type = std::ptrdiff_t;
    using value_type = std::pair<const Key, Value>;
    using pointer = value_type*;
    using reference = value_type&;

    constexpr Iterator(DoubleHashingMap* map, size_type idx)
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

    DoubleHashingMap* map_;
    size_type idx_;
  };

  class ConstIterator {
   public:
    using difference_type = std::ptrdiff_t;
    using value_type = std::pair<const Key, Value>;
    using pointer = const value_type*;
    using reference = const value_type&;

    constexpr ConstIterator(const DoubleHashingMap* map, size_type idx)
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

    const DoubleHashingMap* map_;
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

  // Check if resize is needed using integer arithmetic.
  // Equivalent to: (size + tombstones) / capacity >= 0.7
  constexpr auto needsResize() const -> bool {
    return (size_ + tombstone_count_) * kLoadFactorDenominator >=
           capacity_ * kLoadFactorNumerator;
  }

  // Compute the primary hash index (starting position)
  constexpr auto hashIndex1(const Key& key) const -> size_type {
    return hasher1_(key) % capacity_;
  }

  // Compute the secondary hash (step size)
  // Must be in [1, capacity-1] to avoid infinite loops and ensure full coverage
  constexpr auto hashIndex2(const Key& key) const -> size_type {
    size_type h2 = hasher2_(key) % capacity_;
    // Ensure h2 is non-zero - zero step size would cause infinite loop
    // Check AFTER modulo: h2 could equal capacity_ before, giving 0 after
    if (h2 == 0) {
      h2 = 1;
    }
    return h2;
  }

  // Find the slot containing a key, or capacity_ if not found.
  //
  // This is the core double hashing probe loop:
  //   - Start at h1(key) % capacity
  //   - Probe: idx = (h1 + i * h2) % capacity
  //   - Stop when: empty slot (not found) or key matches (found)
  //   - Skip: tombstones and other occupied slots

  constexpr auto findSlot(const Key& key) const -> size_type {
    size_type h1 = hashIndex1(key);
    size_type h2 = hashIndex2(key);
    size_type idx = h1;
    size_type probes = 0;

    // With prime capacity and non-zero h2, we visit all slots before repeating
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

      // Collision or tombstone: continue double-hashing probe
      idx = (idx + h2) % capacity_;
      ++probes;
    }

    return capacity_;  // Table is full (shouldn't happen with proper resizing)
  }

  // Find slot for insertion, potentially resizing first.
  // Returns {slot_index, was_new_insertion}

  constexpr auto insertSlot(const Key& key) -> std::pair<size_type, bool> {
    // Check if resize is needed
    if (needsResize()) {
      constexpr size_type kMaxCapacity = size_type{1} << 60;
      size_type new_cap = capacity_ * 2;
      // Assert on overflow or exceeding max - silent failure is unacceptable
      assert(new_cap > capacity_ && "insertSlot: capacity overflow");
      assert(new_cap <= kMaxCapacity && "insertSlot: capacity exceeds maximum");
      resize(new_cap);
    }

    size_type h1 = hashIndex1(key);
    size_type h2 = hashIndex2(key);
    size_type idx = h1;
    size_type first_tombstone = capacity_;  // Track first tombstone for reuse
    size_type probes = 0;

    while (probes < capacity_) {
      Slot& slot = slots_[idx];

      if (slot.isEmpty()) {
        // Empty slot: insert here (or at earlier tombstone)
        size_type insert_idx =
            (first_tombstone != capacity_) ? first_tombstone : idx;

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

      // Double hashing probe
      idx = (idx + h2) % capacity_;
      ++probes;
    }

    // Should never reach here with proper load factor management
    assert(false &&
           "insertSlot: table full despite load factor check - bad hash?");
    std::unreachable();
  }

  // Resize the table to next prime >= new_capacity.
  //
  // This is where we clean up tombstones:
  //   1. Allocate new prime-sized array (all slots empty)
  //   2. Re-insert only occupied slots (skip tombstones)
  //   3. Delete old array
  //
  // After resize, tombstone_count = 0 and all slots are either empty or
  // occupied.

  constexpr void resize(size_type new_capacity) {
    new_capacity = nextPrime(std::max(new_capacity, kMinCapacity));

    // Sanity check: capacity must be reasonable
    constexpr size_type kMaxCapacity = size_type{1} << 60;
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
    size_type h1 = hashIndex1(key);
    size_type h2 = hashIndex2(key);
    size_type idx = h1;

    for (size_type probes = 0; probes < capacity_; ++probes) {
      Slot& slot = slots_[idx];
      if (slot.isEmpty()) {
        slot.construct(key, Value{});
        ++size_;
        return idx;
      }
      idx = (idx + h2) % capacity_;
    }

    std::unreachable();
  }

  // --------------------------------------------------------------------------
  // Member Variables
  // --------------------------------------------------------------------------

  size_type capacity_ = kMinCapacity;       // Number of slots (always prime)
  size_type size_ = 0;                      // Number of occupied slots
  size_type tombstone_count_ = 0;           // Number of tombstone slots
  std::unique_ptr<Slot[]> slots_ = nullptr; // The actual storage

  [[no_unique_address]] Hash1 hasher1_{};     // Primary hash function
  [[no_unique_address]] Hash2 hasher2_{};     // Secondary hash function (step)
  [[no_unique_address]] KeyEqual equal_{};    // Key equality function object
};

}  // namespace tempura
