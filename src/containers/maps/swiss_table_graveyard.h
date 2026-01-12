#pragma once

// ============================================================================
// Swiss Table Graveyard - SIMD Matching meets Strategic Tombstone Placement
// ============================================================================
//
// Combines Swiss Table's SIMD control byte matching with Graveyard Hashing's
// insight about strategic tombstone placement to combat clustering.
//
// THE HYBRID APPROACH
// -------------------
// Swiss Tables already use tombstones (kDeleted = 0xFE) for deletion.
// Traditional approach: tombstones are passive - created by deletion, cleaned
// by resize when (size + deleted) exceeds load factor threshold.
//
// Graveyard Hashing's insight: tombstones can ACTIVELY prevent clustering by
// breaking up probe sequences. Maintain tombstones at ~50% of free slots.
//
// KEY DIFFERENCES FROM STANDARD SWISS TABLE
// ------------------------------------------
// 1. Track deleted_ separately from size_ (both implementations do this)
// 2. When reusing a tombstone during insert: just reuse it, don't create new ones
// 3. During rebuild: place tombstones at evenly spaced primitive positions
//    (target ~50% of free slots)
// 4. When tombstones exceed 60% of free slots: rehash at same capacity
//    (reset tombstone distribution without growing the table)
//
// WHY THIS MIGHT HELP
// -------------------
// Swiss Tables already handle clustering well via SIMD batch matching and
// quadratic probing. But in pathological cases (adversarial hashes, poor
// hash functions), maintaining strategic tombstones could reduce probe
// lengths further by breaking up long runs of occupied slots.
//
// Target ratio: deleted ≈ (capacity - size) / 2
// - Too few: less cluster prevention benefit
// - Too many: more slots to skip during SIMD matching
// - ~50%: balance between cluster breaking and search efficiency
//
// PERFORMANCE CONSIDERATIONS
// ---------------------------
// Swiss Tables are already very fast. This variant adds:
// - Random tombstone creation overhead on tombstone reuse
// - Same-capacity rehashes when tombstone ratio gets too high
//
// Trade-off: potentially lower probe lengths vs synthetic tombstone overhead
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
// Control Byte Constants (same as Swiss Table)
// ============================================================================

// Using same control byte encoding as Swiss Table:
// - H2 values: 0x00-0x7F (high bit = 0, occupied)
// - kEmpty: 0xFF (high bit = 1)
// - kDeleted: 0xFE (high bit = 1)

// Note: these are defined in swiss_table.h but redefined here for clarity
inline constexpr std::uint8_t kGraveyardEmpty = 0b1111'1111;     // 0xFF
inline constexpr std::uint8_t kGraveyardDeleted = 0b1111'1110;   // 0xFE

constexpr auto h2Graveyard(std::size_t hash) -> std::uint8_t {
  return static_cast<std::uint8_t>(hash & 0x7F);
}

// ============================================================================
// Group - SIMD Control Byte Matching (same as Swiss Table)
// ============================================================================

class alignas(16) GraveyardGroup {
 public:
  static constexpr std::size_t kWidth = 16;

  constexpr GraveyardGroup() = default;

  static auto load(const std::uint8_t* ctrl) -> GraveyardGroup {
    GraveyardGroup g;
    g.data_ = Vec16i8::load(ctrl);
    return g;
  }

  auto match(std::uint8_t byte) const -> std::uint16_t {
    return data_.match(byte);
  }

  auto matchEmpty() const -> std::uint16_t {
    return data_.matchHighBitSet();
  }

  auto matchEmptyOrDeleted() const -> std::uint16_t {
    return matchEmpty();  // Both have high bit set
  }

  auto countLeadingEmpty() const -> std::size_t {
    std::uint16_t empty = matchEmpty();
    if (empty == 0xFFFF) return kWidth;
    return std::countr_one(empty);
  }

 private:
  Vec16i8 data_{};
};

// ============================================================================
// BitMask - Iterator over set bits (same as Swiss Table)
// ============================================================================

class GraveyardBitMask {
 public:
  explicit constexpr GraveyardBitMask(std::uint16_t mask) : mask_{mask} {}

  constexpr auto operator*() const -> std::size_t {
    return std::countr_zero(mask_);
  }

  constexpr auto operator++() -> GraveyardBitMask& {
    mask_ &= (mask_ - 1);  // Clear lowest set bit
    return *this;
  }

  constexpr explicit operator bool() const { return mask_ != 0; }

 private:
  std::uint16_t mask_;
};

// ============================================================================
// ProbeSeq - Quadratic Probing (same as Swiss Table)
// ============================================================================

class GraveyardProbeSeq {
 public:
  constexpr GraveyardProbeSeq(std::size_t hash, std::size_t mask)
      : mask_{mask}, offset_{hash & mask_} {}

  constexpr auto offset() const -> std::size_t { return offset_; }

  constexpr auto offset(std::size_t i) const -> std::size_t {
    return (offset_ + i) & mask_;
  }

  constexpr void next() {
    index_ += GraveyardGroup::kWidth;
    offset_ += index_;
    offset_ &= mask_;
  }

 private:
  std::size_t mask_;
  std::size_t offset_;
  std::size_t index_ = 0;
};

// ============================================================================
// SwissTableGraveyard
// ============================================================================

template <typename Key, typename Value, Hasher<Key> Hash = std::hash<Key>,
          typename KeyEqual = std::equal_to<Key>>
class SwissTableGraveyard {
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

  // Same 87.5% load factor as Swiss Table
  static constexpr size_type kLoadFactorNumerator = 7;
  static constexpr size_type kLoadFactorDenominator = 8;
  static constexpr size_type kMinCapacity = 16;

  // Graveyard-specific: tombstone ratio thresholds
  // Target: deleted ≈ (capacity - size) / 2 (50% of free slots)
  // Rehash when deleted > (capacity - size) * 0.6 (60% of free slots)
  static constexpr size_type kTombstoneHighNumerator = 6;
  static constexpr size_type kTombstoneDenominator = 10;

  // --------------------------------------------------------------------------
  // Constructors
  // --------------------------------------------------------------------------

  constexpr SwissTableGraveyard() : SwissTableGraveyard{kMinCapacity} {}

  constexpr explicit SwissTableGraveyard(size_type initial_capacity)
      : capacity_{normalizeCapacity(std::max(initial_capacity, kMinCapacity))},
        ctrl_{allocateControls(capacity_)},
        slots_{std::make_unique<Slot[]>(capacity_)} {
    resetControls();
  }

  constexpr SwissTableGraveyard(const SwissTableGraveyard& other)
      : capacity_{other.capacity_},
        size_{other.size_},
        deleted_{other.deleted_},
        ctrl_{allocateControls(capacity_)},
        slots_{std::make_unique<Slot[]>(capacity_)} {
    std::memcpy(ctrl_.get(), other.ctrl_.get(), capacity_ + GraveyardGroup::kWidth);
    for (size_type i = 0; i < capacity_; ++i) {
      if (isOccupied(ctrl_[i])) {
        slots_[i].construct(*other.slots_[i].data());
      }
    }
  }

  constexpr SwissTableGraveyard(SwissTableGraveyard&& other) noexcept
      : capacity_{other.capacity_},
        size_{other.size_},
        deleted_{other.deleted_},
        ctrl_{std::move(other.ctrl_)},
        slots_{std::move(other.slots_)} {
    other.capacity_ = 0;
    other.size_ = 0;
    other.deleted_ = 0;
  }

  constexpr auto operator=(const SwissTableGraveyard& other) -> SwissTableGraveyard& {
    if (this != &other) {
      SwissTableGraveyard tmp{other};
      swap(tmp);
    }
    return *this;
  }

  constexpr auto operator=(SwissTableGraveyard&& other) noexcept -> SwissTableGraveyard& {
    if (this != &other) {
      SwissTableGraveyard tmp{std::move(other)};
      swap(tmp);
    }
    return *this;
  }

  constexpr ~SwissTableGraveyard() {
    if (slots_) {
      for (size_type i = 0; i < capacity_; ++i) {
        if (isOccupied(ctrl_[i])) {
          slots_[i].destroy();
        }
      }
    }
  }

  constexpr void swap(SwissTableGraveyard& other) noexcept {
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

  // Effective load factor includes tombstones since they affect probe chains
  constexpr auto loadFactor() const noexcept -> double {
    return static_cast<double>(size_ + deleted_) /
           static_cast<double>(capacity_);
  }

  // Graveyard-specific queries
  constexpr auto tombstoneCount() const noexcept -> size_type {
    return deleted_;
  }

  constexpr auto occupiedLoadFactor() const noexcept -> double {
    return static_cast<double>(size_) / static_cast<double>(capacity_);
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

    slots_[idx].destroy();
    ctrl_[idx] = kGraveyardDeleted;
    --size_;
    ++deleted_;

    // Check if we need to rehash due to too many tombstones
    maintainTombstones();

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

    constexpr Iterator(SwissTableGraveyard* map, size_type idx)
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

    SwissTableGraveyard* map_;
    size_type idx_;
  };

  class ConstIterator {
   public:
    using difference_type = std::ptrdiff_t;
    using value_type = std::pair<const Key, Value>;
    using pointer = const value_type*;
    using reference = const value_type&;

    constexpr ConstIterator(const SwissTableGraveyard* map, size_type idx)
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

    const SwissTableGraveyard* map_;
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
    return (ctrl & 0x80) == 0;
  }

  constexpr auto needsResize() const -> bool {
    return (size_ + deleted_) * kLoadFactorDenominator >=
           capacity_ * kLoadFactorNumerator;
  }

  static constexpr auto normalizeCapacity(size_type cap) -> size_type {
    if (cap <= GraveyardGroup::kWidth) return GraveyardGroup::kWidth;
    return size_type{1} << (64 - std::countl_zero(cap - 1));
  }

  static auto allocateControls(size_type capacity)
      -> std::unique_ptr<std::uint8_t[]> {
    return std::make_unique<std::uint8_t[]>(capacity + GraveyardGroup::kWidth);
  }

  constexpr void resetControls() {
    std::memset(ctrl_.get(), kGraveyardEmpty, capacity_ + GraveyardGroup::kWidth);
  }

  constexpr auto findSlot(const Key& key) const -> size_type {
    std::size_t hash = hasher_(key);
    std::uint8_t h2_byte = h2Graveyard(hash);
    GraveyardProbeSeq seq{hash, capacity_ - 1};

    while (true) {
      GraveyardGroup g = GraveyardGroup::load(&ctrl_[seq.offset()]);
      GraveyardBitMask matches{g.match(h2_byte)};

      while (matches) {
        size_type idx = seq.offset(*matches);
        if (equal_(slots_[idx].data()->first, key)) {
          return idx;
        }
        ++matches;
      }

      if (g.matchEmpty()) {
        return capacity_;
      }

      seq.next();
    }
  }

  constexpr auto insertSlot(const Key& key) -> std::pair<size_type, bool> {
    if (needsResize()) {
      constexpr size_type kMaxCapacity = size_type{1} << 60;
      size_type new_cap = capacity_ * 2;
      assert(new_cap > capacity_ && "insertSlot: capacity overflow");
      assert(new_cap <= kMaxCapacity && "insertSlot: capacity exceeds maximum");
      resize(new_cap);
    }

    std::size_t hash = hasher_(key);
    std::uint8_t h2_byte = h2Graveyard(hash);
    GraveyardProbeSeq seq{hash, capacity_ - 1};

    while (true) {
      GraveyardGroup g = GraveyardGroup::load(&ctrl_[seq.offset()]);

      // Check for existing key
      GraveyardBitMask matches{g.match(h2_byte)};
      while (matches) {
        size_type idx = seq.offset(*matches);
        if (equal_(slots_[idx].data()->first, key)) {
          return {idx, false};  // Key already exists
        }
        ++matches;
      }

      // Find first empty or deleted slot
      GraveyardBitMask available{g.matchEmptyOrDeleted()};
      if (available) {
        size_type idx = seq.offset(*available);

        if (ctrl_[idx] == kGraveyardDeleted) {
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

  // Place tombstones at evenly spaced primitive positions
  constexpr void placePrimitiveTombstones() {
    size_type free_slots = capacity_ - size_;
    size_type target_tombstones = free_slots / 2;

    if (target_tombstones == 0) return;

    size_type spacing = capacity_ / target_tombstones;
    size_type placed = 0;

    for (size_type pos = spacing / 2; pos < capacity_ && placed < target_tombstones; pos += spacing) {
      if (ctrl_[pos] == kGraveyardEmpty) {
        ctrl_[pos] = kGraveyardDeleted;
        ++deleted_;
        ++placed;
      }
    }
  }

  // Graveyard-specific: check if we need same-capacity rehash
  constexpr void maintainTombstones() {
    size_type free_slots = capacity_ - size_;

    // If tombstones exceed 60% of free slots, rehash at same capacity
    if (deleted_ * kTombstoneDenominator > free_slots * kTombstoneHighNumerator) {
      rehashSameCapacity();
    }
  }

  // Graveyard-specific: rehash at same capacity to reset tombstones
  constexpr void rehashSameCapacity() {
    auto old_ctrl = std::move(ctrl_);
    auto old_slots = std::move(slots_);
    size_type old_capacity = capacity_;

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

    // Place tombstones at evenly spaced positions
    placePrimitiveTombstones();
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

    // Place tombstones at evenly spaced positions
    placePrimitiveTombstones();
  }

  constexpr void insertWithValue(Key key, Value value) {
    std::size_t hash = hasher_(key);
    std::uint8_t h2_byte = h2Graveyard(hash);
    GraveyardProbeSeq seq{hash, capacity_ - 1};

    while (true) {
      GraveyardGroup g = GraveyardGroup::load(&ctrl_[seq.offset()]);
      GraveyardBitMask available{g.matchEmptyOrDeleted()};

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
  size_type deleted_ = 0;

  std::unique_ptr<std::uint8_t[]> ctrl_ = nullptr;
  std::unique_ptr<Slot[]> slots_ = nullptr;

  [[no_unique_address]] Hash hasher_{};
  [[no_unique_address]] KeyEqual equal_{};
};

}  // namespace tempura
