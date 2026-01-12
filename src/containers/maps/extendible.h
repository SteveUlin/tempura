#pragma once

// ============================================================================
// Extendible Hash Map
// ============================================================================
//
// Extendible hashing is a dynamic hash table that grows incrementally by
// splitting ONE bucket at a time, unlike standard hash tables that rehash
// everything when resizing.
//
// THE EXTENDIBLE HASHING PRINCIPLE
// ---------------------------------
// Key insight: separate the directory (index) from the data (buckets).
//
// Directory: Array of pointers to buckets, size is always 2^global_depth
// Buckets: Fixed-size containers for key-value pairs
//
// Each bucket has a local_depth indicating how many hash bits distinguish
// its entries. When local_depth < global_depth, MULTIPLE directory entries
// point to the same bucket.
//
// EXAMPLE: global_depth=2, bucket capacity=2
//
//   Directory (size 4):       Buckets:
//   [00] ───────────┐
//   [01] ─────┐     ├──→ Bucket A (local_depth=1, keys: 001, 101)
//   [10] ───┐ │     │
//   [11] ─┐ │ │     │
//         │ │ └─────┘
//         │ └─────────→ Bucket B (local_depth=2, keys: 010, 110)
//         └───────────→ Bucket C (local_depth=2, keys: 011, 111)
//
// Directory entries [00] and [01] both point to Bucket A because A only
// uses 1 bit (the least significant bit) to distinguish its keys.
//
// WHY IT WORKS
// ------------
// Lookup: Hash key → take top global_depth bits → index directory →
//         find bucket → linear search in bucket
//         Time: O(1) directory lookup + O(bucket_size) search
//
// Insert: If bucket has space, insert. If full:
//   1. If local_depth < global_depth: split bucket, redistribute entries,
//      update affected directory pointers (no directory doubling needed!)
//   2. If local_depth == global_depth: double directory FIRST, then split
//
// INCREMENTAL GROWTH
// ------------------
// Critical property: only ONE bucket splits at a time.
// Directory doubling is O(directory_size) but doesn't touch data.
// This gives smooth, predictable performance vs. all-at-once rehashing.
//
// DIRECTORY DOUBLING
// ------------------
// When doubling from size 2^d to 2^(d+1):
//   - Entry i becomes entries i and i + 2^d
//   - They initially point to the same bucket (bucket's local_depth < d+1)
//   - Cost: O(2^d) pointer copies, NO data movement
//
// BUCKET SPLITTING
// ----------------
// When bucket B with local_depth=d splits:
//   1. Create new bucket B'
//   2. Increment both local depths to d+1
//   3. Redistribute entries based on bit d+1
//   4. Update directory entries: half point to B, half to B'
//
// Example split (global_depth=2, Bucket A from above overflows):
//   Before: [00],[01] → A(local=1)  After: [00] → A₀(local=2)
//                                          [01] → A₁(local=2)
//
// USER PERSPECTIVE
// ----------------
// At most ONE page fault for directory, ONE for bucket.
// Insertion triggers split of ONE bucket (not full table rehash).
// Scales to billions of entries with logarithmic directory size.
//
// REFERENCES
// ----------
// Fagin, Nievergelt, Pippenger, Strong. "Extendible Hashing - A Fast Access
// Method for Dynamic Files." ACM TODS, 1979.
//
// Used in: GFS, ZFS, many database systems (ISAM indexes, etc.)
//
// ============================================================================

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <utility>
#include <vector>

#include "containers/maps/hasher.h"
#include "containers/maps/map_storage.h"
#include "meta/manual_lifetime.h"

namespace tempura {

template <typename Key, typename Value, Hasher<Key> Hash = std::hash<Key>,
          typename KeyEqual = std::equal_to<Key>>
class ExtendibleMap {
 public:
  using key_type = Key;
  using mapped_type = Value;
  using value_type = std::pair<const Key, Value>;
  using size_type = std::size_t;
  using hasher = Hash;
  using key_equal = KeyEqual;

  // --------------------------------------------------------------------------
  // Bucket Structure
  // --------------------------------------------------------------------------
  // Fixed-size bucket containing entries. Each bucket tracks its local_depth
  // to determine which directory entries point to it.

  static constexpr size_type kBucketCapacity = 8;  // Entries per bucket

  struct Bucket {
    size_type local_depth = 0;  // Number of hash bits this bucket uses
    size_type count = 0;        // Number of occupied entries

    struct Entry {
      enum class State : std::uint8_t {
        kEmpty,
        kOccupied,
      };

      State state = State::kEmpty;
      ManualLifetime<MapStorage<Key, Value>> storage;

      auto isEmpty() const -> bool { return state == State::kEmpty; }
      auto isOccupied() const -> bool { return state == State::kOccupied; }

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
        state = State::kOccupied;
      }

      void destroy() {
        if (state == State::kOccupied) {
          std::destroy_at(&storage.get().mutable_pair);
          storage.destruct();
        }
        state = State::kEmpty;
      }
    };

    Entry entries[kBucketCapacity];

    Bucket() = default;

    Bucket(const Bucket& other)
        : local_depth{other.local_depth}, count{other.count} {
      for (size_type i = 0; i < kBucketCapacity; ++i) {
        if (other.entries[i].isOccupied()) {
          entries[i].construct(*other.entries[i].data());
        }
      }
    }

    Bucket(Bucket&& other) noexcept
        : local_depth{other.local_depth}, count{other.count} {
      for (size_type i = 0; i < kBucketCapacity; ++i) {
        if (other.entries[i].isOccupied()) {
          entries[i].construct(std::move(*other.entries[i].mutableData()));
          other.entries[i].destroy();
        }
      }
      other.count = 0;
      other.local_depth = 0;
    }

    auto operator=(const Bucket& other) -> Bucket& {
      if (this != &other) {
        Bucket tmp{other};
        swap(tmp);
      }
      return *this;
    }

    auto operator=(Bucket&& other) noexcept -> Bucket& {
      if (this != &other) {
        Bucket tmp{std::move(other)};
        swap(tmp);
      }
      return *this;
    }

    ~Bucket() {
      for (size_type i = 0; i < kBucketCapacity; ++i) {
        entries[i].destroy();
      }
    }

    void swap(Bucket& other) noexcept {
      std::swap(local_depth, other.local_depth);
      std::swap(count, other.count);
      for (size_type i = 0; i < kBucketCapacity; ++i) {
        // Manual swap for entries
        bool this_occupied = entries[i].isOccupied();
        bool other_occupied = other.entries[i].isOccupied();

        if (this_occupied && other_occupied) {
          std::swap(*entries[i].mutableData(), *other.entries[i].mutableData());
        } else if (this_occupied) {
          other.entries[i].construct(std::move(*entries[i].mutableData()));
          entries[i].destroy();
        } else if (other_occupied) {
          entries[i].construct(std::move(*other.entries[i].mutableData()));
          other.entries[i].destroy();
        }
      }
    }

    auto isFull() const -> bool { return count == kBucketCapacity; }

    // Find entry index for key, or kBucketCapacity if not found
    auto findEntry(const Key& key, const KeyEqual& equal) const -> size_type {
      for (size_type i = 0; i < kBucketCapacity; ++i) {
        if (entries[i].isOccupied() && equal(entries[i].data()->first, key)) {
          return i;
        }
      }
      return kBucketCapacity;
    }

    // Insert into bucket (assumes space available)
    template <typename... Args>
    auto insertEntry(Args&&... args) -> size_type {
      for (size_type i = 0; i < kBucketCapacity; ++i) {
        if (entries[i].isEmpty()) {
          entries[i].construct(std::forward<Args>(args)...);
          ++count;
          return i;
        }
      }
      assert(false && "insertEntry: bucket full");
      std::unreachable();
    }

    auto eraseEntry(size_type idx) -> bool {
      if (idx >= kBucketCapacity || entries[idx].isEmpty()) {
        return false;
      }
      entries[idx].destroy();
      --count;
      return true;
    }
  };

  // --------------------------------------------------------------------------
  // Constants
  // --------------------------------------------------------------------------

  static constexpr size_type kMinCapacity = 8;
  static constexpr size_type kInitialGlobalDepth = 3;  // Start with 2^3 = 8 entries
  static constexpr size_type kMaxGlobalDepth = 60;     // Max 2^60 directory entries

  // --------------------------------------------------------------------------
  // Constructors
  // --------------------------------------------------------------------------

  constexpr ExtendibleMap() : ExtendibleMap{kInitialGlobalDepth} {}

  constexpr explicit ExtendibleMap(size_type initial_global_depth)
      : global_depth_{std::min(initial_global_depth, kMaxGlobalDepth)} {
    size_type dir_size = size_type{1} << global_depth_;
    directory_.resize(dir_size);

    // Initially, all directory entries point to a single bucket
    auto bucket = std::make_shared<Bucket>();
    bucket->local_depth = global_depth_;
    for (size_type i = 0; i < dir_size; ++i) {
      directory_[i] = bucket;
    }
  }

  constexpr ExtendibleMap(const ExtendibleMap& other)
      : global_depth_{other.global_depth_},
        size_{other.size_},
        directory_(other.directory_.size()) {
    // Deep copy buckets, avoiding duplicates
    for (size_type i = 0; i < directory_.size(); ++i) {
      if (!other.directory_[i]) {
        continue;
      }

      // Check if we already copied this bucket
      bool already_copied = false;
      for (size_type j = 0; j < i; ++j) {
        if (other.directory_[j] == other.directory_[i]) {
          directory_[i] = directory_[j];
          already_copied = true;
          break;
        }
      }

      if (!already_copied) {
        directory_[i] = std::make_shared<Bucket>(*other.directory_[i]);
      }
    }
  }

  constexpr ExtendibleMap(ExtendibleMap&& other) noexcept
      : global_depth_{other.global_depth_},
        size_{other.size_},
        directory_{std::move(other.directory_)} {
    other.size_ = 0;
    other.global_depth_ = kInitialGlobalDepth;
  }

  constexpr auto operator=(const ExtendibleMap& other) -> ExtendibleMap& {
    if (this != &other) {
      ExtendibleMap tmp{other};
      swap(tmp);
    }
    return *this;
  }

  constexpr auto operator=(ExtendibleMap&& other) noexcept -> ExtendibleMap& {
    if (this != &other) {
      ExtendibleMap tmp{std::move(other)};
      swap(tmp);
    }
    return *this;
  }

  constexpr ~ExtendibleMap() = default;

  constexpr void swap(ExtendibleMap& other) noexcept {
    std::swap(global_depth_, other.global_depth_);
    std::swap(size_, other.size_);
    std::swap(directory_, other.directory_);
    std::swap(hasher_, other.hasher_);
    std::swap(equal_, other.equal_);
  }

  // --------------------------------------------------------------------------
  // Capacity
  // --------------------------------------------------------------------------

  constexpr auto size() const noexcept -> size_type { return size_; }
  constexpr auto empty() const noexcept -> bool { return size_ == 0; }
  constexpr auto globalDepth() const noexcept -> size_type { return global_depth_; }
  constexpr auto directorySize() const noexcept -> size_type {
    return directory_.size();
  }

  // Count unique buckets
  constexpr auto bucketCount() const -> size_type {
    size_type count = 0;
    for (size_type i = 0; i < directory_.size(); ++i) {
      if (!directory_[i]) {
        continue;
      }
      // Check if this is the first occurrence of this bucket
      bool is_first = true;
      for (size_type j = 0; j < i; ++j) {
        if (directory_[j] == directory_[i]) {
          is_first = false;
          break;
        }
      }
      if (is_first) {
        ++count;
      }
    }
    return count;
  }

  // --------------------------------------------------------------------------
  // Lookup
  // --------------------------------------------------------------------------
  // Use top global_depth bits of hash to index directory → bucket → search

  constexpr auto find(const Key& key) -> Value* {
    size_type bucket_idx = directoryIndex(key);
    auto& bucket = directory_[bucket_idx];
    if (!bucket) {
      return nullptr;
    }

    size_type entry_idx = bucket->findEntry(key, equal_);
    if (entry_idx == kBucketCapacity) {
      return nullptr;
    }

    return &bucket->entries[entry_idx].data()->second;
  }

  constexpr auto find(const Key& key) const -> const Value* {
    size_type bucket_idx = directoryIndex(key);
    auto& bucket = directory_[bucket_idx];
    if (!bucket) {
      return nullptr;
    }

    size_type entry_idx = bucket->findEntry(key, equal_);
    if (entry_idx == kBucketCapacity) {
      return nullptr;
    }

    return &bucket->entries[entry_idx].data()->second;
  }

  constexpr auto contains(const Key& key) const -> bool {
    return find(key) != nullptr;
  }

  constexpr auto operator[](const Key& key) -> Value& {
    auto [bucket_idx, entry_idx, inserted] = insertEntry(key);
    return directory_[bucket_idx]->entries[entry_idx].data()->second;
  }

  // --------------------------------------------------------------------------
  // Insertion
  // --------------------------------------------------------------------------

  constexpr auto insert(const Key& key, const Value& value)
      -> std::pair<Value*, bool> {
    auto [bucket_idx, entry_idx, inserted] = insertEntry(key);
    if (inserted) {
      directory_[bucket_idx]->entries[entry_idx].data()->second = value;
    }
    return {&directory_[bucket_idx]->entries[entry_idx].data()->second,
            inserted};
  }

  constexpr auto insert(const Key& key, Value&& value)
      -> std::pair<Value*, bool> {
    auto [bucket_idx, entry_idx, inserted] = insertEntry(key);
    if (inserted) {
      directory_[bucket_idx]->entries[entry_idx].data()->second =
          std::move(value);
    }
    return {&directory_[bucket_idx]->entries[entry_idx].data()->second,
            inserted};
  }

  constexpr auto insertOrAssign(const Key& key, Value value) -> bool {
    auto [bucket_idx, entry_idx, inserted] = insertEntry(key);
    directory_[bucket_idx]->entries[entry_idx].data()->second =
        std::move(value);
    return inserted;
  }

  // --------------------------------------------------------------------------
  // Deletion
  // --------------------------------------------------------------------------

  constexpr auto erase(const Key& key) -> bool {
    size_type bucket_idx = directoryIndex(key);
    auto& bucket = directory_[bucket_idx];
    if (!bucket) {
      return false;
    }

    size_type entry_idx = bucket->findEntry(key, equal_);
    if (entry_idx == kBucketCapacity) {
      return false;
    }

    bucket->eraseEntry(entry_idx);
    --size_;
    return true;
  }

  constexpr void clear() {
    // Reset to initial state
    global_depth_ = kInitialGlobalDepth;
    size_type dir_size = size_type{1} << global_depth_;
    directory_.clear();
    directory_.resize(dir_size);

    auto bucket = std::make_shared<Bucket>();
    bucket->local_depth = global_depth_;
    for (size_type i = 0; i < dir_size; ++i) {
      directory_[i] = bucket;
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
    using pointer = value_type*;
    using reference = value_type&;

    constexpr Iterator(ExtendibleMap* map, size_type bucket_idx,
                       size_type entry_idx)
        : map_{map}, bucket_idx_{bucket_idx}, entry_idx_{entry_idx} {
      advanceToOccupied();
    }

    constexpr auto operator*() const -> reference {
      return *map_->directory_[bucket_idx_]->entries[entry_idx_].data();
    }

    constexpr auto operator->() const -> pointer {
      return map_->directory_[bucket_idx_]->entries[entry_idx_].data();
    }

    constexpr auto operator++() -> Iterator& {
      ++entry_idx_;
      advanceToOccupied();
      return *this;
    }

    constexpr auto operator++(int) -> Iterator {
      Iterator tmp = *this;
      ++*this;
      return tmp;
    }

    constexpr auto operator==(const Iterator& other) const -> bool {
      return bucket_idx_ == other.bucket_idx_ &&
             entry_idx_ == other.entry_idx_;
    }

   private:
    constexpr void advanceToOccupied() {
      while (bucket_idx_ < map_->directory_.size()) {
        auto& bucket = map_->directory_[bucket_idx_];

        // Skip buckets we've already visited (shared pointers)
        if (bucket && bucket_idx_ > 0) {
          bool already_visited = false;
          for (size_type i = 0; i < bucket_idx_; ++i) {
            if (map_->directory_[i] == bucket) {
              already_visited = true;
              break;
            }
          }
          if (already_visited) {
            ++bucket_idx_;
            entry_idx_ = 0;
            continue;
          }
        }

        if (bucket) {
          while (entry_idx_ < kBucketCapacity) {
            if (bucket->entries[entry_idx_].isOccupied()) {
              return;
            }
            ++entry_idx_;
          }
        }

        ++bucket_idx_;
        entry_idx_ = 0;
      }
    }

    ExtendibleMap* map_;
    size_type bucket_idx_;
    size_type entry_idx_;
  };

  class ConstIterator {
   public:
    using difference_type = std::ptrdiff_t;
    using value_type = std::pair<const Key, Value>;
    using pointer = const value_type*;
    using reference = const value_type&;

    constexpr ConstIterator(const ExtendibleMap* map, size_type bucket_idx,
                            size_type entry_idx)
        : map_{map}, bucket_idx_{bucket_idx}, entry_idx_{entry_idx} {
      advanceToOccupied();
    }

    constexpr auto operator*() const -> reference {
      return *map_->directory_[bucket_idx_]->entries[entry_idx_].data();
    }

    constexpr auto operator->() const -> pointer {
      return map_->directory_[bucket_idx_]->entries[entry_idx_].data();
    }

    constexpr auto operator++() -> ConstIterator& {
      ++entry_idx_;
      advanceToOccupied();
      return *this;
    }

    constexpr auto operator++(int) -> ConstIterator {
      ConstIterator tmp = *this;
      ++*this;
      return tmp;
    }

    constexpr auto operator==(const ConstIterator& other) const -> bool {
      return bucket_idx_ == other.bucket_idx_ &&
             entry_idx_ == other.entry_idx_;
    }

   private:
    constexpr void advanceToOccupied() {
      while (bucket_idx_ < map_->directory_.size()) {
        auto& bucket = map_->directory_[bucket_idx_];

        // Skip buckets we've already visited (shared pointers)
        if (bucket && bucket_idx_ > 0) {
          bool already_visited = false;
          for (size_type i = 0; i < bucket_idx_; ++i) {
            if (map_->directory_[i] == bucket) {
              already_visited = true;
              break;
            }
          }
          if (already_visited) {
            ++bucket_idx_;
            entry_idx_ = 0;
            continue;
          }
        }

        if (bucket) {
          while (entry_idx_ < kBucketCapacity) {
            if (bucket->entries[entry_idx_].isOccupied()) {
              return;
            }
            ++entry_idx_;
          }
        }

        ++bucket_idx_;
        entry_idx_ = 0;
      }
    }

    const ExtendibleMap* map_;
    size_type bucket_idx_;
    size_type entry_idx_;
  };

  constexpr auto begin() -> Iterator { return {this, 0, 0}; }
  constexpr auto end() -> Iterator {
    return {this, directory_.size(), 0};
  }
  constexpr auto begin() const -> ConstIterator { return {this, 0, 0}; }
  constexpr auto end() const -> ConstIterator {
    return {this, directory_.size(), 0};
  }

 private:
  // --------------------------------------------------------------------------
  // Internal Helpers
  // --------------------------------------------------------------------------

  // Get directory index from hash (top global_depth bits)
  constexpr auto directoryIndex(const Key& key) const -> size_type {
    size_type hash = hasher_(key);
    // Use top global_depth bits by shifting right
    size_type mask = (size_type{1} << global_depth_) - 1;
    return hash & mask;
  }

  // Insert entry and handle bucket splitting if needed
  // Returns: (bucket_idx, entry_idx, was_inserted)
  constexpr auto insertEntry(const Key& key)
      -> std::tuple<size_type, size_type, bool> {
    // Use iteration to avoid stack overflow on pathological cases
    while (true) {
      size_type bucket_idx = directoryIndex(key);
      auto& bucket = directory_[bucket_idx];

      if (!bucket) {
        bucket = std::make_shared<Bucket>();
        bucket->local_depth = global_depth_;
      }

      // Check if key already exists
      size_type existing = bucket->findEntry(key, equal_);
      if (existing != kBucketCapacity) {
        return {bucket_idx, existing, false};
      }

      // Try to insert in current bucket
      if (!bucket->isFull()) {
        size_type entry_idx = bucket->insertEntry(key, Value{});
        ++size_;
        return {bucket_idx, entry_idx, true};
      }

      // Bucket is full - need to split
      splitBucket(bucket_idx);
      // Loop continues to retry insertion
    }
  }

  // Split a full bucket
  constexpr void splitBucket(size_type overflow_idx) {
    auto old_bucket_ptr = directory_[overflow_idx];
    assert(old_bucket_ptr && "splitBucket: null bucket");
    assert(old_bucket_ptr->isFull() && "splitBucket: bucket not full");

    // If local_depth == global_depth, must double directory first
    if (old_bucket_ptr->local_depth == global_depth_) {
      doubleDirectory();
    }

    size_type old_depth = old_bucket_ptr->local_depth;
    size_type new_depth = old_depth + 1;

    // Pathological case: if we've reached max depth, we can't split further
    // This can happen with hash collisions where all keys map to same bits
    if (new_depth > global_depth_) {
      assert(false && "splitBucket: cannot split beyond global depth");
      std::unreachable();
    }

    // Create two new buckets to replace the old one
    auto bucket_0 = std::make_shared<Bucket>();
    auto bucket_1 = std::make_shared<Bucket>();
    bucket_0->local_depth = new_depth;
    bucket_1->local_depth = new_depth;

    // Bit mask for the new split bit
    size_type split_bit = size_type{1} << old_depth;

    // Redistribute entries from old bucket to the two new buckets
    for (size_type i = 0; i < kBucketCapacity; ++i) {
      if (old_bucket_ptr->entries[i].isOccupied()) {
        auto [key, value] = std::move(*old_bucket_ptr->entries[i].mutableData());
        size_type hash = hasher_(key);
        if (hash & split_bit) {
          bucket_1->insertEntry(std::move(key), std::move(value));
        } else {
          bucket_0->insertEntry(std::move(key), std::move(value));
        }
      }
    }

    // Update directory pointers
    // All entries that pointed to old_bucket now point to bucket_0 or bucket_1
    for (size_type i = 0; i < directory_.size(); ++i) {
      if (directory_[i] == old_bucket_ptr) {
        if (i & split_bit) {
          directory_[i] = bucket_1;
        } else {
          directory_[i] = bucket_0;
        }
      }
    }

    // Sanity check: if all entries went to one bucket, we have a problem
    // This shouldn't happen with a good hash function, but check anyway
    assert((bucket_0->count > 0 || bucket_1->count > 0) &&
           "splitBucket: all entries in one bucket after split");
  }

  // Double the directory size (global_depth++)
  constexpr void doubleDirectory() {
    assert(global_depth_ < kMaxGlobalDepth &&
           "doubleDirectory: max depth reached");

    ++global_depth_;
    size_type old_size = directory_.size();
    size_type new_size = size_type{1} << global_depth_;

    directory_.resize(new_size);

    // Copy pointers: entry i becomes entries i and i + old_size
    for (size_type i = 0; i < old_size; ++i) {
      directory_[i + old_size] = directory_[i];
    }
  }

  // --------------------------------------------------------------------------
  // Member Variables
  // --------------------------------------------------------------------------

  size_type global_depth_ = kInitialGlobalDepth;
  size_type size_ = 0;
  std::vector<std::shared_ptr<Bucket>> directory_;

  [[no_unique_address]] Hash hasher_{};
  [[no_unique_address]] KeyEqual equal_{};
};

}  // namespace tempura
