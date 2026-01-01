#pragma once

// ============================================================================
// Two-Choice Hash Map
// ============================================================================
//
// Two-choice hashing (also called "power of two choices") dramatically reduces
// maximum load imbalance by giving each element two possible locations and
// choosing the less loaded one.
//
// THE POWER OF TWO CHOICES
// ------------------------
// Classic result from balls-into-bins analysis:
//   - Random placement: max load ≈ O(log n / log log n) with n balls, n bins
//   - Two-choice: max load ≈ O(log log n) - exponentially better!
//
// This is one of the most powerful results in randomized algorithms.
// With almost no extra work (one more hash, one more comparison), we get
// exponentially better load balancing.
//
// HOW IT WORKS
// ------------
// Each bucket is a mini-chain (or we use open addressing with probing).
// On insert:
//   1. Compute h1(key) and h2(key)
//   2. Check load of both buckets
//   3. Insert into the less loaded bucket
//
// On lookup:
//   1. Check h1(key) bucket
//   2. If not found, check h2(key) bucket
//
// IMPLEMENTATION CHOICE
// ---------------------
// We use separate chaining (linked lists) for simplicity. Each bucket has
// a chain, and we insert into the shorter chain. This makes the two-choice
// benefit most visible.
//
// WHY IT WORKS
// ------------
// Intuition: if one location is crowded, the other probably isn't.
// By always picking the better option, we prevent any bucket from getting
// much worse than average.
//
// Math: The probability that both buckets have k+ elements drops exponentially
// with k, so max chain length is O(log log n) instead of O(log n).
//
// ============================================================================

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <utility>

#include "containers/maps/hasher.h"

namespace tempura {

// Secondary hash function for two-choice
template <typename Key>
struct TwoChoiceSecondaryHash {
  auto operator()(const Key& key) const -> std::size_t {
    std::size_t h = std::hash<Key>{}(key);
    // Different mixing constants than Cuckoo's secondary hash
    h ^= h >> 31;
    h *= 0x85ebca6bULL;
    h ^= h >> 27;
    h *= 0xc2b2ae35ULL;
    h ^= h >> 31;
    return h;
  }
};

template <typename Key, typename Value, Hasher<Key> Hash1 = std::hash<Key>,
          Hasher<Key> Hash2 = TwoChoiceSecondaryHash<Key>,
          typename KeyEqual = std::equal_to<Key>>
class TwoChoiceMap {
 public:
  using key_type = Key;
  using mapped_type = Value;
  using value_type = std::pair<const Key, Value>;
  using size_type = std::size_t;
  using hasher = Hash1;
  using key_equal = KeyEqual;

  // --------------------------------------------------------------------------
  // Node Structure (for chaining)
  // --------------------------------------------------------------------------

  struct Node {
    std::pair<const Key, Value> data;
    std::unique_ptr<Node> next;

    template <typename K, typename V>
    Node(K&& k, V&& v)
        : data{std::forward<K>(k), std::forward<V>(v)}, next{nullptr} {}
  };

  struct Bucket {
    std::unique_ptr<Node> head = nullptr;
    size_type count = 0;  // Track chain length for two-choice decision
  };

  // --------------------------------------------------------------------------
  // Constants
  // --------------------------------------------------------------------------

  static constexpr size_type kLoadFactorNumerator = 10;  // Higher load OK with chaining
  static constexpr size_type kLoadFactorDenominator = 10;
  static constexpr size_type kMinCapacity = 8;

  // --------------------------------------------------------------------------
  // Constructors
  // --------------------------------------------------------------------------

  constexpr TwoChoiceMap() : TwoChoiceMap{kMinCapacity} {}

  constexpr explicit TwoChoiceMap(size_type initial_capacity)
      : capacity_{std::max(initial_capacity, kMinCapacity)},
        buckets_{std::make_unique<Bucket[]>(capacity_)} {}

  constexpr TwoChoiceMap(const TwoChoiceMap& other)
      : capacity_{other.capacity_},
        size_{other.size_},
        buckets_{std::make_unique<Bucket[]>(capacity_)} {
    for (size_type i = 0; i < capacity_; ++i) {
      const Node* src = other.buckets_[i].head.get();
      std::unique_ptr<Node>* dst = &buckets_[i].head;
      buckets_[i].count = other.buckets_[i].count;

      while (src != nullptr) {
        *dst = std::make_unique<Node>(src->data.first, src->data.second);
        dst = &(*dst)->next;
        src = src->next.get();
      }
    }
  }

  constexpr TwoChoiceMap(TwoChoiceMap&& other) noexcept
      : capacity_{other.capacity_},
        size_{other.size_},
        buckets_{std::move(other.buckets_)} {
    other.size_ = 0;
    other.capacity_ = 0;
  }

  constexpr auto operator=(const TwoChoiceMap& other) -> TwoChoiceMap& {
    if (this != &other) {
      TwoChoiceMap tmp{other};
      swap(tmp);
    }
    return *this;
  }

  constexpr auto operator=(TwoChoiceMap&& other) noexcept -> TwoChoiceMap& {
    if (this != &other) {
      TwoChoiceMap tmp{std::move(other)};
      swap(tmp);
    }
    return *this;
  }

  ~TwoChoiceMap() = default;

  constexpr void swap(TwoChoiceMap& other) noexcept {
    std::swap(capacity_, other.capacity_);
    std::swap(size_, other.size_);
    std::swap(buckets_, other.buckets_);
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
    return static_cast<double>(size_) / static_cast<double>(capacity_);
  }

  // --------------------------------------------------------------------------
  // Lookup
  // --------------------------------------------------------------------------
  // Check both possible buckets

  constexpr auto find(const Key& key) -> Value* {
    // Check bucket 1
    size_type idx1 = hash1(key);
    for (Node* node = buckets_[idx1].head.get(); node; node = node->next.get()) {
      if (equal_(node->data.first, key)) {
        return &node->data.second;
      }
    }

    // Check bucket 2
    size_type idx2 = hash2(key);
    if (idx2 != idx1) {
      for (Node* node = buckets_[idx2].head.get(); node; node = node->next.get()) {
        if (equal_(node->data.first, key)) {
          return &node->data.second;
        }
      }
    }

    return nullptr;
  }

  constexpr auto find(const Key& key) const -> const Value* {
    size_type idx1 = hash1(key);
    for (const Node* node = buckets_[idx1].head.get(); node;
         node = node->next.get()) {
      if (equal_(node->data.first, key)) {
        return &node->data.second;
      }
    }

    size_type idx2 = hash2(key);
    if (idx2 != idx1) {
      for (const Node* node = buckets_[idx2].head.get(); node;
           node = node->next.get()) {
        if (equal_(node->data.first, key)) {
          return &node->data.second;
        }
      }
    }

    return nullptr;
  }

  constexpr auto contains(const Key& key) const -> bool {
    return find(key) != nullptr;
  }

  constexpr auto operator[](const Key& key) -> Value& {
    Value* existing = find(key);
    if (existing) {
      return *existing;
    }

    // Insert new
    if (needsResize()) {
      resize(capacity_ * 2);
    }

    size_type idx1 = hash1(key);
    size_type idx2 = hash2(key);

    // Two-choice: insert into less loaded bucket
    size_type target =
        (buckets_[idx1].count <= buckets_[idx2].count) ? idx1 : idx2;

    auto new_node = std::make_unique<Node>(key, Value{});
    Value& result = new_node->data.second;
    new_node->next = std::move(buckets_[target].head);
    buckets_[target].head = std::move(new_node);
    ++buckets_[target].count;
    ++size_;

    return result;
  }

  // --------------------------------------------------------------------------
  // Insertion
  // --------------------------------------------------------------------------
  // THE KEY INSIGHT: insert into the less loaded of two buckets

  constexpr auto insert(const Key& key, const Value& value)
      -> std::pair<Value*, bool> {
    Value* existing = find(key);
    if (existing) {
      return {existing, false};
    }

    if (needsResize()) {
      resize(capacity_ * 2);
    }

    size_type idx1 = hash1(key);
    size_type idx2 = hash2(key);

    // Two-choice: insert into less loaded bucket
    size_type target =
        (buckets_[idx1].count <= buckets_[idx2].count) ? idx1 : idx2;

    auto new_node = std::make_unique<Node>(key, value);
    Value* result = &new_node->data.second;
    new_node->next = std::move(buckets_[target].head);
    buckets_[target].head = std::move(new_node);
    ++buckets_[target].count;
    ++size_;

    return {result, true};
  }

  constexpr auto insert(const Key& key, Value&& value)
      -> std::pair<Value*, bool> {
    Value* existing = find(key);
    if (existing) {
      return {existing, false};
    }

    if (needsResize()) {
      resize(capacity_ * 2);
    }

    size_type idx1 = hash1(key);
    size_type idx2 = hash2(key);

    size_type target =
        (buckets_[idx1].count <= buckets_[idx2].count) ? idx1 : idx2;

    auto new_node = std::make_unique<Node>(key, std::move(value));
    Value* result = &new_node->data.second;
    new_node->next = std::move(buckets_[target].head);
    buckets_[target].head = std::move(new_node);
    ++buckets_[target].count;
    ++size_;

    return {result, true};
  }

  constexpr auto insertOrAssign(const Key& key, Value value) -> bool {
    Value* existing = find(key);
    if (existing) {
      *existing = std::move(value);
      return false;
    }

    if (needsResize()) {
      resize(capacity_ * 2);
    }

    size_type idx1 = hash1(key);
    size_type idx2 = hash2(key);

    size_type target =
        (buckets_[idx1].count <= buckets_[idx2].count) ? idx1 : idx2;

    auto new_node = std::make_unique<Node>(key, std::move(value));
    new_node->next = std::move(buckets_[target].head);
    buckets_[target].head = std::move(new_node);
    ++buckets_[target].count;
    ++size_;

    return true;
  }

  // --------------------------------------------------------------------------
  // Deletion
  // --------------------------------------------------------------------------

  constexpr auto erase(const Key& key) -> bool {
    // Try bucket 1
    size_type idx1 = hash1(key);
    if (eraseFromBucket(idx1, key)) {
      return true;
    }

    // Try bucket 2
    size_type idx2 = hash2(key);
    if (idx2 != idx1) {
      return eraseFromBucket(idx2, key);
    }

    return false;
  }

  constexpr void clear() {
    for (size_type i = 0; i < capacity_; ++i) {
      buckets_[i].head.reset();
      buckets_[i].count = 0;
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
    using pointer = std::pair<const Key, Value>*;
    using reference = std::pair<const Key, Value>&;

    constexpr Iterator(TwoChoiceMap* map, size_type bucket_idx, Node* node)
        : map_{map}, bucket_idx_{bucket_idx}, node_{node} {
      if (node_ == nullptr) {
        advanceToNextBucket();
      }
    }

    constexpr auto operator*() const -> reference { return node_->data; }

    constexpr auto operator->() const -> pointer { return &node_->data; }

    constexpr auto operator++() -> Iterator& {
      node_ = node_->next.get();
      if (node_ == nullptr) {
        ++bucket_idx_;
        advanceToNextBucket();
      }
      return *this;
    }

    constexpr auto operator++(int) -> Iterator {
      Iterator tmp = *this;
      ++*this;
      return tmp;
    }

    constexpr auto operator==(const Iterator& other) const -> bool {
      return node_ == other.node_ && bucket_idx_ == other.bucket_idx_;
    }

   private:
    constexpr void advanceToNextBucket() {
      while (bucket_idx_ < map_->capacity_ &&
             map_->buckets_[bucket_idx_].head == nullptr) {
        ++bucket_idx_;
      }
      if (bucket_idx_ < map_->capacity_) {
        node_ = map_->buckets_[bucket_idx_].head.get();
      } else {
        node_ = nullptr;
      }
    }

    TwoChoiceMap* map_;
    size_type bucket_idx_;
    Node* node_;
  };

  class ConstIterator {
   public:
    using difference_type = std::ptrdiff_t;
    using value_type = std::pair<const Key, Value>;
    using pointer = const std::pair<const Key, Value>*;
    using reference = const std::pair<const Key, Value>&;

    constexpr ConstIterator(const TwoChoiceMap* map, size_type bucket_idx,
                            const Node* node)
        : map_{map}, bucket_idx_{bucket_idx}, node_{node} {
      if (node_ == nullptr) {
        advanceToNextBucket();
      }
    }

    constexpr auto operator*() const -> reference { return node_->data; }

    constexpr auto operator->() const -> pointer { return &node_->data; }

    constexpr auto operator++() -> ConstIterator& {
      node_ = node_->next.get();
      if (node_ == nullptr) {
        ++bucket_idx_;
        advanceToNextBucket();
      }
      return *this;
    }

    constexpr auto operator++(int) -> ConstIterator {
      ConstIterator tmp = *this;
      ++*this;
      return tmp;
    }

    constexpr auto operator==(const ConstIterator& other) const -> bool {
      return node_ == other.node_ && bucket_idx_ == other.bucket_idx_;
    }

   private:
    constexpr void advanceToNextBucket() {
      while (bucket_idx_ < map_->capacity_ &&
             map_->buckets_[bucket_idx_].head == nullptr) {
        ++bucket_idx_;
      }
      if (bucket_idx_ < map_->capacity_) {
        node_ = map_->buckets_[bucket_idx_].head.get();
      } else {
        node_ = nullptr;
      }
    }

    const TwoChoiceMap* map_;
    size_type bucket_idx_;
    const Node* node_;
  };

  constexpr auto begin() -> Iterator {
    return {this, 0, buckets_ ? buckets_[0].head.get() : nullptr};
  }
  constexpr auto end() -> Iterator { return {this, capacity_, nullptr}; }
  constexpr auto begin() const -> ConstIterator {
    return {this, 0, buckets_ ? buckets_[0].head.get() : nullptr};
  }
  constexpr auto end() const -> ConstIterator {
    return {this, capacity_, nullptr};
  }

  // --------------------------------------------------------------------------
  // Statistics
  // --------------------------------------------------------------------------

  constexpr auto maxBucketSize() const -> size_type {
    size_type max_size = 0;
    for (size_type i = 0; i < capacity_; ++i) {
      max_size = std::max(max_size, buckets_[i].count);
    }
    return max_size;
  }

 private:
  // --------------------------------------------------------------------------
  // Internal Helpers
  // --------------------------------------------------------------------------

  constexpr auto needsResize() const -> bool {
    return size_ * kLoadFactorDenominator >= capacity_ * kLoadFactorNumerator;
  }

  constexpr auto hash1(const Key& key) const -> size_type {
    return hasher1_(key) % capacity_;
  }

  constexpr auto hash2(const Key& key) const -> size_type {
    return hasher2_(key) % capacity_;
  }

  constexpr auto eraseFromBucket(size_type idx, const Key& key) -> bool {
    std::unique_ptr<Node>* ptr = &buckets_[idx].head;

    while (*ptr != nullptr) {
      if (equal_((*ptr)->data.first, key)) {
        *ptr = std::move((*ptr)->next);
        --buckets_[idx].count;
        --size_;
        return true;
      }
      ptr = &(*ptr)->next;
    }

    return false;
  }

  constexpr void resize(size_type new_capacity) {
    new_capacity = std::max(new_capacity, kMinCapacity);

    auto old_buckets = std::move(buckets_);
    size_type old_capacity = capacity_;

    capacity_ = new_capacity;
    buckets_ = std::make_unique<Bucket[]>(capacity_);
    size_ = 0;

    for (size_type i = 0; i < old_capacity; ++i) {
      Node* node = old_buckets[i].head.get();
      while (node != nullptr) {
        insert(node->data.first, std::move(node->data.second));
        node = node->next.get();
      }
    }
  }

  // --------------------------------------------------------------------------
  // Member Variables
  // --------------------------------------------------------------------------

  size_type capacity_ = kMinCapacity;
  size_type size_ = 0;
  std::unique_ptr<Bucket[]> buckets_ = nullptr;

  [[no_unique_address]] Hash1 hasher1_{};
  [[no_unique_address]] Hash2 hasher2_{};
  [[no_unique_address]] KeyEqual equal_{};
};

}  // namespace tempura
