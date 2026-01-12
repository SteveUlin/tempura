#pragma once

// ============================================================================
// Swiss Table Hash Map with Tombstone Reuse
// ============================================================================
//
// This is a variant of the standard Swiss Table that reuses DELETED slots
// during insertion. The core difference from swiss_table.h:
//
// STANDARD SWISS TABLE (swiss_table.h):
//   - insertSlot finds first empty OR deleted slot and uses it
//   - Simple, but doesn't distinguish between empty and deleted
//
// THIS VARIANT (swiss_table_reuse.h):
//   - insertSlot remembers the FIRST deleted slot encountered
//   - Continues probing to check if key already exists
//   - If inserting new key, uses the remembered deleted slot
//   - Falls back to first empty slot if no deleted slot was seen
//
// WHY THIS MATTERS:
//   - Better memory locality: deleted slots are earlier in probe sequence
//   - Potentially fewer cache misses on subsequent lookups
//   - Keeps the "occupied region" more compact
//
// TRADEOFF:
//   - Slightly slower insert (must probe until empty, not just available)
//   - Lookup is unchanged (same complexity)
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

// Internal namespace to avoid conflicts with swiss_table.h
namespace swiss_reuse {

inline constexpr std::uint8_t kEmpty = 0b1111'1111;     // 0xFF
inline constexpr std::uint8_t kDeleted = 0b1111'1110;   // 0xFE
inline constexpr std::uint8_t kSentinel = 0b1000'0000;  // 0x80

constexpr auto h2(std::size_t hash) -> std::uint8_t {
  return static_cast<std::uint8_t>(hash & 0x7F);
}

class alignas(16) GroupReuse {
 public:
  static constexpr std::size_t kWidth = 16;

  constexpr GroupReuse() = default;

  static auto load(const std::uint8_t* ctrl) -> GroupReuse {
    GroupReuse g;
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
    return matchEmpty();
  }

  auto countLeadingEmpty() const -> std::size_t {
    std::uint16_t empty = matchEmpty();
    if (empty == 0xFFFF) return kWidth;
    return std::countr_one(empty);
  }

 private:
  Vec16i8 data_{};
};

class BitMaskReuse {
 public:
  explicit constexpr BitMaskReuse(std::uint16_t mask) : mask_{mask} {}

  constexpr auto operator*() const -> std::size_t {
    return std::countr_zero(mask_);
  }

  constexpr auto operator++() -> BitMaskReuse& {
    mask_ &= (mask_ - 1);
    return *this;
  }

  constexpr explicit operator bool() const { return mask_ != 0; }

 private:
  std::uint16_t mask_;
};

class ProbeSeqReuse {
 public:
  constexpr ProbeSeqReuse(std::size_t hash, std::size_t mask)
      : mask_{mask}, offset_{hash & mask_} {}

  constexpr auto offset() const -> std::size_t { return offset_; }

  constexpr auto offset(std::size_t i) const -> std::size_t {
    return (offset_ + i) & mask_;
  }

  constexpr void next() {
    index_ += GroupReuse::kWidth;
    offset_ += index_;
    offset_ &= mask_;
  }

 private:
  std::size_t mask_;
  std::size_t offset_;
  std::size_t index_ = 0;
};

}  // namespace swiss_reuse

// ============================================================================
// SwissTableReuse
// ============================================================================

template <typename Key, typename Value, Hasher<Key> Hash = std::hash<Key>,
          typename KeyEqual = std::equal_to<Key>>
class SwissTableReuse {
  // Private type aliases to avoid namespace pollution
  static constexpr std::uint8_t kEmpty = swiss_reuse::kEmpty;
  static constexpr std::uint8_t kDeleted = swiss_reuse::kDeleted;
  using Group = swiss_reuse::GroupReuse;
  using BitMask = swiss_reuse::BitMaskReuse;
  using ProbeSeq = swiss_reuse::ProbeSeqReuse;

  static constexpr auto h2(std::size_t hash) -> std::uint8_t {
    return swiss_reuse::h2(hash);
  }

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

  static constexpr size_type kLoadFactorNumerator = 7;
  static constexpr size_type kLoadFactorDenominator = 8;
  static constexpr size_type kMinCapacity = 16;

  // --------------------------------------------------------------------------
  // Constructors
  // --------------------------------------------------------------------------

  constexpr SwissTableReuse() : SwissTableReuse{kMinCapacity} {}

  constexpr explicit SwissTableReuse(size_type initial_capacity)
      : capacity_{normalizeCapacity(std::max(initial_capacity, kMinCapacity))},
        ctrl_{allocateControls(capacity_)},
        slots_{std::make_unique<Slot[]>(capacity_)} {
    resetControls();
  }

  constexpr SwissTableReuse(const SwissTableReuse& other)
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

  constexpr SwissTableReuse(SwissTableReuse&& other) noexcept
      : capacity_{other.capacity_},
        size_{other.size_},
        deleted_{other.deleted_},
        ctrl_{std::move(other.ctrl_)},
        slots_{std::move(other.slots_)} {
    other.capacity_ = 0;
    other.size_ = 0;
    other.deleted_ = 0;
  }

  constexpr auto operator=(const SwissTableReuse& other) -> SwissTableReuse& {
    if (this != &other) {
      SwissTableReuse tmp{other};
      swap(tmp);
    }
    return *this;
  }

  constexpr auto operator=(SwissTableReuse&& other) noexcept -> SwissTableReuse& {
    if (this != &other) {
      SwissTableReuse tmp{std::move(other)};
      swap(tmp);
    }
    return *this;
  }

  constexpr ~SwissTableReuse() {
    if (slots_) {
      for (size_type i = 0; i < capacity_; ++i) {
        if (isOccupied(ctrl_[i])) {
          slots_[i].destroy();
        }
      }
    }
  }

  constexpr void swap(SwissTableReuse& other) noexcept {
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
    ctrl_[idx] = kDeleted;
    --size_;
    ++deleted_;

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

    constexpr Iterator(SwissTableReuse* map, size_type idx)
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

    SwissTableReuse* map_;
    size_type idx_;
  };

  class ConstIterator {
   public:
    using difference_type = std::ptrdiff_t;
    using value_type = std::pair<const Key, Value>;
    using pointer = const value_type*;
    using reference = const value_type&;

    constexpr ConstIterator(const SwissTableReuse* map, size_type idx)
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

    const SwissTableReuse* map_;
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
    if (cap <= Group::kWidth) return Group::kWidth;
    return size_type{1} << (64 - std::countl_zero(cap - 1));
  }

  static auto allocateControls(size_type capacity)
      -> std::unique_ptr<std::uint8_t[]> {
    return std::make_unique<std::uint8_t[]>(capacity + Group::kWidth);
  }

  constexpr void resetControls() {
    std::memset(ctrl_.get(), kEmpty, capacity_ + Group::kWidth);
  }

  constexpr auto findSlot(const Key& key) const -> size_type {
    std::size_t hash = hasher_(key);
    std::uint8_t h2_byte = h2(hash);
    ProbeSeq seq{hash, capacity_ - 1};

    while (true) {
      Group g = Group::load(&ctrl_[seq.offset()]);
      BitMask matches{g.match(h2_byte)};

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

  // ============================================================================
  // KEY DIFFERENCE: Tombstone Reuse on Insert
  // ============================================================================
  //
  // This insertSlot implementation differs from swiss_table.h:
  //
  // 1. Remember the first DELETED slot encountered (first_deleted_idx)
  // 2. Continue probing until we find the key OR an EMPTY slot
  // 3. If inserting new key:
  //    - Use first_deleted_idx if we found one (tombstone reuse)
  //    - Otherwise use the first empty slot found
  //
  // This keeps deleted slots near the start of probe sequences, improving
  // cache locality for subsequent lookups.

  constexpr auto insertSlot(const Key& key) -> std::pair<size_type, bool> {
    if (needsResize()) {
      constexpr size_type kMaxCapacity = size_type{1} << 60;
      size_type new_cap = capacity_ * 2;
      assert(new_cap > capacity_ && "insertSlot: capacity overflow");
      assert(new_cap <= kMaxCapacity && "insertSlot: capacity exceeds maximum");
      resize(new_cap);
    }

    std::size_t hash = hasher_(key);
    std::uint8_t h2_byte = h2(hash);
    ProbeSeq seq{hash, capacity_ - 1};

    // Remember first deleted slot for potential reuse
    size_type first_deleted_idx = capacity_;  // capacity_ = "not found"

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

      // Check for empty or deleted slots
      BitMask empty{g.matchEmpty()};
      if (empty) {
        // Iterate through all empty/deleted slots in this group
        BitMask scan = empty;
        while (scan) {
          size_type idx = seq.offset(*scan);

          // Remember first deleted slot
          if (ctrl_[idx] == kDeleted && first_deleted_idx == capacity_) {
            first_deleted_idx = idx;
          }

          // If we find an EMPTY slot, stop probing
          if (ctrl_[idx] == kEmpty) {
            // Insert at first deleted slot if we found one, otherwise here
            size_type insert_idx = (first_deleted_idx != capacity_)
                                     ? first_deleted_idx
                                     : idx;

            // If using a deleted slot, decrement deleted count
            if (ctrl_[insert_idx] == kDeleted) {
              --deleted_;
            }

            ctrl_[insert_idx] = h2_byte;
            slots_[insert_idx].construct(key, Value{});
            ++size_;
            return {insert_idx, true};
          }

          ++scan;
        }
        // All slots in this group were deleted, continue probing
      }

      seq.next();
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
  size_type deleted_ = 0;

  std::unique_ptr<std::uint8_t[]> ctrl_ = nullptr;
  std::unique_ptr<Slot[]> slots_ = nullptr;

  [[no_unique_address]] Hash hasher_{};
  [[no_unique_address]] KeyEqual equal_{};
};

}  // namespace tempura
