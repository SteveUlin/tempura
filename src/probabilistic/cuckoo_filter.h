#pragma once

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <random>
#include <vector>

namespace tempura {

// ============================================================================
// Cuckoo Filter
// ============================================================================
//
// A cuckoo filter is a space-efficient probabilistic data structure for
// approximate set membership testing, similar to a Bloom filter but with
// support for deletion and better locality.
//
// STRUCTURE
// ---------
// The filter stores fingerprints (small hashes) in a table of buckets.
// Each bucket holds BucketSize fingerprints. When an item is inserted:
//
//   1. Compute fingerprint fp = hash(item) truncated to FingerprintBits
//   2. Compute two candidate buckets: i1 = hash(item) mod NumBuckets
//                                     i2 = i1 XOR hash(fp)
//   3. If either bucket has space, store fp there
//   4. Otherwise, evict a random fingerprint and relocate it using the
//      XOR trick (cuckoo hashing)
//
// THE XOR TRICK
// -------------
// The key insight is that bucket indices are related by XOR:
//
//   i1 = i2 XOR hash(fp)
//   i2 = i1 XOR hash(fp)
//
// This means given any bucket index and the fingerprint, we can compute
// the alternate bucket WITHOUT storing the original item. This enables:
//   - Deletion (just remove the fingerprint)
//   - Relocation during eviction (compute alternate from current position)
//
// CUCKOO EVICTION
// ---------------
// When both candidate buckets are full, we evict a random fingerprint from
// one bucket and try to relocate it to its alternate bucket. This may
// cascade, causing multiple relocations. We limit to kMaxKicks relocations
// to prevent infinite loops when the filter is too full.
//
//   insert(x):
//     fp = fingerprint(x)
//     i1, i2 = getBuckets(x, fp)
//     if bucket[i1] or bucket[i2] has space:
//       store fp and return success
//
//     pick random i from {i1, i2}
//     for kMaxKicks iterations:
//       swap fp with random entry in bucket[i]
//       i = i XOR hash(fp)  // compute alternate bucket
//       if bucket[i] has space:
//         store fp and return success
//     return failure (filter is full)
//
// FALSE POSITIVES
// ---------------
// Like Bloom filters, cuckoo filters can have false positives:
//   - If two different items have the same fingerprint AND map to
//     overlapping bucket pairs, a false positive occurs
//   - False positive rate ≈ 2 * BucketSize / 2^FingerprintBits
//   - With BucketSize=4 and FingerprintBits=12: FPR ≈ 8/4096 ≈ 0.2%
//
// FALSE NEGATIVES
// ---------------
// No false negatives: if contains() returns false, the item was definitely
// never inserted (or was deleted).
//
// WHY USE A CUCKOO FILTER?
// ------------------------
// vs Bloom Filter:
//   + Supports deletion (Bloom filters cannot delete without counting)
//   + Better cache locality (only 2 memory accesses vs k for k hash functions)
//   + Often more space-efficient at low false positive rates
//   - Insertion can fail if filter is too full (~95% capacity limit)
//   - Slightly more complex implementation
//
// vs Hash Set:
//   + Much more space-efficient (only stores fingerprints, not full items)
//   + O(1) operations with good constants
//   - Cannot retrieve the original items
//   - Probabilistic (has false positives)
//
// TEMPLATE PARAMETERS
// -------------------
//   NumBuckets     - Number of buckets in the filter (should be power of 2)
//   BucketSize     - Number of fingerprints per bucket (default 4)
//   FingerprintBits - Bits per fingerprint (default 12, max 32)
//
// COMPLEXITY
// ----------
//   insert:    O(1) average, O(kMaxKicks) worst case
//   contains:  O(1) (exactly 2 bucket lookups)
//   erase:     O(1) (exactly 2 bucket lookups)
//   Space:     NumBuckets * BucketSize * FingerprintBits bits
//
template <std::size_t NumBuckets, std::size_t BucketSize = 4,
          std::size_t FingerprintBits = 12>
class CuckooFilter {
  static_assert(NumBuckets > 0, "NumBuckets must be positive");
  static_assert(BucketSize > 0, "BucketSize must be positive");
  static_assert(FingerprintBits > 0 && FingerprintBits <= 32,
                "FingerprintBits must be in [1, 32]");
  static_assert((NumBuckets & (NumBuckets - 1)) == 0,
                "NumBuckets should be a power of 2 for uniform distribution");

public:
  // Fingerprint type: use smallest type that fits
  using Fingerprint = std::conditional_t<
      FingerprintBits <= 8, std::uint8_t,
      std::conditional_t<FingerprintBits <= 16, std::uint16_t, std::uint32_t>>;

  static constexpr Fingerprint kFingerprintMask =
      static_cast<Fingerprint>((1ULL << FingerprintBits) - 1);

  // 0 is reserved to indicate empty slot
  static constexpr Fingerprint kEmptySlot = 0;

  // Maximum eviction attempts before declaring filter full
  static constexpr std::size_t kMaxKicks = 500;

  struct Bucket {
    std::array<Fingerprint, BucketSize> slots{};

    // Find empty slot and insert fingerprint. Returns true if successful.
    auto tryInsert(Fingerprint fp) -> bool {
      for (auto& slot : slots) {
        if (slot == kEmptySlot) {
          slot = fp;
          return true;
        }
      }
      return false;
    }

    // Check if fingerprint exists in bucket
    auto contains(Fingerprint fp) const -> bool {
      for (const auto& slot : slots) {
        if (slot == fp) {
          return true;
        }
      }
      return false;
    }

    // Remove fingerprint from bucket. Returns true if found and removed.
    auto erase(Fingerprint fp) -> bool {
      for (auto& slot : slots) {
        if (slot == fp) {
          slot = kEmptySlot;
          return true;
        }
      }
      return false;
    }
  };

  CuckooFilter() : buckets_(NumBuckets), size_{0} {}

  // Insert an item. Returns true if successful, false if filter is full.
  auto insert(std::uint64_t item) -> bool {
    auto fp = fingerprint(item);
    auto [i1, i2] = getBuckets(item, fp);

    // Try to insert in either bucket
    if (buckets_[i1].tryInsert(fp)) {
      ++size_;
      return true;
    }
    if (buckets_[i2].tryInsert(fp)) {
      ++size_;
      return true;
    }

    // Both buckets full - need to evict
    // Pick random bucket to start eviction chain
    std::uniform_int_distribution<int> coin{0, 1};
    std::size_t i = coin(rng_) ? i1 : i2;

    for (std::size_t kick = 0; kick < kMaxKicks; ++kick) {
      // Pick random slot to evict
      std::uniform_int_distribution<std::size_t> slot_dist{0, BucketSize - 1};
      std::size_t slot = slot_dist(rng_);

      // Swap our fingerprint with the evicted one
      std::swap(fp, buckets_[i].slots[slot]);

      // Compute alternate bucket for evicted fingerprint
      i = altBucket(i, fp);

      // Try to insert evicted fingerprint in its alternate bucket
      if (buckets_[i].tryInsert(fp)) {
        ++size_;
        return true;
      }
    }

    // Too many evictions - filter is too full
    return false;
  }

  // Check if an item might be in the filter.
  // Returns true if item is probably present, false if definitely absent.
  auto contains(std::uint64_t item) const -> bool {
    auto fp = fingerprint(item);
    auto [i1, i2] = getBuckets(item, fp);

    return buckets_[i1].contains(fp) || buckets_[i2].contains(fp);
  }

  // Remove an item from the filter.
  // Returns true if found and removed, false if not found.
  // WARNING: Deleting an item that was never inserted can cause false negatives!
  auto erase(std::uint64_t item) -> bool {
    auto fp = fingerprint(item);
    auto [i1, i2] = getBuckets(item, fp);

    if (buckets_[i1].erase(fp)) {
      --size_;
      return true;
    }
    if (buckets_[i2].erase(fp)) {
      --size_;
      return true;
    }
    return false;
  }

  // Number of items currently in the filter
  auto size() const -> std::size_t { return size_; }

  // Total capacity of the filter
  static constexpr auto capacity() -> std::size_t {
    return NumBuckets * BucketSize;
  }

  // Current load factor (occupancy ratio)
  auto loadFactor() const -> double {
    return static_cast<double>(size_) / static_cast<double>(capacity());
  }

  // Check if filter is empty
  auto empty() const -> bool { return size_ == 0; }

  // Clear all entries
  void clear() {
    for (auto& bucket : buckets_) {
      bucket.slots.fill(kEmptySlot);
    }
    size_ = 0;
  }

private:
  // Hash function for fingerprint computation
  // Uses MurmurHash3 finalizer for good distribution
  static auto hash64(std::uint64_t x) -> std::uint64_t {
    x ^= x >> 33;
    x *= 0xff51afd7ed558ccdULL;
    x ^= x >> 33;
    x *= 0xc4ceb9fe1a85ec53ULL;
    x ^= x >> 33;
    return x;
  }

  // Compute fingerprint from item
  // Uses upper bits of hash (lower bits used for bucket index)
  auto fingerprint(std::uint64_t item) const -> Fingerprint {
    auto h = hash64(item);
    // Use upper bits for fingerprint, mask to fit
    auto fp = static_cast<Fingerprint>((h >> 32) & kFingerprintMask);
    // Ensure fingerprint is never 0 (reserved for empty)
    return fp == kEmptySlot ? static_cast<Fingerprint>(1) : fp;
  }

  // Compute both candidate bucket indices for an item
  auto getBuckets(std::uint64_t item, Fingerprint fp) const
      -> std::pair<std::size_t, std::size_t> {
    auto h = hash64(item);
    auto i1 = static_cast<std::size_t>(h) & (NumBuckets - 1);
    auto i2 = altBucket(i1, fp);
    return {i1, i2};
  }

  // Compute alternate bucket using XOR trick
  // altBucket(altBucket(i, fp), fp) == i
  auto altBucket(std::size_t i, Fingerprint fp) const -> std::size_t {
    // Hash the fingerprint to get good distribution
    auto fp_hash = hash64(static_cast<std::uint64_t>(fp));
    return (i ^ fp_hash) & (NumBuckets - 1);
  }

  std::vector<Bucket> buckets_;
  std::size_t size_;
  mutable std::mt19937 rng_{std::random_device{}()};
};

}  // namespace tempura
