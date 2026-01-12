#pragma once

// ============================================================================
// Swiss Table Hash Map (32-byte AVX2 Groups)
// ============================================================================
//
// This is a variant of Swiss Tables using 32-byte groups instead of 16-byte
// groups. With AVX2, we can match 32 control bytes in a single instruction,
// potentially reducing probe iterations by 2x compared to SSE2.
//
// KEY DIFFERENCES FROM 16-BYTE VERSION
// ------------------------------------
// - Group width: 32 bytes (vs 16)
// - SIMD register: __m256i / Vec32i8 (vs __m128i / Vec16i8)
// - Bitmask type: uint32_t (vs uint16_t)
// - Min capacity: 32 (vs 16)
//
// TRADEOFFS
// ---------
// Advantages of 32-byte groups:
//   - Fewer probe iterations (check 32 slots per group vs 16)
//   - Better utilization of AVX2 width (available on all modern CPUs)
//   - Potentially better for medium-to-large tables
//
// Disadvantages:
//   - Larger minimum allocation (32 slots minimum)
//   - May overshoot for very small tables
//   - 32-byte alignment preferred (cache line considerations)
//
// WHEN TO USE
// -----------
// - Medium hash tables (100+ elements)
// - Good balance between SSE2 (16-byte) and AVX-512 (64-byte) variants
// - Universal AVX2 support on modern CPUs (no frequency throttling)
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
// Control Byte Constants (same as 16-byte version)
// ============================================================================

namespace swiss32 {
inline constexpr std::uint8_t kEmpty = 0b1111'1111;
inline constexpr std::uint8_t kDeleted = 0b1111'1110;

constexpr auto h2(std::size_t hash) -> std::uint8_t {
  return static_cast<std::uint8_t>(hash & 0x7F);
}
}  // namespace swiss32

// ============================================================================
// Group32 - AVX2 Control Byte Matching (32 bytes at once)
// ============================================================================

class alignas(32) Group32 {
 public:
  static constexpr std::size_t kWidth = 32;

  constexpr Group32() = default;

  static auto load(const std::uint8_t* ctrl) -> Group32 {
    Group32 g;
    g.data_ = Vec32i8::load(ctrl);
    return g;
  }

  // Match control bytes equal to the given value
  // Returns a 32-bit mask where bit i is set if ctrl[i] == byte
  auto match(std::uint8_t byte) const -> std::uint32_t {
    return data_.match(byte);
  }

  // Match empty slots (control bytes with high bit set)
  auto matchEmpty() const -> std::uint32_t {
    return data_.matchHighBitSet();
  }

  // Match empty or deleted slots
  auto matchEmptyOrDeleted() const -> std::uint32_t {
    return matchEmpty();
  }

 private:
  Vec32i8 data_{};
};

// ============================================================================
// BitMask32 - Iterator over set bits in 32-bit mask
// ============================================================================

class BitMask32 {
 public:
  explicit constexpr BitMask32(std::uint32_t mask) : mask_{mask} {}

  constexpr auto operator*() const -> std::size_t {
    return std::countr_zero(mask_);
  }

  constexpr auto operator++() -> BitMask32& {
    mask_ &= (mask_ - 1);  // Clear lowest set bit
    return *this;
  }

  constexpr explicit operator bool() const { return mask_ != 0; }

 private:
  std::uint32_t mask_;
};

// ============================================================================
// ProbeSeq32 - Quadratic Probing for 32-byte groups
// ============================================================================

class ProbeSeq32 {
 public:
  constexpr ProbeSeq32(std::size_t hash, std::size_t mask)
      : mask_{mask}, offset_{hash & mask_} {}

  constexpr auto offset() const -> std::size_t { return offset_; }

  constexpr auto offset(std::size_t i) const -> std::size_t {
    return (offset_ + i) & mask_;
  }

  constexpr void next() {
    index_ += Group32::kWidth;
    offset_ += index_;
    offset_ &= mask_;
  }

 private:
  std::size_t mask_;
  std::size_t offset_;
  std::size_t index_ = 0;
};

// ============================================================================
// SwissTable32
// ============================================================================

template <typename Key, typename Value, Hasher<Key> Hash = std::hash<Key>,
          typename KeyEqual = std::equal_to<Key>>
class SwissTable32 {
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
  static constexpr size_type kMinCapacity = 32;  // Must be multiple of 32

  // --------------------------------------------------------------------------
  // Constructors
  // --------------------------------------------------------------------------

  constexpr SwissTable32() : SwissTable32{kMinCapacity} {}

  constexpr explicit SwissTable32(size_type initial_capacity)
      : capacity_{normalizeCapacity(std::max(initial_capacity, kMinCapacity))},
        ctrl_{allocateControls(capacity_)},
        slots_{std::make_unique<Slot[]>(capacity_)} {
    resetControls();
  }

  constexpr SwissTable32(const SwissTable32& other)
      : capacity_{other.capacity_},
        size_{other.size_},
        deleted_{other.deleted_},
        ctrl_{allocateControls(capacity_)},
        slots_{std::make_unique<Slot[]>(capacity_)} {
    std::memcpy(ctrl_.get(), other.ctrl_.get(), capacity_ + Group32::kWidth);
    for (size_type i = 0; i < capacity_; ++i) {
      if (isOccupied(ctrl_[i])) {
        slots_[i].construct(*other.slots_[i].data());
      }
    }
  }

  constexpr SwissTable32(SwissTable32&& other) noexcept
      : capacity_{other.capacity_},
        size_{other.size_},
        deleted_{other.deleted_},
        ctrl_{std::move(other.ctrl_)},
        slots_{std::move(other.slots_)} {
    other.capacity_ = 0;
    other.size_ = 0;
    other.deleted_ = 0;
  }

  constexpr auto operator=(const SwissTable32& other) -> SwissTable32& {
    if (this != &other) {
      SwissTable32 tmp{other};
      swap(tmp);
    }
    return *this;
  }

  constexpr auto operator=(SwissTable32&& other) noexcept -> SwissTable32& {
    if (this != &other) {
      SwissTable32 tmp{std::move(other)};
      swap(tmp);
    }
    return *this;
  }

  constexpr ~SwissTable32() {
    if (slots_) {
      for (size_type i = 0; i < capacity_; ++i) {
        if (isOccupied(ctrl_[i])) {
          slots_[i].destroy();
        }
      }
    }
  }

  constexpr void swap(SwissTable32& other) noexcept {
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

    // Optimization: if next slot is empty, we can use EMPTY instead of DELETED
    // This works because no probe chain could have passed through this slot
    // to reach a later element (since the next slot is empty)
    size_type next_idx = (idx + 1) & (capacity_ - 1);
    if (ctrl_[next_idx] == swiss32::kEmpty) {
      ctrl_[idx] = swiss32::kEmpty;
      // Don't increment deleted_ since we're not creating a tombstone
    } else {
      ctrl_[idx] = swiss32::kDeleted;
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

    constexpr Iterator(SwissTable32* map, size_type idx)
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

    SwissTable32* map_;
    size_type idx_;
  };

  class ConstIterator {
   public:
    using difference_type = std::ptrdiff_t;
    using value_type = std::pair<const Key, Value>;
    using pointer = const value_type*;
    using reference = const value_type&;

    constexpr ConstIterator(const SwissTable32* map, size_type idx)
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

    const SwissTable32* map_;
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

  // Normalize capacity to power of 2 (required for bitwise AND masking in ProbeSeq32)
  // Must also be >= Group32::kWidth (32) for AVX2 alignment
  static constexpr auto normalizeCapacity(size_type cap) -> size_type {
    if (cap <= Group32::kWidth) return Group32::kWidth;
    // Round up to next power of 2: 1 << ceil(log2(cap))
    return size_type{1} << (64 - std::countl_zero(cap - 1));
  }

  static auto allocateControls(size_type capacity)
      -> std::unique_ptr<std::uint8_t[]> {
    return std::make_unique<std::uint8_t[]>(capacity + Group32::kWidth);
  }

  constexpr void resetControls() {
    std::memset(ctrl_.get(), swiss32::kEmpty, capacity_ + Group32::kWidth);
  }

  // Lookup using 32-byte AVX2 groups
  constexpr auto findSlot(const Key& key) const -> size_type {
    std::size_t hash = hasher_(key);
    std::uint8_t h2_byte = swiss32::h2(hash);
    ProbeSeq32 seq{hash, capacity_ - 1};

    while (true) {
      Group32 g = Group32::load(&ctrl_[seq.offset()]);
      BitMask32 matches{g.match(h2_byte)};

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
    std::uint8_t h2_byte = swiss32::h2(hash);
    ProbeSeq32 seq{hash, capacity_ - 1};

    while (true) {
      Group32 g = Group32::load(&ctrl_[seq.offset()]);

      BitMask32 matches{g.match(h2_byte)};
      while (matches) {
        size_type idx = seq.offset(*matches);
        if (equal_(slots_[idx].data()->first, key)) {
          return {idx, false};
        }
        ++matches;
      }

      BitMask32 available{g.matchEmptyOrDeleted()};
      if (available) {
        size_type idx = seq.offset(*available);

        if (ctrl_[idx] == swiss32::kDeleted) {
          --deleted_;
        }

        ctrl_[idx] = h2_byte;
        slots_[idx].construct(key, Value{});
        ++size_;
        return {idx, true};
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

    for (size_type i = 0; i < old_capacity; ++i) {
      if (isOccupied(old_ctrl[i])) {
        auto& [key, value] = *old_slots[i].mutableData();
        insertWithValue(std::move(key), std::move(value));
      }
    }

    for (size_type i = 0; i < old_capacity; ++i) {
      if (isOccupied(old_ctrl[i])) {
        old_slots[i].destroy();
      }
    }
  }

  constexpr void insertWithValue(Key key, Value value) {
    std::size_t hash = hasher_(key);
    std::uint8_t h2_byte = swiss32::h2(hash);
    ProbeSeq32 seq{hash, capacity_ - 1};

    while (true) {
      Group32 g = Group32::load(&ctrl_[seq.offset()]);
      BitMask32 available{g.matchEmptyOrDeleted()};

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
