#pragma once

// ============================================================================
// Separate Chaining Hash Map
// ============================================================================
//
// A hash map is an associative container that maps keys to values. This
// implementation uses "separate chaining" (also called "open hashing" or
// "closed addressing") as its collision resolution strategy.
//
// THE BASIC IDEA
// --------------
// 1. We maintain an array of "buckets" (slots)
// 2. Each bucket is the head of a singly-linked list
// 3. To insert key K: compute hash(K) % bucket_count to get the target bucket
// 4. Add the key-value pair to that bucket's linked list
// 5. To find a key: go to the bucket, then linear search through its list
//
// EXAMPLE: Inserting keys with hash values 5, 12, 5, 6 into 7 buckets
//
//   Insert hash=5:  bucket 5, list: [K1]
//   Insert hash=12: 12%7=5, bucket 5, list: [K2 → K1]
//   Insert hash=5:  bucket 5, list: [K3 → K2 → K1]
//   Insert hash=6:  bucket 6, list: [K4]
//
//   Array:  [0]  [1]  [2]  [3]  [4]    [5]           [6]
//            ∅    ∅    ∅    ∅    ∅   K3→K2→K1        K4
//
// COMPARISON WITH OPEN ADDRESSING
// -------------------------------
// In open addressing (linear probing, quadratic probing, double hashing),
// all elements live directly in the array, and collisions probe other slots.
//
// Separate chaining takes a different approach: colliding elements share a
// bucket via a linked list. This has important implications:
//
// PROS:
//   - No clustering: each bucket is independent, performance doesn't degrade
//     in pathological patterns like open addressing does
//   - Load factor can exceed 1.0: we can have more elements than buckets,
//     lists just get longer
//   - Simple deletion: remove node from list, no tombstones needed
//   - Stable pointers: unlike open addressing, pointers to values remain valid
//     even after insertions (only rehash invalidates them)
//   - Graceful degradation: performance degrades linearly with load factor,
//     not quadratically like linear probing
//
// CONS:
//   - Memory overhead: each node has a pointer to the next node
//   - Cache-unfriendly: following pointers means cache misses
//   - More allocations: each insertion allocates a new node
//   - Worse constant factors for small tables due to pointer overhead
//
// LOAD FACTOR
// -----------
// Load factor α = size / bucket_count. Unlike open addressing where α < 1
// is required, separate chaining can have α > 1.
//
//   α = 0.5: Average 0.5 comparisons per lookup (very fast)
//   α = 1.0: Average 1 comparison (reasonable default)
//   α = 2.0: Average 2 comparisons (still fine for small keys)
//   α = 10.0: Average 10 comparisons (getting slow)
//
// Expected chain length = α (assuming uniform hashing)
// Expected probes for successful search ≈ 1 + α/2
// Expected probes for unsuccessful search ≈ 1 + α (just α comparisons)
//
// We resize when α > 1.0 by default. This is more lenient than open
// addressing (~0.7) because:
//   1. Performance degrades linearly, not quadratically
//   2. Higher α trades speed for memory efficiency
//   3. No catastrophic failure mode at high load factors
//
// DELETION
// --------
// Unlike open addressing, deletion is straightforward. We simply find the
// node in the bucket's linked list and remove it. No tombstones are needed
// because the linked list structure doesn't rely on contiguous occupancy.
//
// If key K is in the list: [A → K → B → null]
// After deleting K:        [A → B → null]
//
// This simplicity is one of the main advantages of separate chaining.
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

// ============================================================================
// SeparateChainingMap
// ============================================================================

template <typename Key, typename Value, Hasher<Key> Hash = std::hash<Key>,
          typename KeyEqual = std::equal_to<Key>>
class SeparateChainingMap {
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
  // Node Structure
  // --------------------------------------------------------------------------
  // Each node in the linked list contains a key-value pair and a pointer
  // to the next node. Unlike open addressing maps, we store pair<const Key, Value>
  // directly since nodes are individually allocated and never moved during resize.

  struct Node {
    std::pair<const Key, Value> data;
    Node* next = nullptr;

    template <typename K, typename V>
    Node(K&& k, V&& v, Node* n = nullptr)
        : data{std::forward<K>(k), std::forward<V>(v)}, next{n} {}
  };

  // --------------------------------------------------------------------------
  // Constants
  // --------------------------------------------------------------------------

  // Load factor threshold: resize when size > bucket_count
  // This is α = 1.0, more lenient than open addressing's ~0.7
  static constexpr double kMaxLoadFactor = 1.0;

  // Minimum bucket count. Must be > 0.
  static constexpr size_type kMinBucketCount = 8;

  // --------------------------------------------------------------------------
  // Constructors
  // --------------------------------------------------------------------------

  constexpr SeparateChainingMap() : SeparateChainingMap{kMinBucketCount} {}

  constexpr explicit SeparateChainingMap(size_type initial_bucket_count)
      : bucket_count_{std::max(initial_bucket_count, kMinBucketCount)},
        buckets_{std::make_unique<Node*[]>(bucket_count_)} {}

  // Copy constructor: deep copy all nodes
  constexpr SeparateChainingMap(const SeparateChainingMap& other)
      : bucket_count_{other.bucket_count_},
        size_{other.size_},
        buckets_{std::make_unique<Node*[]>(bucket_count_)} {
    for (size_type i = 0; i < bucket_count_; ++i) {
      Node* src = other.buckets_[i];
      Node** dst = &buckets_[i];
      while (src != nullptr) {
        *dst = new Node{src->data.first, src->data.second};
        dst = &((*dst)->next);
        src = src->next;
      }
    }
  }

  // Move constructor: steal resources
  constexpr SeparateChainingMap(SeparateChainingMap&& other) noexcept
      : bucket_count_{other.bucket_count_},
        size_{other.size_},
        buckets_{std::move(other.buckets_)} {
    other.size_ = 0;
    other.bucket_count_ = 0;
  }

  // Copy assignment using copy-and-swap for exception safety
  constexpr auto operator=(const SeparateChainingMap& other)
      -> SeparateChainingMap& {
    if (this != &other) {
      SeparateChainingMap tmp{other};
      swap(tmp);
    }
    return *this;
  }

  // Move assignment using swap
  constexpr auto operator=(SeparateChainingMap&& other) noexcept
      -> SeparateChainingMap& {
    if (this != &other) {
      SeparateChainingMap tmp{std::move(other)};
      swap(tmp);
    }
    return *this;
  }

  constexpr ~SeparateChainingMap() {
    if (buckets_) {
      for (size_type i = 0; i < bucket_count_; ++i) {
        Node* node = buckets_[i];
        while (node != nullptr) {
          Node* next = node->next;
          delete node;
          node = next;
        }
      }
    }
  }

  // Swap two maps (used by copy-and-swap idiom)
  constexpr void swap(SeparateChainingMap& other) noexcept {
    std::swap(bucket_count_, other.bucket_count_);
    std::swap(size_, other.size_);
    std::swap(buckets_, other.buckets_);
    std::swap(hasher_, other.hasher_);
    std::swap(equal_, other.equal_);
  }

  // --------------------------------------------------------------------------
  // Capacity
  // --------------------------------------------------------------------------

  constexpr auto size() const noexcept -> size_type { return size_; }

  constexpr auto bucketCount() const noexcept -> size_type {
    return bucket_count_;
  }

  // Alias for compatibility with open addressing implementations
  constexpr auto capacity() const noexcept -> size_type {
    return bucket_count_;
  }

  constexpr auto empty() const noexcept -> bool { return size_ == 0; }

  constexpr auto loadFactor() const noexcept -> double {
    return static_cast<double>(size_) / static_cast<double>(bucket_count_);
  }

  // --------------------------------------------------------------------------
  // Lookup
  // --------------------------------------------------------------------------

  // Find a key and return pointer to its value, or nullptr if not found.
  //
  // Algorithm:
  //   1. Compute bucket index: hash(key) % bucket_count
  //   2. Walk the linked list in that bucket
  //   3. Return pointer to value if found, nullptr otherwise
  //
  // Time complexity: O(1 + α) average, O(n) worst case (all keys in one bucket)

  constexpr auto find(const Key& key) -> Value* {
    Node* node = findNode(key);
    return node != nullptr ? &node->data.second : nullptr;
  }

  constexpr auto find(const Key& key) const -> const Value* {
    const Node* node = findNode(key);
    return node != nullptr ? &node->data.second : nullptr;
  }

  constexpr auto contains(const Key& key) const -> bool {
    return findNode(key) != nullptr;
  }

  // Operator[] with auto-insertion of default value (like std::map)
  constexpr auto operator[](const Key& key) -> Value& {
    auto [ptr, inserted] = insertImpl(key, Value{});
    return *ptr;
  }

  // --------------------------------------------------------------------------
  // Insertion
  // --------------------------------------------------------------------------

  // Insert a key-value pair. Returns {pointer_to_value, true} if inserted,
  // {pointer_to_value, false} if key already existed.
  //
  // Algorithm:
  //   1. Check load factor; resize if necessary
  //   2. Compute bucket index
  //   3. Search the bucket's list for existing key
  //   4. If not found, prepend new node to list (O(1) insertion)
  //
  // We prepend rather than append for simplicity and efficiency: no need
  // to traverse to the end of the list.

  constexpr auto insert(const Key& key, const Value& value)
      -> std::pair<Value*, bool> {
    return insertImpl(key, value);
  }

  constexpr auto insert(const Key& key, Value&& value)
      -> std::pair<Value*, bool> {
    return insertImpl(key, std::move(value));
  }

  // Insert or update: always sets the value, returns true if new insertion
  constexpr auto insertOrAssign(const Key& key, Value value) -> bool {
    auto [ptr, inserted] = insertImpl(key, std::move(value));
    if (!inserted) {
      *ptr = std::move(value);
    }
    return inserted;
  }

  // --------------------------------------------------------------------------
  // Deletion
  // --------------------------------------------------------------------------

  // Erase a key. Returns true if the key was found and removed.
  //
  // Algorithm:
  //   1. Compute bucket index
  //   2. Search for the node with matching key
  //   3. Unlink it from the list and delete it
  //
  // Unlike open addressing, we don't need tombstones. The linked list
  // structure allows clean removal without breaking search chains.

  constexpr auto erase(const Key& key) -> bool {
    size_type bucket = bucketIndex(key);
    Node** ptr = &buckets_[bucket];

    while (*ptr != nullptr) {
      if (equal_((*ptr)->data.first, key)) {
        Node* to_delete = *ptr;
        *ptr = (*ptr)->next;  // Unlink
        delete to_delete;
        --size_;
        return true;
      }
      ptr = &((*ptr)->next);
    }

    return false;  // Key not found
  }

  // Clear all elements
  constexpr void clear() {
    for (size_type i = 0; i < bucket_count_; ++i) {
      Node* node = buckets_[i];
      while (node != nullptr) {
        Node* next = node->next;
        delete node;
        node = next;
      }
      buckets_[i] = nullptr;
    }
    size_ = 0;
  }

  // --------------------------------------------------------------------------
  // Iterator Support
  // --------------------------------------------------------------------------
  // We provide a forward iterator that walks through all buckets and their
  // linked lists. This is less cache-friendly than open addressing iterators
  // but provides stable iteration order within buckets.

  class Iterator {
   public:
    using difference_type = std::ptrdiff_t;
    using value_type = std::pair<const Key, Value>;
    using pointer = value_type*;
    using reference = value_type&;

    constexpr Iterator(SeparateChainingMap* map, size_type bucket, Node* node)
        : map_{map}, bucket_{bucket}, node_{node} {
      advanceToValid();
    }

    constexpr auto operator*() const -> reference { return node_->data; }

    constexpr auto operator->() const -> pointer { return &node_->data; }

    constexpr auto operator++() -> Iterator& {
      node_ = node_->next;
      advanceToValid();
      return *this;
    }

    constexpr auto operator++(int) -> Iterator {
      Iterator tmp = *this;
      ++*this;
      return tmp;
    }

    constexpr auto operator==(const Iterator& other) const -> bool {
      return node_ == other.node_ && bucket_ == other.bucket_;
    }

   private:
    constexpr void advanceToValid() {
      // If current node is null, find next non-empty bucket
      while (node_ == nullptr && bucket_ < map_->bucket_count_) {
        ++bucket_;
        if (bucket_ < map_->bucket_count_) {
          node_ = map_->buckets_[bucket_];
        }
      }
    }

    SeparateChainingMap* map_;
    size_type bucket_;
    Node* node_;
  };

  class ConstIterator {
   public:
    using difference_type = std::ptrdiff_t;
    using value_type = std::pair<const Key, Value>;
    using pointer = const value_type*;
    using reference = const value_type&;

    constexpr ConstIterator(const SeparateChainingMap* map, size_type bucket,
                            const Node* node)
        : map_{map}, bucket_{bucket}, node_{node} {
      advanceToValid();
    }

    constexpr auto operator*() const -> reference { return node_->data; }

    constexpr auto operator->() const -> pointer { return &node_->data; }

    constexpr auto operator++() -> ConstIterator& {
      node_ = node_->next;
      advanceToValid();
      return *this;
    }

    constexpr auto operator++(int) -> ConstIterator {
      ConstIterator tmp = *this;
      ++*this;
      return tmp;
    }

    constexpr auto operator==(const ConstIterator& other) const -> bool {
      return node_ == other.node_ && bucket_ == other.bucket_;
    }

   private:
    constexpr void advanceToValid() {
      while (node_ == nullptr && bucket_ < map_->bucket_count_) {
        ++bucket_;
        if (bucket_ < map_->bucket_count_) {
          node_ = map_->buckets_[bucket_];
        }
      }
    }

    const SeparateChainingMap* map_;
    size_type bucket_;
    const Node* node_;
  };

  constexpr auto begin() -> Iterator {
    return {this, 0, bucket_count_ > 0 ? buckets_[0] : nullptr};
  }

  constexpr auto end() -> Iterator { return {this, bucket_count_, nullptr}; }

  constexpr auto begin() const -> ConstIterator {
    return {this, 0, bucket_count_ > 0 ? buckets_[0] : nullptr};
  }

  constexpr auto end() const -> ConstIterator {
    return {this, bucket_count_, nullptr};
  }

 private:
  // --------------------------------------------------------------------------
  // Internal Helpers
  // --------------------------------------------------------------------------

  // Check if resize is needed
  constexpr auto needsResize() const -> bool {
    return size_ >= bucket_count_;  // α >= 1.0
  }

  // Compute the bucket index for a key
  constexpr auto bucketIndex(const Key& key) const -> size_type {
    return hasher_(key) % bucket_count_;
  }

  // Find the node containing a key, or nullptr if not found
  constexpr auto findNode(const Key& key) const -> Node* {
    size_type bucket = bucketIndex(key);
    Node* node = buckets_[bucket];

    while (node != nullptr) {
      if (equal_(node->data.first, key)) {
        return node;
      }
      node = node->next;
    }

    return nullptr;
  }

  // Insert implementation
  template <typename V>
  constexpr auto insertImpl(const Key& key, V&& value)
      -> std::pair<Value*, bool> {
    // Check if resize is needed
    if (needsResize()) {
      size_type new_count = bucket_count_ * 2;
      // Assert on overflow - silent failure is unacceptable
      assert(new_count > bucket_count_ && "insertImpl: bucket count overflow");
      resize(new_count);
    }

    size_type bucket = bucketIndex(key);

    // Search for existing key
    Node* node = buckets_[bucket];
    while (node != nullptr) {
      if (equal_(node->data.first, key)) {
        // Key already exists
        return {&node->data.second, false};
      }
      node = node->next;
    }

    // Key not found, prepend new node
    buckets_[bucket] =
        new Node{key, std::forward<V>(value), buckets_[bucket]};
    ++size_;
    return {&buckets_[bucket]->data.second, true};
  }

  // Resize the table to new_bucket_count.
  //
  // Unlike open addressing (which cleans up tombstones), we simply
  // redistribute all nodes into new buckets. Each node gets rehashed.
  //
  // Note: We reuse the existing nodes rather than allocating new ones,
  // which saves allocations during rehashing.

  constexpr void resize(size_type new_bucket_count) {
    new_bucket_count = std::max(new_bucket_count, kMinBucketCount);

    auto old_buckets = std::move(buckets_);
    size_type old_bucket_count = bucket_count_;

    // Allocate new bucket array
    bucket_count_ = new_bucket_count;
    buckets_ = std::make_unique<Node*[]>(bucket_count_);

    // Rehash all nodes (reuse existing nodes, just relink)
    for (size_type i = 0; i < old_bucket_count; ++i) {
      Node* node = old_buckets[i];
      while (node != nullptr) {
        Node* next = node->next;

        // Compute new bucket and prepend
        size_type new_bucket = hasher_(node->data.first) % bucket_count_;
        node->next = buckets_[new_bucket];
        buckets_[new_bucket] = node;

        node = next;
      }
    }
    // old_buckets automatically freed when unique_ptr goes out of scope
  }

  // --------------------------------------------------------------------------
  // Member Variables
  // --------------------------------------------------------------------------

  size_type bucket_count_ = kMinBucketCount;       // Number of buckets
  size_type size_ = 0;                            // Number of elements
  std::unique_ptr<Node*[]> buckets_ = nullptr;    // Array of bucket heads

  [[no_unique_address]] Hash hasher_{};       // Hash function object
  [[no_unique_address]] KeyEqual equal_{};    // Key equality function object
};

}  // namespace tempura
