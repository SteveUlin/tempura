#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <ranges>

namespace tempura {

// ============================================================================
// One Permutation Hashing (OPH) for Jaccard Similarity Estimation
// ============================================================================
//
// One Permutation Hashing is a faster alternative to standard MinHash for
// estimating Jaccard similarity between sets.
//
// THE MINHASH BOTTLENECK
// ----------------------
// Standard MinHash uses k independent hash functions to create a k-dimensional
// signature. For each set element, we must compute k hash values - O(k) work
// per element. For large k (needed for accuracy), this becomes expensive.
//
// THE OPH INSIGHT
// ---------------
// Instead of k hash functions, use ONE hash function and divide the hash space
// into k bins. Each element hashes to exactly one bin, and we track the minimum
// hash value seen in each bin.
//
// For element x with hash h = hash(x):
//   bin_index = h mod k
//   signature[bin_index] = min(signature[bin_index], h)
//
// This reduces per-element work from O(k) to O(1) - a significant speedup.
//
// SIMILARITY ESTIMATION
// ---------------------
// For two signatures sig_A and sig_B:
//   1. Count bins where BOTH signatures are non-empty (valid bins)
//   2. Among valid bins, count where sig_A[i] == sig_B[i] (matches)
//   3. Similarity = matches / valid_bins
//
// Empty bins (no elements hashed there) are excluded from the calculation
// since they provide no information about set overlap.
//
// THE EMPTY BIN PROBLEM
// ---------------------
// With k bins and n elements, expected elements per bin = n/k. When n < k,
// many bins will be empty, reducing the effective number of hash comparisons.
//
// For very sparse sets (n << k), most bins are empty and similarity estimates
// become unreliable. The variance increases because fewer bins contribute.
//
// DENSIFICATION (HANDLING EMPTY BINS)
// -----------------------------------
// Several strategies exist to fill empty bins:
//
// 1. Rotation (implemented here): For each empty bin i, search cyclically
//    through bins (i+1) mod k, (i+2) mod k, ... until finding a non-empty
//    bin, and copy its value. Simple and effective.
//
// 2. Optimal Densification: Fill empty bin with value from the nearest
//    non-empty neighbor. Slightly better theoretical properties.
//
// 3. Zero-fill: Treat empty bins as having value 0. Introduces bias.
//
// Densification ensures all k bins participate in similarity estimation,
// maintaining accuracy even for sparse sets.
//
// ACCURACY ANALYSIS
// -----------------
// OPH achieves the same asymptotic accuracy as standard MinHash when:
//   - Both sets have sufficient elements (n >= k)
//   - Densification is applied for sparse sets
//
// Standard error for similarity J with k bins:
//   SE ≈ sqrt(J(1-J)/k)
//
// Same as standard MinHash with k hash functions.
//
// WHY USE OPH OVER STANDARD MINHASH?
// ----------------------------------
// ✓ O(n) signature computation vs O(nk) for standard MinHash
// ✓ Same accuracy when sets are dense enough
// ✓ Signature merging still works: merge = elementwise_min
// ✓ Compatible with LSH banding for approximate nearest neighbor
//
// ✗ Empty bins reduce effective k for sparse sets (unless densified)
// ✗ Densification adds complexity and small overhead
//
// WHEN TO CHOOSE OPH
// ------------------
// - Large k values (256, 512, 1024) where per-element hashing cost matters
// - Streaming applications where signature must be updated incrementally
// - When sets are typically dense (n > k)
// - When you need the speed and can tolerate slightly more variance on sparse sets
//
// TEMPLATE PARAMETERS
// -------------------
//   NumBins - Number of bins to divide hash space into (k). Default 128.
//             More bins = better accuracy but more memory and empty bins for small sets.
//
// COMPLEXITY
// ----------
//   computeSignature:    O(n) where n is set size (vs O(nk) for standard MinHash)
//   estimateSimilarity:  O(k)
//   densify:             O(k)
//   merge:               O(k)
//   Space per signature: k x 8 bytes
//
template <std::size_t NumBins = 128>
class OnePermutationHash {
  static_assert(NumBins > 0, "NumBins must be positive");
  // NumBins should be a power of 2 for efficient modulo via bitmask
  static_assert((NumBins & (NumBins - 1)) == 0,
                "NumBins must be a power of 2 for efficient bin assignment");

 public:
  static constexpr std::uint64_t kEmpty = std::numeric_limits<std::uint64_t>::max();

  using Signature = std::array<std::uint64_t, NumBins>;

  // Compute the OPH signature for a range of elements.
  // Each element is hashed once, and placed in its corresponding bin.
  // Empty bins are marked with kEmpty (UINT64_MAX).
  template <std::ranges::input_range Range>
  auto computeSignature(Range&& range) const -> Signature {
    Signature sig;
    sig.fill(kEmpty);

    for (const auto& element : range) {
      std::uint64_t h = computeHash(element);
      std::size_t bin = h & (NumBins - 1);  // h mod NumBins (NumBins is power of 2)
      sig[bin] = std::min(sig[bin], h);
    }

    return sig;
  }

  // Estimate Jaccard similarity from two OPH signatures.
  // Only counts bins where BOTH signatures are non-empty.
  // Returns the fraction of valid bins where signatures agree.
  static auto estimateSimilarity(const Signature& sig_a, const Signature& sig_b)
      -> double {
    std::size_t valid_bins = 0;
    std::size_t matches = 0;

    for (std::size_t i = 0; i < NumBins; ++i) {
      bool a_empty = (sig_a[i] == kEmpty);
      bool b_empty = (sig_b[i] == kEmpty);

      if (!a_empty && !b_empty) {
        ++valid_bins;
        if (sig_a[i] == sig_b[i]) {
          ++matches;
        }
      }
    }

    if (valid_bins == 0) {
      // Both signatures are entirely empty (both sets are empty)
      // Convention: identical empty sets have similarity 1.0
      bool a_all_empty = std::all_of(sig_a.begin(), sig_a.end(),
                                     [](auto v) { return v == kEmpty; });
      bool b_all_empty = std::all_of(sig_b.begin(), sig_b.end(),
                                     [](auto v) { return v == kEmpty; });
      return (a_all_empty && b_all_empty) ? 1.0 : 0.0;
    }

    return static_cast<double>(matches) / static_cast<double>(valid_bins);
  }

  // Densify a signature using rotation strategy.
  // For each empty bin i, search cyclically for the next non-empty bin
  // and copy its value. This ensures all bins are non-empty.
  // Returns false if signature cannot be densified (all bins empty).
  static auto densify(Signature& sig) -> bool {
    // First check if there's at least one non-empty bin
    std::size_t first_non_empty = NumBins;
    for (std::size_t i = 0; i < NumBins; ++i) {
      if (sig[i] != kEmpty) {
        first_non_empty = i;
        break;
      }
    }

    if (first_non_empty == NumBins) {
      // All bins empty - cannot densify
      return false;
    }

    // Fill empty bins by rotation: search forward for non-empty bin
    for (std::size_t i = 0; i < NumBins; ++i) {
      if (sig[i] == kEmpty) {
        // Search forward (cyclically) for non-empty bin
        std::size_t j = (i + 1) % NumBins;
        while (sig[j] == kEmpty) {
          j = (j + 1) % NumBins;
        }
        sig[i] = sig[j];
      }
    }

    return true;
  }

  // Merge two signatures to produce the signature of their union.
  // signature(A U B) = elementwise_min(signature(A), signature(B))
  static auto merge(const Signature& sig_a, const Signature& sig_b) -> Signature {
    Signature result;
    for (std::size_t i = 0; i < NumBins; ++i) {
      result[i] = std::min(sig_a[i], sig_b[i]);
    }
    return result;
  }

  // Count the number of non-empty bins in a signature.
  static auto countNonEmpty(const Signature& sig) -> std::size_t {
    std::size_t count = 0;
    for (std::size_t i = 0; i < NumBins; ++i) {
      if (sig[i] != kEmpty) {
        ++count;
      }
    }
    return count;
  }

  // Get the number of bins.
  static constexpr auto numBins() -> std::size_t { return NumBins; }

 private:
  // Compute hash for an element using std::hash + SplitMix64 mixing.
  template <typename T>
  static auto computeHash(const T& element) -> std::uint64_t {
    std::uint64_t h = std::hash<T>{}(element);
    // SplitMix64 mixing for better distribution
    h += 0x9e3779b97f4a7c15ULL;
    h = (h ^ (h >> 30)) * 0xbf58476d1ce4e5b9ULL;
    h = (h ^ (h >> 27)) * 0x94d049bb133111ebULL;
    return h ^ (h >> 31);
  }
};

}  // namespace tempura
