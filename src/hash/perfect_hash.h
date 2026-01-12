#pragma once

// ============================================================================
// Perfect Hashing: Zero-Collision Lookup for Static Key Sets
// ============================================================================
//
// A perfect hash function maps a known set of N keys to N distinct integers
// with NO collisions. This is the holy grail for lookup tables: O(1) lookup
// with zero probe chains, zero linked lists, zero wasted comparisons.
//
// WHEN TO USE PERFECT HASHING
// ===========================
//
// Perfect hashing is ideal when:
//   - The key set is known at compile time
//   - The key set never changes at runtime
//   - Lookup speed is critical
//
// Classic use cases:
//   - Keyword recognition (parsing languages, command interpreters)
//   - Enum-to-string and string-to-enum conversions
//   - Static lookup tables (HTTP headers, MIME types, country codes)
//   - Compiler symbol tables for reserved words
//
// Perfect hashing is NOT suitable when:
//   - Keys are added/removed at runtime (use std::unordered_map)
//   - The key set is very large (>10K keys - construction gets slow)
//   - You need to detect non-member keys (perfect hash gives garbage)
//
// THE TWO-LEVEL HASHING TRICK
// ===========================
//
// How do you find a collision-free hash? The clever insight is to use
// TWO hash functions in sequence:
//
//   1. First hash H1(key) → bucket index
//   2. Look up a "displacement" d for that bucket
//   3. Second hash H2(key, d) → final slot
//
// The magic: we choose different displacement values for each bucket
// until all keys land in unique final slots.
//
// Example with 4 keys {A, B, C, D}:
//
//   Step 1: Compute first hash for each key
//     H1(A) = 0, H1(B) = 1, H1(C) = 0, H1(D) = 1
//
//   Keys land in buckets: bucket[0] = {A, C}, bucket[1] = {B, D}
//
//   Step 2: For each bucket, find displacement that avoids collisions
//     Try d[0]=1 for bucket 0: H2(A,1)=2, H2(C,1)=3  ✓ (no collision)
//     Try d[1]=0 for bucket 1: H2(B,0)=0, H2(D,0)=1  ✓ (no collision)
//
//   Final mapping: A→2, B→0, C→3, D→1 (all unique!)
//
//   Lookup: hash(key) = H2(key, displacements[H1(key)])
//
// WHY THIS WORKS
// ==============
//
// The first hash partitions keys into buckets. If we're lucky, some buckets
// have only one key (trivial to place). For buckets with multiple keys, we
// try different displacements until we find one that places all keys in
// unoccupied slots.
//
// The key insight from CHD (Compress, Hash, Displace) algorithm:
//   - Process largest buckets FIRST
//   - Larger buckets have more constraints, so handle them while most
//     slots are still empty
//   - By the time we reach small buckets (1-2 keys), plenty of slots
//     remain and finding a valid displacement is easy
//
// MINIMAL vs NON-MINIMAL PERFECT HASHING
// ======================================
//
// A MINIMAL perfect hash function maps N keys to exactly {0, 1, ..., N-1}.
// The table size equals the key count - no wasted slots.
//
// A non-minimal perfect hash allows the table to be larger (e.g., 1.2N).
// This makes construction easier and faster, at the cost of some wasted
// space.
//
// This implementation is non-minimal for simplicity. For N keys, we use
// a table of size ~1.5N to ensure we can always find valid displacements.
//
// CONSTEXPR PERFECT HASHING
// =========================
//
// The beautiful thing about perfect hashing in C++: if the key set is
// known at compile time, we can compute the displacement table AT COMPILE
// TIME using constexpr. The result:
//
//   - Zero runtime initialization cost
//   - Lookup is just: compute H1, load displacement, compute H2, done
//   - The compiler embeds the perfect hash table directly in the binary
//
// This is what tools like gperf do (generate C code), but with constexpr
// we get the same result without code generation or external tools.
//
// LIMITATIONS AND GOTCHAS
// =======================
//
// 1. NO MEMBERSHIP TEST
//    If you look up a key that wasn't in the original set, you get
//    GARBAGE - some random slot, not "not found". You must either:
//      - Store and compare the original keys
//      - Only ever look up keys you know are valid
//
// 2. CONSTRUCTION CAN FAIL
//    For some key sets and hash functions, no valid displacement exists.
//    Our implementation retries with different seeds, but pathological
//    cases may fail. Use a larger table multiplier if this happens.
//
// 3. COMPILE TIME
//    Constexpr perfect hash construction is slow. For 100 keys, expect
//    a few seconds of compile time. For 1000+ keys, consider generating
//    the table offline.
//
// ============================================================================

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <utility>

#include "hash/fnv.h"
#include "hash/hash.h"

namespace tempura {

// ----------------------------------------------------------------------------
// PerfectHashBuilder: Constructs perfect hash at compile time
// ----------------------------------------------------------------------------
//
// This class encapsulates the CHD-like algorithm for building a perfect
// hash function. It's designed to be used in a constexpr context.
//
// Template parameters:
//   KeyType:      Type of keys (typically std::string_view for strings)
//   N:            Number of keys
//   TableMult:    Table size multiplier (1.5 means 50% extra slots)

template <typename KeyType, std::size_t N, std::size_t TableMult = 3,
          std::size_t TableDiv = 2>
class PerfectHashBuilder {
 public:
  // Table size: N * TableMult / TableDiv, minimum N
  static constexpr std::size_t kTableSize =
      std::max(N, N * TableMult / TableDiv);

  // Number of buckets for first-level hash (√N is a good choice)
  static constexpr std::size_t kNumBuckets = []() constexpr {
    std::size_t b = 1;
    while (b * b < N) ++b;
    return std::max(b, std::size_t{1});
  }();

  // The displacement table: one entry per bucket
  using DisplacementTable = std::array<std::uint32_t, kNumBuckets>;

  // Result of building: displacements + seed used
  struct BuildResult {
    DisplacementTable displacements;
    std::uint64_t seed;
    bool success;
  };

  // --------------------------------------------------------------------------
  // Build the perfect hash function
  // --------------------------------------------------------------------------
  //
  // This is the main algorithm. It tries different seeds until it finds
  // one that allows all keys to be placed without collision.

  static constexpr auto build(const std::array<KeyType, N>& keys) -> BuildResult {
    // Try different seeds (in case one fails)
    for (std::uint64_t seed = 0; seed < 256; ++seed) {
      auto result = tryBuild(keys, seed);
      if (result.success) {
        return result;
      }
    }

    // Failed to find a valid configuration
    return {{}, 0, false};
  }

 private:
  // --------------------------------------------------------------------------
  // Hash functions for the two levels
  // --------------------------------------------------------------------------
  //
  // H1: Maps key → bucket (for grouping keys)
  // H2: Maps (key, displacement) → final slot

  static constexpr auto hash1(KeyType key, std::uint64_t seed) -> std::size_t {
    // Use FNV for simplicity and constexpr compatibility
    std::uint64_t h = Fnv64Constants::kOffsetBasis ^ seed;
    if constexpr (std::is_same_v<KeyType, std::string_view>) {
      for (char c : key) {
        h ^= static_cast<std::uint8_t>(c);
        h *= Fnv64Constants::kPrime;
      }
    } else {
      // For integer types
      for (std::size_t i = 0; i < sizeof(KeyType); ++i) {
        h ^= static_cast<std::uint8_t>(key >> (i * 8));
        h *= Fnv64Constants::kPrime;
      }
    }
    return h % kNumBuckets;
  }

  static constexpr auto hash2(KeyType key, std::uint32_t displacement,
                              std::uint64_t seed) -> std::size_t {
    // Second hash incorporates the displacement as extra mixing
    std::uint64_t h = Fnv64Constants::kOffsetBasis ^ seed ^ displacement;
    if constexpr (std::is_same_v<KeyType, std::string_view>) {
      for (char c : key) {
        h ^= static_cast<std::uint8_t>(c);
        h *= Fnv64Constants::kPrime;
      }
    } else {
      for (std::size_t i = 0; i < sizeof(KeyType); ++i) {
        h ^= static_cast<std::uint8_t>(key >> (i * 8));
        h *= Fnv64Constants::kPrime;
      }
    }
    // Rotate to spread influence of displacement
    h = (h >> 17) | (h << 47);
    h *= Fnv64Constants::kPrime;
    return h % kTableSize;
  }

  // --------------------------------------------------------------------------
  // Bucket structure for first-level grouping
  // --------------------------------------------------------------------------

  struct Bucket {
    std::size_t indices[N];  // Indices of keys in this bucket
    std::size_t count = 0;
  };

  // --------------------------------------------------------------------------
  // Try to build with a specific seed
  // --------------------------------------------------------------------------

  static constexpr auto tryBuild(const std::array<KeyType, N>& keys,
                                 std::uint64_t seed) -> BuildResult {
    BuildResult result{{}, seed, false};

    // Step 1: Assign keys to buckets using first hash
    std::array<Bucket, kNumBuckets> buckets{};
    for (std::size_t i = 0; i < N; ++i) {
      std::size_t bucket_idx = hash1(keys[i], seed);
      buckets[bucket_idx].indices[buckets[bucket_idx].count++] = i;
    }

    // Step 2: Sort buckets by size (largest first)
    // This is the key insight from CHD: handle constrained buckets first
    std::array<std::size_t, kNumBuckets> bucket_order{};
    for (std::size_t i = 0; i < kNumBuckets; ++i) {
      bucket_order[i] = i;
    }

    // Simple bubble sort (constexpr-friendly)
    for (std::size_t i = 0; i < kNumBuckets; ++i) {
      for (std::size_t j = i + 1; j < kNumBuckets; ++j) {
        if (buckets[bucket_order[j]].count > buckets[bucket_order[i]].count) {
          std::size_t tmp = bucket_order[i];
          bucket_order[i] = bucket_order[j];
          bucket_order[j] = tmp;
        }
      }
    }

    // Step 3: Track which final slots are occupied
    std::array<bool, kTableSize> occupied{};

    // Step 4: For each bucket (largest first), find a valid displacement
    for (std::size_t bi = 0; bi < kNumBuckets; ++bi) {
      std::size_t bucket_idx = bucket_order[bi];
      const Bucket& bucket = buckets[bucket_idx];

      if (bucket.count == 0) {
        result.displacements[bucket_idx] = 0;
        continue;
      }

      // Try different displacements until we find one that works
      bool found = false;
      for (std::uint32_t d = 0; d < kTableSize * 4 && !found; ++d) {
        // Check if this displacement causes any collision
        bool collision = false;
        std::array<std::size_t, N> slots{};

        for (std::size_t ki = 0; ki < bucket.count && !collision; ++ki) {
          std::size_t key_idx = bucket.indices[ki];
          std::size_t slot = hash2(keys[key_idx], d, seed);

          // Check for collision with already-occupied slots
          if (occupied[slot]) {
            collision = true;
          }

          // Check for collision within this bucket
          for (std::size_t prev = 0; prev < ki; ++prev) {
            if (slots[prev] == slot) {
              collision = true;
              break;
            }
          }

          slots[ki] = slot;
        }

        if (!collision) {
          // Found a valid displacement - mark slots as occupied
          result.displacements[bucket_idx] = d;
          for (std::size_t ki = 0; ki < bucket.count; ++ki) {
            occupied[slots[ki]] = true;
          }
          found = true;
        }
      }

      if (!found) {
        // Could not find valid displacement for this bucket
        return result;  // result.success is still false
      }
    }

    result.success = true;
    return result;
  }

 public:
  // Make hash functions accessible for lookup
  static constexpr auto computeHash1(KeyType key, std::uint64_t seed)
      -> std::size_t {
    return hash1(key, seed);
  }

  static constexpr auto computeHash2(KeyType key, std::uint32_t displacement,
                                     std::uint64_t seed) -> std::size_t {
    return hash2(key, displacement, seed);
  }
};

// ----------------------------------------------------------------------------
// PerfectHashMap: Compile-time constructed perfect hash map
// ----------------------------------------------------------------------------
//
// This is the user-facing class. It wraps the builder and provides a
// simple lookup interface.
//
// Usage:
//   constexpr auto keywords = std::array{
//     std::pair{"if"sv, Token::If},
//     std::pair{"else"sv, Token::Else},
//     ...
//   };
//   constexpr auto map = makePerfectHashMap(keywords);
//   auto token = map.find("if");  // Returns Token::If

template <typename KeyType, typename ValueType, std::size_t N>
class PerfectHashMap {
 public:
  using Builder = PerfectHashBuilder<KeyType, N>;
  static constexpr std::size_t kTableSize = Builder::kTableSize;

  // Storage for values and keys (keys needed for verification)
  struct Slot {
    KeyType key{};
    ValueType value{};
    bool occupied = false;
  };

 private:
  std::array<Slot, kTableSize> slots_{};
  typename Builder::DisplacementTable displacements_{};
  std::uint64_t seed_ = 0;
  bool valid_ = false;

 public:
  // --------------------------------------------------------------------------
  // Construct from array of key-value pairs
  // --------------------------------------------------------------------------

  constexpr PerfectHashMap() = default;

  constexpr explicit PerfectHashMap(
      const std::array<std::pair<KeyType, ValueType>, N>& entries) {
    // Extract just the keys for building
    std::array<KeyType, N> keys{};
    for (std::size_t i = 0; i < N; ++i) {
      keys[i] = entries[i].first;
    }

    // Build the perfect hash
    auto build_result = Builder::build(keys);
    if (!build_result.success) {
      valid_ = false;
      return;
    }

    displacements_ = build_result.displacements;
    seed_ = build_result.seed;
    valid_ = true;

    // Place entries in their slots
    for (std::size_t i = 0; i < N; ++i) {
      std::size_t slot = computeSlot(entries[i].first);
      slots_[slot].key = entries[i].first;
      slots_[slot].value = entries[i].second;
      slots_[slot].occupied = true;
    }
  }

  // --------------------------------------------------------------------------
  // Lookup
  // --------------------------------------------------------------------------

  constexpr auto find(KeyType key) const -> const ValueType* {
    if (!valid_) {
      return nullptr;
    }

    std::size_t slot = computeSlot(key);
    if (slots_[slot].occupied && slots_[slot].key == key) {
      return &slots_[slot].value;
    }
    return nullptr;
  }

  constexpr auto operator[](KeyType key) const -> const ValueType& {
    const ValueType* v = find(key);
    // In constexpr context, this will fail at compile time if key not found
    return *v;
  }

  constexpr auto contains(KeyType key) const -> bool {
    return find(key) != nullptr;
  }

  constexpr auto isValid() const -> bool { return valid_; }

  constexpr auto size() const -> std::size_t { return N; }

  constexpr auto tableSize() const -> std::size_t { return kTableSize; }

  // Load factor: actual entries / table size
  constexpr auto loadFactor() const -> double {
    return static_cast<double>(N) / kTableSize;
  }

 private:
  constexpr auto computeSlot(KeyType key) const -> std::size_t {
    std::size_t bucket = Builder::computeHash1(key, seed_);
    return Builder::computeHash2(key, displacements_[bucket], seed_);
  }
};

// ----------------------------------------------------------------------------
// Factory function for type deduction
// ----------------------------------------------------------------------------

template <typename KeyType, typename ValueType, std::size_t N>
constexpr auto makePerfectHashMap(
    const std::array<std::pair<KeyType, ValueType>, N>& entries)
    -> PerfectHashMap<KeyType, ValueType, N> {
  return PerfectHashMap<KeyType, ValueType, N>(entries);
}

// Overload for brace-initialized arrays
template <typename KeyType, typename ValueType, std::size_t N>
constexpr auto makePerfectHashMap(
    const std::pair<KeyType, ValueType> (&entries)[N])
    -> PerfectHashMap<KeyType, ValueType, N> {
  std::array<std::pair<KeyType, ValueType>, N> arr{};
  for (std::size_t i = 0; i < N; ++i) {
    arr[i] = entries[i];
  }
  return PerfectHashMap<KeyType, ValueType, N>(arr);
}

// ----------------------------------------------------------------------------
// PerfectHashSet: Just keys, no values
// ----------------------------------------------------------------------------
//
// For membership testing only. Still stores keys for verification.

template <typename KeyType, std::size_t N>
class PerfectHashSet {
 public:
  using Builder = PerfectHashBuilder<KeyType, N>;
  static constexpr std::size_t kTableSize = Builder::kTableSize;

 private:
  struct Slot {
    KeyType key{};
    bool occupied = false;
  };

  std::array<Slot, kTableSize> slots_{};
  typename Builder::DisplacementTable displacements_{};
  std::uint64_t seed_ = 0;
  bool valid_ = false;

 public:
  constexpr PerfectHashSet() = default;

  constexpr explicit PerfectHashSet(const std::array<KeyType, N>& keys) {
    auto build_result = Builder::build(keys);
    if (!build_result.success) {
      valid_ = false;
      return;
    }

    displacements_ = build_result.displacements;
    seed_ = build_result.seed;
    valid_ = true;

    for (std::size_t i = 0; i < N; ++i) {
      std::size_t slot = computeSlot(keys[i]);
      slots_[slot].key = keys[i];
      slots_[slot].occupied = true;
    }
  }

  constexpr auto contains(KeyType key) const -> bool {
    if (!valid_) {
      return false;
    }
    std::size_t slot = computeSlot(key);
    return slots_[slot].occupied && slots_[slot].key == key;
  }

  constexpr auto isValid() const -> bool { return valid_; }

  constexpr auto size() const -> std::size_t { return N; }

 private:
  constexpr auto computeSlot(KeyType key) const -> std::size_t {
    std::size_t bucket = Builder::computeHash1(key, seed_);
    return Builder::computeHash2(key, displacements_[bucket], seed_);
  }
};

template <typename KeyType, std::size_t N>
constexpr auto makePerfectHashSet(const std::array<KeyType, N>& keys)
    -> PerfectHashSet<KeyType, N> {
  return PerfectHashSet<KeyType, N>(keys);
}

template <typename KeyType, std::size_t N>
constexpr auto makePerfectHashSet(const KeyType (&keys)[N])
    -> PerfectHashSet<KeyType, N> {
  std::array<KeyType, N> arr{};
  for (std::size_t i = 0; i < N; ++i) {
    arr[i] = keys[i];
  }
  return PerfectHashSet<KeyType, N>(arr);
}

}  // namespace tempura
