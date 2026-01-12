#pragma once

// ============================================================================
// Swiss Table 32 with Prefetch - AVX2 + Memory Latency Hiding
// ============================================================================
//
// Combines SwissTable32's AVX2 32-byte group matching with software prefetching
// to hide memory latency during probe sequences.
//
// ============================================================================

#include <bit>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <immintrin.h>
#include <memory>
#include <utility>

#include "containers/maps/hasher.h"
#include "containers/maps/map_storage.h"
#include "meta/manual_lifetime.h"
#include "simd/simd.h"

namespace tempura {

namespace swiss32pf {
inline constexpr std::uint8_t kEmpty = 0b1111'1111;
inline constexpr std::uint8_t kDeleted = 0b1111'1110;

constexpr auto h2(std::size_t hash) -> std::uint8_t {
  return static_cast<std::uint8_t>(hash & 0x7F);
}

class alignas(32) Group {
 public:
  static constexpr std::size_t kWidth = 32;

  static auto load(const std::uint8_t* ctrl) -> Group {
    Group g;
    g.data_ = Vec32i8::load(ctrl);
    return g;
  }

  auto match(std::uint8_t byte) const -> std::uint32_t {
    return data_.match(byte);
  }

  auto matchEmpty() const -> std::uint32_t {
    return data_.matchHighBitSet();
  }

  auto matchEmptyOrDeleted() const -> std::uint32_t {
    return matchEmpty();
  }

 private:
  Vec32i8 data_{};
};

class BitMask {
 public:
  explicit constexpr BitMask(std::uint32_t mask) : mask_{mask} {}

  constexpr auto operator*() const -> std::size_t {
    return std::countr_zero(mask_);
  }

  constexpr auto operator++() -> BitMask& {
    mask_ &= (mask_ - 1);
    return *this;
  }

  constexpr explicit operator bool() const { return mask_ != 0; }

 private:
  std::uint32_t mask_;
};

class ProbeSeq {
 public:
  constexpr ProbeSeq(std::size_t hash, std::size_t mask)
      : mask_{mask}, offset_{hash & mask_} {}

  constexpr auto offset() const -> std::size_t { return offset_; }

  constexpr auto offset(std::size_t i) const -> std::size_t {
    return (offset_ + i) & mask_;
  }

  constexpr auto nextOffset() const -> std::size_t {
    std::size_t next_index = index_ + Group::kWidth;
    return (offset_ + next_index) & mask_;
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
}  // namespace swiss32pf

template <typename Key, typename Value, Hasher<Key> Hash = std::hash<Key>,
          typename KeyEqual = std::equal_to<Key>>
class SwissTable32Prefetch {
  static constexpr std::uint8_t kEmpty = swiss32pf::kEmpty;
  static constexpr std::uint8_t kDeleted = swiss32pf::kDeleted;
  using Group = swiss32pf::Group;
  using BitMask = swiss32pf::BitMask;
  using ProbeSeq = swiss32pf::ProbeSeq;

  static constexpr auto h2(std::size_t hash) -> std::uint8_t {
    return swiss32pf::h2(hash);
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
  static constexpr size_type kMinCapacity = 32;

  constexpr SwissTable32Prefetch() : SwissTable32Prefetch{kMinCapacity} {}

  constexpr explicit SwissTable32Prefetch(size_type initial_capacity)
      : capacity_{normalizeCapacity(std::max(initial_capacity, kMinCapacity))},
        ctrl_{allocateControls(capacity_)},
        slots_{std::make_unique<Slot[]>(capacity_)} {
    resetControls();
  }

  constexpr SwissTable32Prefetch(const SwissTable32Prefetch& other)
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

  constexpr SwissTable32Prefetch(SwissTable32Prefetch&& other) noexcept
      : capacity_{other.capacity_},
        size_{other.size_},
        deleted_{other.deleted_},
        ctrl_{std::move(other.ctrl_)},
        slots_{std::move(other.slots_)} {
    other.capacity_ = 0;
    other.size_ = 0;
    other.deleted_ = 0;
  }

  constexpr auto operator=(const SwissTable32Prefetch& other) -> SwissTable32Prefetch& {
    if (this != &other) {
      SwissTable32Prefetch tmp{other};
      swap(tmp);
    }
    return *this;
  }

  constexpr auto operator=(SwissTable32Prefetch&& other) noexcept -> SwissTable32Prefetch& {
    if (this != &other) {
      SwissTable32Prefetch tmp{std::move(other)};
      swap(tmp);
    }
    return *this;
  }

  constexpr ~SwissTable32Prefetch() {
    if (slots_) {
      for (size_type i = 0; i < capacity_; ++i) {
        if (isOccupied(ctrl_[i])) {
          slots_[i].destroy();
        }
      }
    }
  }

  constexpr void swap(SwissTable32Prefetch& other) noexcept {
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

  constexpr auto erase(const Key& key) -> bool {
    size_type idx = findSlot(key);
    if (idx == capacity_) return false;
    slots_[idx].destroy();

    // Optimization: if next slot is empty, we can use EMPTY instead of DELETED
    size_type next_idx = (idx + 1) & (capacity_ - 1);
    if (ctrl_[next_idx] == kEmpty) {
      ctrl_[idx] = kEmpty;
    } else {
      ctrl_[idx] = kDeleted;
      ++deleted_;
    }

    --size_;
    return true;
  }

  constexpr void clear() {
    for (size_type i = 0; i < capacity_; ++i) {
      if (isOccupied(ctrl_[i])) slots_[i].destroy();
    }
    size_ = 0;
    deleted_ = 0;
    resetControls();
  }

 private:
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
      // Prefetch next group
      size_type next_off = seq.nextOffset();
      _mm_prefetch(reinterpret_cast<const char*>(&ctrl_[next_off]), _MM_HINT_T0);
      _mm_prefetch(reinterpret_cast<const char*>(&slots_[next_off]), _MM_HINT_T0);

      Group g = Group::load(&ctrl_[seq.offset()]);
      BitMask matches{g.match(h2_byte)};

      while (matches) {
        size_type idx = seq.offset(*matches);
        if (equal_(slots_[idx].data()->first, key)) return idx;
        ++matches;
      }

      if (g.matchEmpty()) return capacity_;
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

    while (true) {
      size_type next_off = seq.nextOffset();
      _mm_prefetch(reinterpret_cast<const char*>(&ctrl_[next_off]), _MM_HINT_T0);
      _mm_prefetch(reinterpret_cast<const char*>(&slots_[next_off]), _MM_HINT_T0);

      Group g = Group::load(&ctrl_[seq.offset()]);

      BitMask matches{g.match(h2_byte)};
      while (matches) {
        size_type idx = seq.offset(*matches);
        if (equal_(slots_[idx].data()->first, key)) return {idx, false};
        ++matches;
      }

      BitMask available{g.matchEmptyOrDeleted()};
      if (available) {
        size_type idx = seq.offset(*available);
        if (ctrl_[idx] == kDeleted) --deleted_;
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
      if (isOccupied(old_ctrl[i])) old_slots[i].destroy();
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

  size_type capacity_ = kMinCapacity;
  size_type size_ = 0;
  size_type deleted_ = 0;

  std::unique_ptr<std::uint8_t[]> ctrl_ = nullptr;
  std::unique_ptr<Slot[]> slots_ = nullptr;

  [[no_unique_address]] Hash hasher_{};
  [[no_unique_address]] KeyEqual equal_{};
};

}  // namespace tempura
