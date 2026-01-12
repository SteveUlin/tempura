#pragma once

// ============================================================================
// Swiss Table with Overflow Count - F14-style Early Termination
// ============================================================================
//
// This variant uses F14's overflow counting technique for early probe
// termination, which is superior to bloom filters.
//
// BACKGROUND (from Amble & Knuth 1974, refined by Facebook F14)
// -------------------------------------------------------------
// Standard Swiss Tables must probe until finding an empty slot to confirm
// a key doesn't exist. With overflow counting, we can terminate earlier:
//
//   Group structure: [15 H2 bytes | 1 overflow count byte]
//
// The overflow count tracks how many keys that WANTED to be in this group
// are actually stored in a later group (they "overflowed" past).
//
// ALGORITHM
// ---------
// Insert:
//   1. Probe from home group until finding an available slot
//   2. For each full group we probe past, increment its overflow count
//   3. Insert into the available slot
//
// Lookup:
//   1. Check if H2 matches any of the 15 control bytes (SIMD match)
//   2. If match found, verify key equality
//   3. If no match AND overflow count == 0 → key definitely doesn't exist
//   4. If no match AND overflow count > 0 → continue probing
//   5. If empty slot found → key doesn't exist
//
// Erase:
//   1. Find the key's location
//   2. Walk from home group to actual location, decrementing overflow counts
//   3. Mark slot as deleted
//
// WHY OVERFLOW COUNT > BLOOM FILTER
// ---------------------------------
// Our earlier bloom filter approach had problems:
//   - 8 bits tracking 128 H2 values → high false positive rate
//   - Bits never clear → saturates quickly
//   - At 87.5% load, most bloom bytes were ~0xFF (useless)
//
// Overflow count advantages:
//   - Exact count (no false positives)
//   - Decrements on erase (self-cleaning)
//   - Works well at high load factors
//   - F14 achieves 1.04 expected probes at max load
//
// REFERENCE
// ---------
// - Amble & Knuth, "Ordered Hash Tables" (1974) - overflow bit concept
// - Facebook Folly F14 - overflow count extension
// - https://engineering.fb.com/2019/04/25/developer-tools/f14/
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

namespace swissbloom {
inline constexpr std::uint8_t kEmpty = 0b1111'1111;
inline constexpr std::uint8_t kDeleted = 0b1111'1110;

constexpr auto h2(std::size_t hash) -> std::uint8_t {
  return static_cast<std::uint8_t>(hash & 0x7F);
}
}  // namespace swissbloom

// Group with 15 slots + 1 overflow count byte
class alignas(16) GroupBloom {
 public:
  static constexpr std::size_t kWidth = 16;
  static constexpr std::size_t kSlots = 15;
  static constexpr std::size_t kOverflowIndex = 15;

  static auto load(const std::uint8_t* ctrl) -> GroupBloom {
    GroupBloom g;
    g.data_ = Vec16i8::load(ctrl);
    return g;
  }

  // Match H2 in first 15 bytes only
  auto match(std::uint8_t byte) const -> std::uint16_t {
    return data_.match(byte) & 0x7FFF;
  }

  // Match truly empty slots (0xFF only, not deleted 0xFE) - for probe termination
  auto matchEmpty() const -> std::uint16_t {
    return data_.match(swissbloom::kEmpty) & 0x7FFF;
  }

  // Match empty or deleted slots (high bit set) - for finding insertion slots
  auto matchEmptyOrDeleted() const -> std::uint16_t {
    return data_.matchHighBitSet() & 0x7FFF;
  }

 private:
  Vec16i8 data_{};
};

class BitMaskBloom {
 public:
  explicit constexpr BitMaskBloom(std::uint16_t mask) : mask_{mask} {}

  constexpr auto operator*() const -> std::size_t {
    return std::countr_zero(mask_);
  }

  constexpr auto operator++() -> BitMaskBloom& {
    mask_ &= (mask_ - 1);
    return *this;
  }

  constexpr explicit operator bool() const { return mask_ != 0; }

 private:
  std::uint16_t mask_;
};

class ProbeSeqBloom {
 public:
  constexpr ProbeSeqBloom(std::size_t hash, std::size_t mask)
      : mask_{mask}, offset_{hash & mask_} {}

  constexpr auto offset() const -> std::size_t { return offset_; }

  constexpr auto offset(std::size_t i) const -> std::size_t {
    return (offset_ + i) & mask_;
  }

  constexpr void next() {
    index_ += GroupBloom::kWidth;
    offset_ += index_;
    offset_ &= mask_;
  }

  constexpr auto index() const -> std::size_t { return index_; }

 private:
  std::size_t mask_;
  std::size_t offset_;
  std::size_t index_ = 0;
};

template <typename Key, typename Value, Hasher<Key> Hash = std::hash<Key>,
          typename KeyEqual = std::equal_to<Key>>
class SwissTableBloom {
  static constexpr std::uint8_t kEmpty = swissbloom::kEmpty;
  static constexpr std::uint8_t kDeleted = swissbloom::kDeleted;
  using Group = GroupBloom;
  using BitMask = BitMaskBloom;
  using ProbeSeq = ProbeSeqBloom;

  static constexpr auto h2(std::size_t hash) -> std::uint8_t {
    return swissbloom::h2(hash);
  }

 public:
  using key_type = Key;
  using mapped_type = Value;
  using value_type = std::pair<const Key, Value>;
  using size_type = std::size_t;
  using hasher = Hash;
  using key_equal = KeyEqual;

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

  static constexpr size_type kLoadFactorNumerator = 7;
  static constexpr size_type kLoadFactorDenominator = 8;
  static constexpr size_type kMinCapacity = 16;

  constexpr SwissTableBloom() : SwissTableBloom{kMinCapacity} {}

  constexpr explicit SwissTableBloom(size_type initial_capacity)
      : capacity_{normalizeCapacity(std::max(initial_capacity, kMinCapacity))},
        ctrl_{allocateControls(capacity_)},
        slots_{std::make_unique<Slot[]>(capacity_)} {
    resetControls();
  }

  constexpr SwissTableBloom(const SwissTableBloom& other)
      : capacity_{other.capacity_},
        size_{other.size_},
        deleted_{other.deleted_},
        ctrl_{allocateControls(capacity_)},
        slots_{std::make_unique<Slot[]>(capacity_)} {
    std::memcpy(ctrl_.get(), other.ctrl_.get(), capacity_ + Group::kWidth);
    for (size_type i = 0; i < capacity_; ++i) {
      if (isSlotIndex(i) && isOccupied(ctrl_[i])) {
        slots_[i].construct(*other.slots_[i].data());
      }
    }
  }

  constexpr SwissTableBloom(SwissTableBloom&& other) noexcept
      : capacity_{other.capacity_},
        size_{other.size_},
        deleted_{other.deleted_},
        ctrl_{std::move(other.ctrl_)},
        slots_{std::move(other.slots_)} {
    other.capacity_ = 0;
    other.size_ = 0;
    other.deleted_ = 0;
  }

  constexpr auto operator=(const SwissTableBloom& other) -> SwissTableBloom& {
    if (this != &other) {
      SwissTableBloom tmp{other};
      swap(tmp);
    }
    return *this;
  }

  constexpr auto operator=(SwissTableBloom&& other) noexcept -> SwissTableBloom& {
    if (this != &other) {
      SwissTableBloom tmp{std::move(other)};
      swap(tmp);
    }
    return *this;
  }

  constexpr ~SwissTableBloom() {
    if (slots_) {
      for (size_type i = 0; i < capacity_; ++i) {
        if (isSlotIndex(i) && isOccupied(ctrl_[i])) {
          slots_[i].destroy();
        }
      }
    }
  }

  constexpr void swap(SwissTableBloom& other) noexcept {
    std::swap(capacity_, other.capacity_);
    std::swap(size_, other.size_);
    std::swap(deleted_, other.deleted_);
    std::swap(ctrl_, other.ctrl_);
    std::swap(slots_, other.slots_);
    std::swap(hasher_, other.hasher_);
    std::swap(equal_, other.equal_);
  }

  constexpr auto size() const noexcept -> size_type { return size_; }
  constexpr auto capacity() const noexcept -> size_type { return capacity_; }
  constexpr auto empty() const noexcept -> bool { return size_ == 0; }

  constexpr auto find(const Key& key) -> Value* {
    size_type idx = findSlot(key);
    if (idx == capacity_) return nullptr;
    return &slots_[idx].data()->second;
  }

  constexpr auto find(const Key& key) const -> const Value* {
    size_type idx = findSlot(key);
    if (idx == capacity_) return nullptr;
    return &slots_[idx].data()->second;
  }

  constexpr auto contains(const Key& key) const -> bool {
    return findSlot(key) != capacity_;
  }

  constexpr auto operator[](const Key& key) -> Value& {
    auto [idx, inserted] = insertSlot(key);
    return slots_[idx].data()->second;
  }

  constexpr auto insert(const Key& key, const Value& value)
      -> std::pair<Value*, bool> {
    auto [idx, inserted] = insertSlot(key);
    if (inserted) slots_[idx].data()->second = value;
    return {&slots_[idx].data()->second, inserted};
  }

  constexpr auto insert(const Key& key, Value&& value)
      -> std::pair<Value*, bool> {
    auto [idx, inserted] = insertSlot(key);
    if (inserted) slots_[idx].data()->second = std::move(value);
    return {&slots_[idx].data()->second, inserted};
  }

  constexpr auto insertOrAssign(const Key& key, const Value& value) -> bool {
    auto [idx, inserted] = insertSlot(key);
    slots_[idx].data()->second = value;
    return inserted;
  }

  constexpr auto erase(const Key& key) -> bool {
    std::size_t hash = hasher_(key);
    std::uint8_t h2_byte = h2(hash);
    ProbeSeq seq{hash, capacity_ - 1};

    // Track groups we probe past (to decrement overflow counts)
    size_type home_group = seq.offset() / Group::kWidth * Group::kWidth;

    while (true) {
      size_type group_start = seq.offset() / Group::kWidth * Group::kWidth;
      Group g = Group::load(&ctrl_[group_start]);
      BitMask matches{g.match(h2_byte)};

      while (matches) {
        size_type slot_in_group = *matches;
        size_type idx = group_start + slot_in_group;
        if (idx < capacity_ && equal_(slots_[idx].data()->first, key)) {
          // Found it - erase and decrement overflow counts
          slots_[idx].destroy();
          ctrl_[idx] = kDeleted;
          --size_;
          ++deleted_;

          // Decrement overflow counts from home group to here
          if (group_start != home_group) {
            decrementOverflowCounts(hash, group_start);
          }
          return true;
        }
        ++matches;
      }

      // Check for early termination via overflow count
      std::uint8_t overflow = ctrl_[group_start + Group::kOverflowIndex];
      if (overflow == 0 && !g.matchEmpty()) {
        // No overflow and no empty → key not here
        return false;
      }

      if (g.matchEmpty()) return false;
      seq.next();
    }
  }

  constexpr void clear() {
    for (size_type i = 0; i < capacity_; ++i) {
      if (isSlotIndex(i) && isOccupied(ctrl_[i])) slots_[i].destroy();
    }
    size_ = 0;
    deleted_ = 0;
    resetControls();
  }

  // Iterator support
  class iterator {
   public:
    using value_type = std::pair<const Key, Value>;
    using reference = value_type&;
    using pointer = value_type*;

    constexpr iterator() = default;
    constexpr iterator(SwissTableBloom* table, size_type pos)
        : table_{table}, pos_{pos} {
      skipToValid();
    }

    constexpr auto operator*() const -> reference {
      return *table_->slots_[pos_].data();
    }

    constexpr auto operator->() const -> pointer {
      return table_->slots_[pos_].data();
    }

    constexpr auto operator++() -> iterator& {
      ++pos_;
      skipToValid();
      return *this;
    }

    constexpr auto operator==(const iterator& other) const -> bool {
      return pos_ == other.pos_;
    }

    constexpr auto operator!=(const iterator& other) const -> bool {
      return !(*this == other);
    }

   private:
    constexpr void skipToValid() {
      while (pos_ < table_->capacity_ &&
             (!isSlotIndex(pos_) || !isOccupied(table_->ctrl_[pos_]))) {
        ++pos_;
      }
    }

    static constexpr auto isOccupied(std::uint8_t ctrl) -> bool {
      return (ctrl & 0x80) == 0;
    }

    static constexpr auto isSlotIndex(size_type idx) -> bool {
      return (idx % Group::kWidth) != Group::kOverflowIndex;
    }

    SwissTableBloom* table_ = nullptr;
    size_type pos_ = 0;
  };

  class const_iterator {
   public:
    using value_type = std::pair<const Key, Value>;
    using reference = const value_type&;
    using pointer = const value_type*;

    constexpr const_iterator() = default;
    constexpr const_iterator(const SwissTableBloom* table, size_type pos)
        : table_{table}, pos_{pos} {
      skipToValid();
    }

    constexpr auto operator*() const -> reference {
      return *table_->slots_[pos_].data();
    }

    constexpr auto operator->() const -> pointer {
      return table_->slots_[pos_].data();
    }

    constexpr auto operator++() -> const_iterator& {
      ++pos_;
      skipToValid();
      return *this;
    }

    constexpr auto operator==(const const_iterator& other) const -> bool {
      return pos_ == other.pos_;
    }

    constexpr auto operator!=(const const_iterator& other) const -> bool {
      return !(*this == other);
    }

   private:
    constexpr void skipToValid() {
      while (pos_ < table_->capacity_ &&
             (!isSlotIndex(pos_) || !isOccupied(table_->ctrl_[pos_]))) {
        ++pos_;
      }
    }

    static constexpr auto isOccupied(std::uint8_t ctrl) -> bool {
      return (ctrl & 0x80) == 0;
    }

    static constexpr auto isSlotIndex(size_type idx) -> bool {
      return (idx % Group::kWidth) != Group::kOverflowIndex;
    }

    const SwissTableBloom* table_ = nullptr;
    size_type pos_ = 0;
  };

  constexpr auto begin() -> iterator { return iterator{this, 0}; }
  constexpr auto end() -> iterator { return iterator{this, capacity_}; }
  constexpr auto begin() const -> const_iterator { return const_iterator{this, 0}; }
  constexpr auto end() const -> const_iterator { return const_iterator{this, capacity_}; }

 private:
  static constexpr auto isOccupied(std::uint8_t ctrl) -> bool {
    return (ctrl & 0x80) == 0;
  }

  static constexpr auto isSlotIndex(size_type idx) -> bool {
    // Check if idx is a slot position (not an overflow count position)
    return (idx % Group::kWidth) != Group::kOverflowIndex;
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
    // Initialize overflow counts to 0
    for (size_type i = Group::kOverflowIndex; i < capacity_; i += Group::kWidth) {
      ctrl_[i] = 0;
    }
  }

  constexpr auto findSlot(const Key& key) const -> size_type {
    std::size_t hash = hasher_(key);
    std::uint8_t h2_byte = h2(hash);
    ProbeSeq seq{hash, capacity_ - 1};

    while (true) {
      size_type group_start = seq.offset() / Group::kWidth * Group::kWidth;
      Group g = Group::load(&ctrl_[group_start]);
      BitMask matches{g.match(h2_byte)};

      while (matches) {
        size_type slot_in_group = *matches;
        size_type idx = group_start + slot_in_group;
        if (idx < capacity_ && equal_(slots_[idx].data()->first, key)) {
          return idx;
        }
        ++matches;
      }

      // Standard termination: found an empty slot
      if (g.matchEmpty()) return capacity_;

      // Early termination: group is full but overflow == 0
      // This means no keys that hash here are stored beyond this group
      std::uint8_t overflow = ctrl_[group_start + Group::kOverflowIndex];
      if (overflow == 0) {
        return capacity_;
      }

      seq.next();
    }
  }

  constexpr auto insertSlot(const Key& key) -> std::pair<size_type, bool> {
    if (needsResize()) {
      resize(capacity_ * 2);
    }

    std::size_t hash = hasher_(key);
    std::uint8_t h2_byte = h2(hash);
    ProbeSeq seq{hash, capacity_ - 1};
    size_type home_group = seq.offset() / Group::kWidth * Group::kWidth;
    size_type first_available = capacity_;

    while (true) {
      size_type group_start = seq.offset() / Group::kWidth * Group::kWidth;
      Group g = Group::load(&ctrl_[group_start]);

      // Check for existing key
      BitMask matches{g.match(h2_byte)};
      while (matches) {
        size_type slot_in_group = *matches;
        size_type idx = group_start + slot_in_group;
        if (idx < capacity_ && equal_(slots_[idx].data()->first, key)) {
          return {idx, false};
        }
        ++matches;
      }

      // Track first available slot
      if (first_available == capacity_) {
        BitMask available{g.matchEmptyOrDeleted()};
        if (available) {
          size_type slot_in_group = *available;
          if (slot_in_group < Group::kSlots) {
            first_available = group_start + slot_in_group;
          }
        }
      }

      // If we found an available slot and overflow count is 0, key doesn't exist
      std::uint8_t overflow = ctrl_[group_start + Group::kOverflowIndex];
      if (first_available != capacity_ && overflow == 0) {
        break;
      }

      if (g.matchEmpty()) break;
      seq.next();
    }

    // Insert at first available slot
    if (first_available == capacity_) {
      // Should not happen if load factor is maintained
      resize(capacity_ * 2);
      return insertSlot(key);
    }

    size_type insert_group = first_available / Group::kWidth * Group::kWidth;

    // Increment overflow counts for groups between home and insert location
    if (insert_group != home_group) {
      incrementOverflowCounts(hash, insert_group);
    }

    if (ctrl_[first_available] == kDeleted) {
      --deleted_;
    }

    ctrl_[first_available] = h2_byte;
    slots_[first_available].construct(key, Value{});
    ++size_;

    return {first_available, true};
  }

  // Increment overflow counts from home group up to (but not including) target_group
  constexpr void incrementOverflowCounts(std::size_t hash, size_type target_group) {
    ProbeSeq seq{hash, capacity_ - 1};
    while (true) {
      size_type group_start = seq.offset() / Group::kWidth * Group::kWidth;
      if (group_start == target_group) break;

      // Increment overflow count (saturating at 255)
      std::uint8_t& overflow = ctrl_[group_start + Group::kOverflowIndex];
      if (overflow < 255) ++overflow;

      seq.next();
    }
  }

  // Decrement overflow counts from home group up to (but not including) target_group
  constexpr void decrementOverflowCounts(std::size_t hash, size_type target_group) {
    ProbeSeq seq{hash, capacity_ - 1};
    while (true) {
      size_type group_start = seq.offset() / Group::kWidth * Group::kWidth;
      if (group_start == target_group) break;

      // Decrement overflow count
      std::uint8_t& overflow = ctrl_[group_start + Group::kOverflowIndex];
      if (overflow > 0) --overflow;

      seq.next();
    }
  }

  constexpr void resize(size_type new_capacity) {
    new_capacity = normalizeCapacity(std::max(new_capacity, kMinCapacity));

    auto old_ctrl = std::move(ctrl_);
    auto old_slots = std::move(slots_);
    size_type old_capacity = capacity_;

    capacity_ = new_capacity;
    ctrl_ = allocateControls(capacity_);
    slots_ = std::make_unique<Slot[]>(capacity_);
    size_ = 0;
    deleted_ = 0;
    resetControls();

    for (size_type i = 0; i < old_capacity; ++i) {
      if (isOccupied(old_ctrl[i]) && isSlotIndex(i)) {
        auto& [key, value] = *old_slots[i].mutableData();
        insertWithValue(std::move(key), std::move(value));
      }
    }

    for (size_type i = 0; i < old_capacity; ++i) {
      if (isOccupied(old_ctrl[i]) && isSlotIndex(i)) {
        old_slots[i].destroy();
      }
    }
  }

  constexpr void insertWithValue(Key key, Value value) {
    std::size_t hash = hasher_(key);
    std::uint8_t h2_byte = h2(hash);
    ProbeSeq seq{hash, capacity_ - 1};
    size_type home_group = seq.offset() / Group::kWidth * Group::kWidth;

    while (true) {
      size_type group_start = seq.offset() / Group::kWidth * Group::kWidth;
      Group g = Group::load(&ctrl_[group_start]);
      BitMask available{g.matchEmptyOrDeleted()};

      if (available) {
        size_type slot_in_group = *available;
        if (slot_in_group < Group::kSlots) {
          size_type idx = group_start + slot_in_group;

          // Increment overflow counts if not inserting in home group
          if (group_start != home_group) {
            incrementOverflowCounts(hash, group_start);
          }

          ctrl_[idx] = h2_byte;
          slots_[idx].construct(std::move(key), std::move(value));
          ++size_;
          return;
        }
      }

      seq.next();
    }
  }

  size_type capacity_ = kMinCapacity;
  size_type size_ = 0;
  size_type deleted_ = 0;

  std::unique_ptr<std::uint8_t[]> ctrl_ = nullptr;
  std::unique_ptr<Slot[]> slots_ = nullptr;

  [[no_unique_address]] Hash hasher_{};
  [[no_unique_address]] KeyEqual equal_{};
};

}  // namespace tempura
