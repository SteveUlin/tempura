#pragma once

// ============================================================================
// Coalesced Hash Map
// ============================================================================
//
// Coalesced hashing is a hybrid collision resolution strategy combining the
// best aspects of separate chaining and open addressing. Instead of external
// linked lists, collision chains are stored WITHIN the hash table itself.
//
// THE COALESCED HASHING PRINCIPLE
// --------------------------------
// Each slot contains:
//   1. State (empty/occupied)
//   2. Key-value pair storage
//   3. "Next" pointer (index into the same table)
//
// When a collision occurs, we find an empty slot elsewhere in the table and
// link it into the chain using the next pointer. All storage is contiguous -
// no external allocation.
//
// EXAMPLE: Insert keys with hash indices 2, 2, 5, 2
//
//   Initial table: [_, _, _, _, _, _, _, _]  (8 slots)
//
//   Insert A (h=2): Table[2] = A, next=-1
//   Table: [_, _, A→∅, _, _, _, _, _]
//
//   Insert B (h=2): Collision! Find empty slot (say 7), link from A
//   Table: [_, _, A→7, _, _, _, _, B→∅]
//
//   Insert C (h=5): Table[5] = C, next=-1
//   Table: [_, _, A→7, _, _, C→∅, _, B→∅]
//
//   Insert D (h=2): Another collision! Find empty slot (say 6), link from B
//   Table: [_, _, A→7, _, _, C→∅, D→∅, B→6]
//
//   To find a key with h=2: Check slot 2 → check slot 7 → check slot 6 → done
//
// ADDRESS REGION vs CELLAR
// -------------------------
// Standard coalesced hashing divides the table into two regions:
//   - Address region [0, address_max): where keys naturally hash to
//   - Cellar [address_max, capacity): overflow area for collision chains
//
// The cellar acts as a "stack" of available overflow slots, allocated from
// the end backward. Literature suggests 86% address / 14% cellar is optimal.
//
// WHY THIS WORKS
// --------------
// Separate chaining:
//   ✓ No clustering, independent chains
//   ✗ External allocation overhead, poor cache locality
//
// Open addressing (linear probing):
//   ✓ Contiguous storage, excellent cache performance
//   ✗ Primary clustering, sensitive to load factor
//
// Coalesced hashing:
//   ✓ Contiguous storage (cache-friendly)
//   ✓ Chains don't form clusters (collisions don't block other keys)
//   ✓ Higher load factors than open addressing (can use > 80%)
//   ✗ Slightly more complex than linear probing
//
// PERFORMANCE CHARACTERISTICS
// ----------------------------
// Average probe length at 90% load factor:
//   Linear probing: ~50 probes
//   Coalesced hashing: ~2.5 probes
//
// With 86/14 address/cellar split at 80% load:
//   Average successful search: ~1.5 probes
//   Average unsuccessful search: ~2.0 probes
//
// LATE INSERTION (LICH)
// ---------------------
// When collision occurs at slot i:
//   1. Follow chain from i to the end
//   2. Allocate new slot from cellar
//   3. Link new slot at END of chain
//
// This keeps chains in hash order (all elements with h=i before h=j if i<j),
// which improves cache locality during lookups.
//
// DELETION STRATEGY
// -----------------
// We use TOMBSTONES rather than chain relinking. Relinking is complex:
//   - Must find predecessor in chain (no backward pointers)
//   - Must handle multiple cases (head vs interior vs tail)
//   - Can't reuse slot immediately (breaks chains)
//
// Tombstones are simpler:
//   - Mark slot as deleted
//   - Continue following chain during lookup
//   - Reuse tombstones during insertion
//
// REFERENCES
// ----------
// - Vitter & Chen, "Design and Analysis of Coalesced Hashing" (1987)
// - Knuth, "The Art of Computer Programming", Vol 3, Section 6.4
// - Williams, "A modification to coalesced hashing" (1979) [cellar idea]
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
class CoalescedMap {
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
  // Each slot needs state, storage, AND a "next" index for chaining.
  // We use a sentinel value (capacity) to indicate end-of-chain.

  struct Slot {
    enum class State : std::uint8_t {
      kEmpty,
      kOccupied,
      kTombstone,
    };

    State state = State::kEmpty;
    size_type next = 0;  // Index of next slot in chain (or sentinel)
    ManualLifetime<MapStorage<Key, Value>> storage;

    auto isEmpty() const -> bool { return state == State::kEmpty; }
    auto isOccupied() const -> bool { return state == State::kOccupied; }
    auto isTombstone() const -> bool { return state == State::kTombstone; }
    auto isAvailable() const -> bool {
      return state == State::kEmpty || state == State::kTombstone;
    }

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
    void construct(size_type next_idx, Args&&... args) {
      storage.construct();
      std::construct_at(&storage.get().mutable_pair,
                        std::forward<Args>(args)...);
      next = next_idx;
      state = State::kOccupied;
    }

    void destroy() {
      if (state == State::kOccupied) {
        std::destroy_at(&storage.get().mutable_pair);
        storage.destruct();
      }
      state = State::kEmpty;
      next = 0;
    }

    void markTombstone() {
      if (state == State::kOccupied) {
        std::destroy_at(&storage.get().mutable_pair);
        storage.destruct();
      }
      state = State::kTombstone;
      // Keep next pointer intact - chains must remain connected
    }
  };

  // --------------------------------------------------------------------------
  // Constants
  // --------------------------------------------------------------------------

  // Coalesced hashing works well at high load factors (80-90%)
  static constexpr size_type kLoadFactorNumerator = 8;
  static constexpr size_type kLoadFactorDenominator = 10;
  static constexpr size_type kMinCapacity = 16;

  // Address region is 86% of table, cellar is 14% (Vitter & Chen)
  static constexpr size_type kAddressNumerator = 86;
  static constexpr size_type kAddressDenominator = 100;

  // Sentinel value for end-of-chain
  static constexpr size_type kEndOfChain = static_cast<size_type>(-1);

  // --------------------------------------------------------------------------
  // Constructors
  // --------------------------------------------------------------------------

  constexpr CoalescedMap() : CoalescedMap{kMinCapacity} {}

  constexpr explicit CoalescedMap(size_type initial_capacity)
      : capacity_{std::max(initial_capacity, kMinCapacity)},
        address_limit_{(capacity_ * kAddressNumerator) / kAddressDenominator},
        cellar_ptr_{capacity_ - 1},
        slots_{std::make_unique<Slot[]>(capacity_)} {
    // Initialize all next pointers to sentinel
    for (size_type i = 0; i < capacity_; ++i) {
      slots_[i].next = kEndOfChain;
    }
  }

  constexpr CoalescedMap(const CoalescedMap& other)
      : capacity_{other.capacity_},
        address_limit_{other.address_limit_},
        size_{other.size_},
        cellar_ptr_{other.cellar_ptr_},
        slots_{std::make_unique<Slot[]>(capacity_)} {
    for (size_type i = 0; i < capacity_; ++i) {
      if (other.slots_[i].isOccupied()) {
        slots_[i].construct(other.slots_[i].next, *other.slots_[i].data());
      } else {
        slots_[i].state = other.slots_[i].state;
        slots_[i].next = other.slots_[i].next;
      }
    }
  }

  constexpr CoalescedMap(CoalescedMap&& other) noexcept
      : capacity_{other.capacity_},
        address_limit_{other.address_limit_},
        size_{other.size_},
        cellar_ptr_{other.cellar_ptr_},
        slots_{std::move(other.slots_)} {
    other.size_ = 0;
    other.capacity_ = 0;
    other.address_limit_ = 0;
    other.cellar_ptr_ = 0;
  }

  constexpr auto operator=(const CoalescedMap& other) -> CoalescedMap& {
    if (this != &other) {
      CoalescedMap tmp{other};
      swap(tmp);
    }
    return *this;
  }

  constexpr auto operator=(CoalescedMap&& other) noexcept -> CoalescedMap& {
    if (this != &other) {
      CoalescedMap tmp{std::move(other)};
      swap(tmp);
    }
    return *this;
  }

  constexpr ~CoalescedMap() {
    if (slots_) {
      for (size_type i = 0; i < capacity_; ++i) {
        slots_[i].destroy();
      }
    }
  }

  constexpr void swap(CoalescedMap& other) noexcept {
    std::swap(capacity_, other.capacity_);
    std::swap(address_limit_, other.address_limit_);
    std::swap(size_, other.size_);
    std::swap(cellar_ptr_, other.cellar_ptr_);
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
  // Follow the chain from the hash index until we find the key or hit
  // end-of-chain. Skip over tombstones.

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
  // Mark slot as tombstone. Keep next pointer intact to preserve chains.

  constexpr auto erase(const Key& key) -> bool {
    size_type idx = findSlot(key);
    if (idx == capacity_) {
      return false;
    }

    slots_[idx].markTombstone();
    --size_;
    return true;
  }

  constexpr void clear() {
    for (size_type i = 0; i < capacity_; ++i) {
      slots_[i].destroy();
      slots_[i].next = kEndOfChain;
    }
    size_ = 0;
    cellar_ptr_ = capacity_ - 1;
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

    constexpr Iterator(CoalescedMap* map, size_type idx)
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

    CoalescedMap* map_;
    size_type idx_;
  };

  class ConstIterator {
   public:
    using difference_type = std::ptrdiff_t;
    using value_type = std::pair<const Key, Value>;
    using pointer = const value_type*;
    using reference = const value_type&;

    constexpr ConstIterator(const CoalescedMap* map, size_type idx)
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

    const CoalescedMap* map_;
    size_type idx_;
  };

  constexpr auto begin() -> Iterator { return {this, 0}; }
  constexpr auto end() -> Iterator { return {this, capacity_}; }
  constexpr auto begin() const -> ConstIterator { return {this, 0}; }
  constexpr auto end() const -> ConstIterator { return {this, capacity_}; }

  // --------------------------------------------------------------------------
  // Analysis (for understanding chain behavior)
  // --------------------------------------------------------------------------

  constexpr auto maxChainLength() const -> size_type {
    size_type max_len = 0;
    for (size_type i = 0; i < address_limit_; ++i) {
      size_type len = 0;
      size_type idx = i;
      while (idx != kEndOfChain) {
        if (slots_[idx].isOccupied() || slots_[idx].isTombstone()) {
          ++len;
        }
        idx = slots_[idx].next;
      }
      max_len = std::max(max_len, len);
    }
    return max_len;
  }

  constexpr auto avgChainLength() const -> double {
    size_type total_len = 0;
    size_type num_chains = 0;
    for (size_type i = 0; i < address_limit_; ++i) {
      size_type len = 0;
      size_type idx = i;
      while (idx != kEndOfChain) {
        if (slots_[idx].isOccupied() || slots_[idx].isTombstone()) {
          ++len;
        }
        idx = slots_[idx].next;
      }
      if (len > 0) {
        total_len += len;
        ++num_chains;
      }
    }
    return num_chains == 0 ? 0.0
                           : static_cast<double>(total_len) /
                                 static_cast<double>(num_chains);
  }

 private:
  // --------------------------------------------------------------------------
  // Internal Helpers
  // --------------------------------------------------------------------------

  constexpr auto needsResize() const -> bool {
    return size_ * kLoadFactorDenominator >= capacity_ * kLoadFactorNumerator;
  }

  constexpr auto hashIndex(const Key& key) const -> size_type {
    return hasher_(key) % address_limit_;
  }

  // Follow chain from hash index, looking for key
  constexpr auto findSlot(const Key& key) const -> size_type {
    size_type idx = hashIndex(key);

    while (idx != kEndOfChain) {
      const Slot& slot = slots_[idx];

      if (slot.isEmpty()) {
        return capacity_;  // Chain ended without finding key
      }

      if (slot.isOccupied() && equal_(slot.data()->first, key)) {
        return idx;
      }

      // Skip tombstones and continue along chain
      idx = slot.next;
    }

    return capacity_;  // Not found
  }

  // LICH: Late Insertion Coalesced Hashing
  // Find end of chain, allocate from cellar, link at end
  constexpr auto insertSlot(const Key& key) -> std::pair<size_type, bool> {
    if (needsResize()) {
      constexpr size_type kMaxCapacity = size_type{1} << 60;
      size_type new_cap = capacity_ * 2;
      assert(new_cap > capacity_ && "insertSlot: capacity overflow");
      assert(new_cap <= kMaxCapacity && "insertSlot: capacity exceeds maximum");
      resize(new_cap);
    }

    size_type idx = hashIndex(key);

    // If hash slot is empty, insert directly
    if (slots_[idx].isEmpty()) {
      slots_[idx].construct(kEndOfChain, key, Value{});
      ++size_;
      return {idx, true};
    }

    // Check if key already exists in chain, and find chain end
    size_type current = idx;
    size_type prev = capacity_;  // Track predecessor for linking

    while (current != kEndOfChain) {
      Slot& slot = slots_[current];

      if (slot.isOccupied() && equal_(slot.data()->first, key)) {
        return {current, false};  // Already exists
      }

      prev = current;
      current = slot.next;
    }

    // Key doesn't exist - allocate new slot from cellar or address region
    size_type new_slot = allocateSlot();
    assert(new_slot < capacity_ && "allocateSlot: failed to find empty slot");

    // Link new slot at end of chain
    slots_[prev].next = new_slot;
    slots_[new_slot].construct(kEndOfChain, key, Value{});
    ++size_;

    return {new_slot, true};
  }

  // Allocate a slot for collision chain
  // Prefer cellar, but scan address region if cellar exhausted
  constexpr auto allocateSlot() -> size_type {
    // Try cellar first (scan backward from cellar_ptr_)
    for (size_type i = cellar_ptr_; i >= address_limit_ && i < capacity_; --i) {
      if (slots_[i].isAvailable()) {
        cellar_ptr_ = i - 1;
        return i;
      }
    }

    // Cellar exhausted - scan address region for available slots
    // This can happen with many tombstones or high load
    for (size_type i = 0; i < address_limit_; ++i) {
      if (slots_[i].isAvailable()) {
        return i;
      }
    }

    // Should never reach here if load factor check works
    assert(false && "allocateSlot: no available slots");
    return capacity_;
  }

  constexpr void resize(size_type new_capacity) {
    new_capacity = std::max(new_capacity, kMinCapacity);

    constexpr size_type kMaxCapacity = size_type{1} << 60;
    assert(new_capacity <= kMaxCapacity && "resize: capacity exceeds maximum");

    auto old_slots = std::move(slots_);
    size_type old_capacity = capacity_;

    capacity_ = new_capacity;
    address_limit_ = (capacity_ * kAddressNumerator) / kAddressDenominator;
    cellar_ptr_ = capacity_ - 1;
    slots_ = std::make_unique<Slot[]>(capacity_);
    size_ = 0;

    // Initialize next pointers
    for (size_type i = 0; i < capacity_; ++i) {
      slots_[i].next = kEndOfChain;
    }

    // Reinsert all elements
    for (size_type i = 0; i < old_capacity; ++i) {
      if (old_slots[i].isOccupied()) {
        auto& [key, value] = *old_slots[i].mutableData();
        insertWithValue(std::move(key), std::move(value));
      }
    }

    // Clean up old slots
    for (size_type i = 0; i < old_capacity; ++i) {
      old_slots[i].destroy();
    }
  }

  // Insert with known value (used during resize)
  constexpr void insertWithValue(Key key, Value value) {
    size_type idx = hashIndex(key);

    if (slots_[idx].isEmpty()) {
      slots_[idx].construct(kEndOfChain, std::move(key), std::move(value));
      ++size_;
      return;
    }

    // Find end of chain
    size_type prev = idx;
    while (slots_[prev].next != kEndOfChain) {
      prev = slots_[prev].next;
    }

    // Allocate and link
    size_type new_slot = allocateSlot();
    slots_[prev].next = new_slot;
    slots_[new_slot].construct(kEndOfChain, std::move(key), std::move(value));
    ++size_;
  }

  // --------------------------------------------------------------------------
  // Member Variables
  // --------------------------------------------------------------------------

  size_type capacity_ = kMinCapacity;
  size_type address_limit_ = 0;  // End of address region (start of cellar)
  size_type size_ = 0;
  size_type cellar_ptr_ = 0;  // Next available cellar slot (scans backward)
  std::unique_ptr<Slot[]> slots_ = nullptr;

  [[no_unique_address]] Hash hasher_{};
  [[no_unique_address]] KeyEqual equal_{};
};

}  // namespace tempura
