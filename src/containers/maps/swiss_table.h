#pragma once

// ============================================================================
// Swiss Table Hash Map - A Visual Guide
// ============================================================================
//
// Swiss Tables power Google's Abseil, Rust's hashbrown, and Go 1.24's maps.
// The core insight: use a SEPARATE array of 1-byte "control bytes" to avoid
// touching actual keys until you're almost certain you've found a match.
//
//
// ┌─────────────────────────────────────────────────────────────────────────┐
// │  THE PROBLEM WITH TRADITIONAL HASH TABLES                               │
// └─────────────────────────────────────────────────────────────────────────┘
//
// Linear probing checks slots one-by-one, comparing keys:
//
//   slots:  [  K₀  |  K₁  |  K₂  |  K₃  |  ∅   |  K₅  | ... ]
//              ↑      ↑      ↑
//           check  check  check  ... until found or empty
//
// Each check requires: load key → compare → maybe found. Expensive!
// Cache misses hurt because keys are big (strings, structs, etc.)
//
//
// ┌─────────────────────────────────────────────────────────────────────────┐
// │  THE SWISS TABLE INSIGHT: CONTROL BYTES                                 │
// └─────────────────────────────────────────────────────────────────────────┘
//
// Add a parallel array of 1-byte "fingerprints" (control bytes):
//
//   ctrl:   [ 3A | 7F | 12 | 55 | FF | 3A | FF | 01 | ... ]   ← 1 byte each
//   slots:  [ K₀ | K₁ | K₂ | K₃ | ∅  | K₅ | ∅  | K₇ | ... ]   ← full keys
//             ↑    ↑    ↑    ↑    ↑    ↑    ↑    ↑
//            3A   7F   12   55  empty 3A  empty 01
//
// Each control byte is either:
//   - A 7-bit hash fingerprint (0x00-0x7F) if slot is occupied
//   - EMPTY (0xFF) if slot was never used
//   - DELETED (0xFE) if slot was erased (tombstone)
//
//
// ┌─────────────────────────────────────────────────────────────────────────┐
// │  CONTROL BYTE ENCODING - THE HIGH BIT TRICK                             │
// └─────────────────────────────────────────────────────────────────────────┘
//
// We need to distinguish "occupied" from "empty/deleted" FAST. Solution:
// use the high bit (bit 7) as a flag:
//
//   ┌─────────┬──────────────────────────────────────────┐
//   │ Bit 7   │  Meaning                                 │
//   ├─────────┼──────────────────────────────────────────┤
//   │   0     │  OCCUPIED - bits 0-6 hold H2 fingerprint │
//   │   1     │  SPECIAL - either EMPTY or DELETED       │
//   └─────────┴──────────────────────────────────────────┘
//
// Concrete byte values:
//
//   0x00-0x7F  →  Occupied slot, H2 fingerprint = the byte value
//                 Binary: 0xxxxxxx (high bit = 0)
//
//   0xFF       →  EMPTY (never used)
//                 Binary: 11111111 (high bit = 1)
//
//   0xFE       →  DELETED (tombstone)
//                 Binary: 11111110 (high bit = 1)
//
// Why this encoding?
//   - Check "is occupied?" with single bitwise AND: (ctrl & 0x80) == 0
//   - Check "is available?" (empty OR deleted): (ctrl & 0x80) != 0
//   - SIMD can test high bit of 16 bytes in ONE instruction
//
//
// ┌─────────────────────────────────────────────────────────────────────────┐
// │  H1/H2 HASH SPLITTING                                                   │
// └─────────────────────────────────────────────────────────────────────────┘
//
// We split the hash into two parts:
//
//   hash(key) = 0x4A7F_B312_8C5E_1A3F   (64-bit example)
//               └──────────┬─────────┘
//                     │
//          ┌──────────┴──────────┐
//          ▼                     ▼
//        H1 (upper bits)      H2 (lower 7 bits)
//        determines WHERE     stored IN the control byte
//        to look (group)      as a fingerprint
//
//   H1 = hash >> 7  →  which group of 16 slots to probe
//   H2 = hash & 0x7F  →  7-bit fingerprint (0-127)
//
// Example: hash = 0x...1A3F
//   H1 = 0x...1A3F >> 7 = 0x...34   →  start at group 0x34
//   H2 = 0x3F & 0x7F = 0x3F = 63    →  look for control byte 0x3F
//
//
// ┌─────────────────────────────────────────────────────────────────────────┐
// │  THE LOOKUP ALGORITHM - VISUAL WALKTHROUGH                              │
// └─────────────────────────────────────────────────────────────────────────┘
//
// Looking for a key with hash 0x...1A3F:
//
// Step 1: Compute H1 and H2
//   H1 = 0x34 (group index), H2 = 0x3F (fingerprint)
//
// Step 2: Jump to group 0x34 and load 16 control bytes into SIMD register
//
//   Group 0x34:
//   index:  0    1    2    3    4    5    6    7    8   ...  15
//   ctrl: [12 | 3F | FF | 3F | 55 | FF | 7A | 3F | 01 | ... | 88]
//              ^^        ^^                  ^^
//           MATCH!    MATCH!              MATCH!   (3 slots have H2=0x3F)
//
// Step 3: SIMD compares ALL 16 bytes against 0x3F simultaneously
//   Result bitmask: 0b0000000010001010 = positions {1, 3, 7}
//                              ^  ^ ^
//                             7  3 1
//
// Step 4: Only check actual keys at positions 1, 3, 7
//   - Position 1: key == target? If yes, found!
//   - Position 3: key == target? If yes, found!
//   - Position 7: key == target? If yes, found!
//
// Step 5: Not found in matches? Check for EMPTY slots.
//   If group has any EMPTY (0xFF), key definitely doesn't exist.
//   If group is full of occupied/deleted, probe next group.
//
// Key insight: We only compare 3 actual keys, not 16. The H2 fingerprint
// filters out ~99% of slots (only 1/128 chance of false positive per slot).
//
//
// ┌─────────────────────────────────────────────────────────────────────────┐
// │  MEMORY LAYOUT                                                          │
// └─────────────────────────────────────────────────────────────────────────┘
//
// Control bytes and slots are stored in separate arrays:
//
//   ┌───────────────────────────────────────────────────────────────┐
//   │ Control Bytes (1 byte each, 16-byte aligned for SIMD)        │
//   │                                                               │
//   │  Group 0         Group 1         Group 2        ...          │
//   │ [░░░░░░░░░░░░░░░░][░░░░░░░░░░░░░░░░][░░░░░░░░░░░░░░░░]        │
//   │  16 bytes         16 bytes         16 bytes                  │
//   └───────────────────────────────────────────────────────────────┘
//
//   ┌───────────────────────────────────────────────────────────────┐
//   │ Slots (key-value pairs, variable size)                       │
//   │                                                               │
//   │ [  Slot 0  ][  Slot 1  ][  Slot 2  ][ ... ][  Slot N-1  ]    │
//   └───────────────────────────────────────────────────────────────┘
//
// This separation is crucial:
//   - Control bytes are DENSE: 16 fit in one cache line (64 bytes)
//   - We only touch slots AFTER control byte match
//   - Much better cache utilization than interleaved layout
//
//
// ┌─────────────────────────────────────────────────────────────────────────┐
// │  THE SIMD MAGIC                                                         │
// └─────────────────────────────────────────────────────────────────────────┘
//
// SSE2 can compare 16 bytes in parallel with just 2 instructions:
//
//   __m128i ctrl = _mm_load_si128(control_bytes);   // Load 16 bytes
//   __m128i pattern = _mm_set1_epi8(h2);            // Broadcast H2
//   __m128i matches = _mm_cmpeq_epi8(ctrl, pattern); // Compare all 16
//   int mask = _mm_movemask_epi8(matches);          // Extract to bitmask
//
// The resulting mask has bit i set if control_byte[i] == h2.
// We iterate the set bits to find candidate slots.
//
// For checking empty slots (high bit set):
//   int empty_mask = _mm_movemask_epi8(ctrl);  // Bit i = high bit of byte i
//
// This is why control bytes use 0xFF/0xFE for empty/deleted:
// their high bit is 1, making empty detection trivial.
//
//
// ┌─────────────────────────────────────────────────────────────────────────┐
// │  DELETION AND TOMBSTONES                                                │
// └─────────────────────────────────────────────────────────────────────────┘
//
// When we delete a key, we can't just mark it EMPTY. Why?
//
//   Insert A at slot 5, B at slot 6 (B collided, probed forward)
//   Delete A (slot 5)
//   Find B → starts at slot 5, sees EMPTY, concludes "B not found" ✗
//
// Solution: mark deleted slots as DELETED (0xFE), not EMPTY (0xFF).
//
//   Before delete A:  [ ... | 3A | 7F | ... ]   slots 5, 6
//                            (A)  (B)
//
//   After delete A:   [ ... | FE | 7F | ... ]   slot 5 = DELETED
//                          tombstone (B)
//
//   Find B:  slot 5 = DELETED (keep probing), slot 6 = match!  ✓
//
// DELETED (0xFE) has high bit set, so:
//   - "Is occupied?" check fails (good, don't compare keys)
//   - "Is empty?" check also fails (good, keep probing)
//   - "Is available for insert?" check passes (we can reuse the slot)
//
//
// ┌─────────────────────────────────────────────────────────────────────────┐
// │  REFERENCES                                                             │
// └─────────────────────────────────────────────────────────────────────────┘
//
// Google's Swiss Tables design:
//   https://abseil.io/about/design/swisstables
//
// CppCon 2017 - Matt Kulukundis "Designing a Fast, Efficient Hash Table"
//   https://www.youtube.com/watch?v=ncHmEUmJZf4
//
// Rust hashbrown (excellent code + docs):
//   https://github.com/rust-lang/hashbrown
//
// ============================================================================

#include <bit>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <memory>
#include <utility>

#include "containers/maps/hasher.h"
#include "containers/maps/map_storage.h"
#include "meta/manual_lifetime.h"
#include "simd/simd.h"

namespace tempura {

// ============================================================================
// Control Byte Constants
// ============================================================================

// Control bytes use the high bit to distinguish special states from H2 values.
// Since H2 is 7 bits, valid H2 values are 0x00-0x7F (high bit = 0).
// Special states have high bit = 1.

inline constexpr std::uint8_t kEmpty = 0b1111'1111;     // 0xFF
inline constexpr std::uint8_t kDeleted = 0b1111'1110;   // 0xFE
inline constexpr std::uint8_t kSentinel = 0b1000'0000;  // 0x80 (also empty marker)

// H2 values are stored with high bit = 0, so they're in range [0, 127]
constexpr auto h2(std::size_t hash) -> std::uint8_t {
  return static_cast<std::uint8_t>(hash & 0x7F);
}

// ============================================================================
// Group - SIMD-based Control Byte Matching
// ============================================================================
//
// A Group represents 16 consecutive control bytes and provides parallel
// matching operations using SSE2 SIMD via Vec16i8.

class alignas(16) Group {
 public:
  static constexpr std::size_t kWidth = 16;

  constexpr Group() = default;

  static auto load(const std::uint8_t* ctrl) -> Group {
    Group g;
    g.data_ = Vec16i8::load(ctrl);
    return g;
  }

  // Match control bytes equal to the given value
  // Returns a bitmask where bit i is set if ctrl[i] == byte
  auto match(std::uint8_t byte) const -> std::uint16_t {
    return data_.match(byte);
  }

  // Match empty slots (control byte = kEmpty or high bit set)
  auto matchEmpty() const -> std::uint16_t {
    // Empty is 0xFF, deleted is 0xFE - both have high bit set
    // H2 values are 0x00-0x7F, so high bit = 0 means occupied
    return data_.matchHighBitSet();
  }

  // Match empty or deleted slots (any byte with high bit set)
  auto matchEmptyOrDeleted() const -> std::uint16_t {
    return matchEmpty();  // Both have high bit set
  }

  // Count leading empty slots
  auto countLeadingEmpty() const -> std::size_t {
    std::uint16_t empty = matchEmpty();
    if (empty == 0xFFFF) return kWidth;  // All empty
    return std::countr_one(empty);
  }

 private:
  Vec16i8 data_{};
};

// ============================================================================
// BitMask - Iterator over set bits
// ============================================================================

class BitMask {
 public:
  explicit constexpr BitMask(std::uint16_t mask) : mask_{mask} {}

  constexpr auto operator*() const -> std::size_t {
    return std::countr_zero(mask_);
  }

  constexpr auto operator++() -> BitMask& {
    mask_ &= (mask_ - 1);  // Clear lowest set bit
    return *this;
  }

  constexpr explicit operator bool() const { return mask_ != 0; }

 private:
  std::uint16_t mask_;
};

// ============================================================================
// ProbeSeq - Quadratic Probing Sequence
// ============================================================================
//
// Quadratic probing: probe_index(i) = (H1 + i² / 2) mod capacity
// This gives better distribution than linear probing while maintaining
// cache locality within groups.

class ProbeSeq {
 public:
  constexpr ProbeSeq(std::size_t hash, std::size_t mask)
      : mask_{mask}, offset_{hash & mask_} {}

  constexpr auto offset() const -> std::size_t { return offset_; }

  constexpr auto offset(std::size_t i) const -> std::size_t {
    return (offset_ + i) & mask_;
  }

  constexpr void next() {
    index_ += Group::kWidth;
    offset_ += index_;
    offset_ &= mask_;
  }

 private:
  std::size_t mask_;
  std::size_t offset_;
  std::size_t index_ = 0;
};

// ============================================================================
// SwissTable
// ============================================================================

template <typename Key, typename Value, Hasher<Key> Hash = std::hash<Key>,
          typename KeyEqual = std::equal_to<Key>>
class SwissTable {
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

  // Swiss Tables can operate at 87.5% load factor (7/8) efficiently
  // due to SIMD batch matching reducing probe lengths
  static constexpr size_type kLoadFactorNumerator = 7;
  static constexpr size_type kLoadFactorDenominator = 8;
  static constexpr size_type kMinCapacity = 16;  // Must be multiple of Group::kWidth

  // --------------------------------------------------------------------------
  // Constructors
  // --------------------------------------------------------------------------

  constexpr SwissTable() : SwissTable{kMinCapacity} {}

  constexpr explicit SwissTable(size_type initial_capacity)
      : capacity_{normalizeCapacity(std::max(initial_capacity, kMinCapacity))},
        ctrl_{allocateControls(capacity_)},
        slots_{std::make_unique<Slot[]>(capacity_)} {
    resetControls();
  }

  constexpr SwissTable(const SwissTable& other)
      : capacity_{other.capacity_},
        size_{other.size_},
        deleted_{other.deleted_},
        ctrl_{allocateControls(capacity_)},
        slots_{std::make_unique<Slot[]>(capacity_)} {
    std::memcpy(ctrl_.get(), other.ctrl_.get(), capacity_ + Group::kWidth);
    for (size_type i = 0; i < capacity_; ++i) {
      if (isOccupied(ctrl_[i])) {
        slots_[i].construct(*other.slots_[i].data());
      }
    }
  }

  constexpr SwissTable(SwissTable&& other) noexcept
      : capacity_{other.capacity_},
        size_{other.size_},
        deleted_{other.deleted_},
        ctrl_{std::move(other.ctrl_)},
        slots_{std::move(other.slots_)} {
    other.capacity_ = 0;
    other.size_ = 0;
    other.deleted_ = 0;
  }

  constexpr auto operator=(const SwissTable& other) -> SwissTable& {
    if (this != &other) {
      SwissTable tmp{other};
      swap(tmp);
    }
    return *this;
  }

  constexpr auto operator=(SwissTable&& other) noexcept -> SwissTable& {
    if (this != &other) {
      SwissTable tmp{std::move(other)};
      swap(tmp);
    }
    return *this;
  }

  constexpr ~SwissTable() {
    if (slots_) {
      for (size_type i = 0; i < capacity_; ++i) {
        if (isOccupied(ctrl_[i])) {
          slots_[i].destroy();
        }
      }
    }
  }

  constexpr void swap(SwissTable& other) noexcept {
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
  // Lookup
  // --------------------------------------------------------------------------
  // Swiss Table lookup: H1 determines group, H2 filters via SIMD match

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
  // Swiss Tables use tombstones (DELETED) for O(1) deletion without
  // invalidating SIMD control byte positions

  constexpr auto erase(const Key& key) -> bool {
    size_type idx = findSlot(key);
    if (idx == capacity_) {
      return false;
    }

    slots_[idx].destroy();

    // Optimization: if next slot is empty, we can use EMPTY instead of DELETED
    // This works because no probe chain could have passed through this slot
    // to reach a later element (since the next slot is empty)
    size_type next_idx = (idx + 1) & (capacity_ - 1);
    if (ctrl_[next_idx] == kEmpty) {
      ctrl_[idx] = kEmpty;
      // Don't increment deleted_ since we're not creating a tombstone
    } else {
      ctrl_[idx] = kDeleted;
      ++deleted_;
    }

    --size_;
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

    constexpr Iterator(SwissTable* map, size_type idx)
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

    SwissTable* map_;
    size_type idx_;
  };

  class ConstIterator {
   public:
    using difference_type = std::ptrdiff_t;
    using value_type = std::pair<const Key, Value>;
    using pointer = const value_type*;
    using reference = const value_type&;

    constexpr ConstIterator(const SwissTable* map, size_type idx)
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

    const SwissTable* map_;
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
    // Control byte is occupied if high bit is 0 (H2 values are [0, 127])
    return (ctrl & 0x80) == 0;
  }

  constexpr auto needsResize() const -> bool {
    // Resize when (size + deleted) exceeds load factor threshold
    // This ensures SIMD matching always finds empty slots efficiently
    return (size_ + deleted_) * kLoadFactorDenominator >=
           capacity_ * kLoadFactorNumerator;
  }

  // Normalize capacity to power of 2 (required for bitwise AND masking in ProbeSeq)
  // Must also be >= Group::kWidth for SIMD alignment
  static constexpr auto normalizeCapacity(size_type cap) -> size_type {
    if (cap <= Group::kWidth) return Group::kWidth;
    // Round up to next power of 2: 1 << ceil(log2(cap))
    return size_type{1} << (64 - std::countl_zero(cap - 1));
  }

  // Allocate control bytes with extra Group::kWidth sentinel bytes at end
  static auto allocateControls(size_type capacity)
      -> std::unique_ptr<std::uint8_t[]> {
    return std::make_unique<std::uint8_t[]>(capacity + Group::kWidth);
  }

  // Initialize all control bytes to kEmpty, including sentinels
  constexpr void resetControls() {
    std::memset(ctrl_.get(), kEmpty, capacity_ + Group::kWidth);
  }

  // Swiss Table lookup using H1/H2 split and SIMD matching
  constexpr auto findSlot(const Key& key) const -> size_type {
    std::size_t hash = hasher_(key);
    std::uint8_t h2_byte = h2(hash);
    ProbeSeq seq{hash, capacity_ - 1};

    while (true) {
      Group g = Group::load(&ctrl_[seq.offset()]);
      BitMask matches{g.match(h2_byte)};

      // Check each slot where control byte matches H2
      while (matches) {
        size_type idx = seq.offset(*matches);
        if (equal_(slots_[idx].data()->first, key)) {
          return idx;
        }
        ++matches;
      }

      // If group has any empty slots, key is not in table
      if (g.matchEmpty()) {
        return capacity_;
      }

      seq.next();
    }
  }

  constexpr auto insertSlot(const Key& key) -> std::pair<size_type, bool> {
    if (needsResize()) {
      // Check if same-capacity rehash is sufficient (lots of tombstones, few elements)
      // size ≤ 25/32 of capacity means we can rehash-in-place to clear tombstones
      // But only if capacity > kMinCapacity (probing needs at least 2 groups to work)
      if (size_ * 32 <= capacity_ * 25 && capacity_ > kMinCapacity) {
        rehashSameCapacity();
      } else {
        // Actually need more space - double the capacity
        constexpr size_type kMaxCapacity = size_type{1} << 60;
        size_type new_cap = capacity_ * 2;
        assert(new_cap > capacity_ && "insertSlot: capacity overflow");
        assert(new_cap <= kMaxCapacity && "insertSlot: capacity exceeds maximum");
        resize(new_cap);
      }
    }

    std::size_t hash = hasher_(key);
    std::uint8_t h2_byte = h2(hash);
    ProbeSeq seq{hash, capacity_ - 1};

    while (true) {
      Group g = Group::load(&ctrl_[seq.offset()]);

      // Check for existing key
      BitMask matches{g.match(h2_byte)};
      while (matches) {
        size_type idx = seq.offset(*matches);
        if (equal_(slots_[idx].data()->first, key)) {
          return {idx, false};  // Key already exists
        }
        ++matches;
      }

      // Find first empty or deleted slot
      BitMask available{g.matchEmptyOrDeleted()};
      if (available) {
        size_type idx = seq.offset(*available);

        // If slot was deleted, decrement deleted count
        if (ctrl_[idx] == kDeleted) {
          --deleted_;
        }

        // Insert new element
        ctrl_[idx] = h2_byte;
        slots_[idx].construct(key, Value{});
        ++size_;
        return {idx, true};
      }

      seq.next();
    }
  }

  // Rehash at same capacity to clear tombstones without growing
  constexpr void rehashSameCapacity() {
    auto old_ctrl = std::move(ctrl_);
    auto old_slots = std::move(slots_);
    size_type old_capacity = capacity_;

    ctrl_ = allocateControls(capacity_);
    slots_ = std::make_unique<Slot[]>(capacity_);
    size_ = 0;
    deleted_ = 0;
    resetControls();

    // Reinsert all live elements (skipping tombstones)
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

  // Insert with known value (used during resize)
  constexpr void insertWithValue(Key key, Value value) {
    std::size_t hash = hasher_(key);
    std::uint8_t h2_byte = h2(hash);
    ProbeSeq seq{hash, capacity_ - 1};

    while (true) {
      Group g = Group::load(&ctrl_[seq.offset()]);
      BitMask available{g.matchEmptyOrDeleted()};

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
  size_type deleted_ = 0;  // Track tombstones for resize decisions

  // Control bytes: capacity + Group::kWidth (sentinels for wrap-around)
  std::unique_ptr<std::uint8_t[]> ctrl_ = nullptr;
  std::unique_ptr<Slot[]> slots_ = nullptr;

  [[no_unique_address]] Hash hasher_{};
  [[no_unique_address]] KeyEqual equal_{};
};

}  // namespace tempura
