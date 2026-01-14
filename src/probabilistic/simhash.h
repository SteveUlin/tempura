#pragma once

#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <numbers>
#include <span>
#include <utility>

namespace tempura {

// ============================================================================
// SimHash - Locality-Sensitive Hashing for Cosine Similarity
// ============================================================================
//
// SimHash is a locality-sensitive hashing (LSH) technique that produces
// compact binary signatures where the Hamming distance approximates the
// angular distance between high-dimensional vectors. This allows efficient
// near-duplicate detection and similarity search.
//
// THE INTUITION: RANDOM HYPERPLANES
// ---------------------------------
// Imagine your feature vectors as points in high-dimensional space. SimHash
// picks NumBits random hyperplanes through the origin. For each hyperplane,
// we record which side of the plane the vector falls on (1 or 0).
//
// Two similar vectors (small angle between them) will likely fall on the same
// side of most hyperplanes. Two dissimilar vectors (large angle) will differ
// on roughly half the hyperplanes.
//
//                    Hyperplane 1
//                         |
//               A •       |
//                 \       |
//                  \      |        Hyperplane 2
//                   \     |       /
//                    \    |      /
//                     \   |     /
//                      \  |    /
//                       \ |   /
//              ----------\|--/---------- Hyperplane 3
//                         |`
//                         | `
//                         |  * B
//
// A and B are on the same side of hyperplanes 1 and 3, different sides of 2.
// Signature for A: 110, Signature for B: 100, Hamming distance = 1
//
// THE ALGORITHM
// -------------
// Instead of explicitly generating random hyperplanes (expensive!), we use
// the hash of each feature to implicitly define random directions:
//
// 1. Initialize an array of NumBits accumulators (sums) to 0
// 2. For each (feature, weight) pair:
//    a. Compute hash(feature) to get a NumBits-bit value
//    b. For each bit position i:
//       - If bit i of hash is 1: sum[i] += weight
//       - If bit i of hash is 0: sum[i] -= weight
// 3. Final signature: bit i = 1 if sum[i] > 0, else 0
//
// The hash function acts as a random projection: if hash(feature) bit i is 1,
// the feature contributes positively to dimension i; otherwise negatively.
//
// MATHEMATICAL CONNECTION
// -----------------------
// The key insight: for two vectors A and B with angle θ between them,
//
//   P(hash bit differs) = θ / π
//
// Therefore:
//   Expected Hamming distance = NumBits × (θ / π)
//   θ ≈ π × (Hamming distance / NumBits)
//   cos(θ) ≈ cos(π × Hamming distance / NumBits)
//
// This gives us an unbiased estimator of cosine similarity!
//
// EXAMPLE
// -------
// Document A: {"the": 3, "cat": 2, "sat": 1}
// Document B: {"the": 3, "dog": 2, "sat": 1}
//
// Both share "the" and "sat" with same weights - their signatures will be
// similar. The differing features ("cat" vs "dog") will cause some bit flips.
//
// WHY USE SIMHASH?
// ----------------
// vs Direct Cosine Similarity:
//   ✓ O(1) comparison time after O(n) signature computation
//   ✓ Fixed-size signature regardless of original dimensionality
//   ✓ Hamming distance enables hardware acceleration (popcount)
//   ✓ Can build indexes for sublinear similarity search
//   ✗ Approximate, not exact (but often good enough for dedup/search)
//
// vs MinHash (Jaccard similarity):
//   ✓ Handles weighted features naturally
//   ✓ Measures angle (cosine) rather than overlap (Jaccard)
//   ✗ MinHash may be better for pure set similarity
//
// WHEN TO USE SIMHASH
// -------------------
// • Near-duplicate detection (web pages, documents, images)
// • Clustering similar items efficiently
// • Building similarity search indexes
// • Any case where you need fast approximate cosine similarity
//
// TEMPLATE PARAMETERS
// -------------------
//   NumBits - Size of the signature in bits (default 64). More bits = more
//             accurate but larger signatures. 64 bits is common for dedup.
//
// COMPLEXITY
// ----------
//   computeSignature:   O(n × k) where n = features, k = NumBits
//   hammingDistance:    O(1) for 64-bit, O(NumBits/64) for larger
//   estimateSimilarity: O(1) (just arccos lookup after Hamming)
//   Space:              NumBits / 8 bytes per signature
//
template <std::size_t NumBits = 64>
class SimHash {
  static_assert(NumBits > 0, "NumBits must be positive");
  static_assert(NumBits % 64 == 0, "NumBits must be a multiple of 64");

public:
  static constexpr std::size_t kNumWords = NumBits / 64;

  // Signature type: single uint64_t for 64-bit, array for larger
  using Signature = std::conditional_t<kNumWords == 1,
                                       std::uint64_t,
                                       std::array<std::uint64_t, kNumWords>>;

  // A weighted feature: (hash value, weight)
  // The hash value is pre-computed by the caller using std::hash or similar
  struct WeightedFeature {
    std::size_t hash;
    double weight;
  };

  // Compute the SimHash signature for a set of weighted features.
  // Features are (hash, weight) pairs where hash is the pre-computed hash
  // of the original feature (e.g., a word or n-gram).
  static auto computeSignature(std::span<const WeightedFeature> features)
      -> Signature {
    // Accumulator for each bit position
    std::array<double, NumBits> sums{};

    for (const auto& [feature_hash, weight] : features) {
      // Use the feature hash and additional mixing to get NumBits of randomness
      for (std::size_t word = 0; word < kNumWords; ++word) {
        // Mix the hash with the word index to get independent bits for each word
        std::uint64_t bits = mixHash(feature_hash, word);

        for (std::size_t bit = 0; bit < 64; ++bit) {
          std::size_t idx = word * 64 + bit;
          if ((bits >> bit) & 1) {
            sums[idx] += weight;
          } else {
            sums[idx] -= weight;
          }
        }
      }
    }

    // Convert sums to signature: bit i = 1 if sum[i] > 0
    return buildSignature(sums);
  }

  // Convenience overload for unweighted features (all weights = 1)
  static auto computeSignature(std::span<const std::size_t> hashes)
      -> Signature {
    std::array<double, NumBits> sums{};

    for (std::size_t feature_hash : hashes) {
      for (std::size_t word = 0; word < kNumWords; ++word) {
        std::uint64_t bits = mixHash(feature_hash, word);

        for (std::size_t bit = 0; bit < 64; ++bit) {
          std::size_t idx = word * 64 + bit;
          if ((bits >> bit) & 1) {
            sums[idx] += 1.0;
          } else {
            sums[idx] -= 1.0;
          }
        }
      }
    }

    return buildSignature(sums);
  }

  // Compute Hamming distance between two signatures.
  // This is the number of bit positions that differ.
  static auto hammingDistance(const Signature& a, const Signature& b)
      -> std::size_t {
    if constexpr (kNumWords == 1) {
      return static_cast<std::size_t>(std::popcount(a ^ b));
    } else {
      std::size_t distance = 0;
      for (std::size_t i = 0; i < kNumWords; ++i) {
        distance += static_cast<std::size_t>(std::popcount(a[i] ^ b[i]));
      }
      return distance;
    }
  }

  // Estimate cosine similarity from Hamming distance.
  // Hamming/NumBits ≈ θ/π, so cos(θ) ≈ cos(π × Hamming/NumBits)
  static auto estimateSimilarity(const Signature& a, const Signature& b)
      -> double {
    std::size_t dist = hammingDistance(a, b);
    double angle = std::numbers::pi * static_cast<double>(dist) /
                   static_cast<double>(NumBits);
    return std::cos(angle);
  }

  // Direct similarity estimate from Hamming distance (for when distance is
  // already computed)
  static auto estimateSimilarity(std::size_t hamming_distance) -> double {
    double angle = std::numbers::pi * static_cast<double>(hamming_distance) /
                   static_cast<double>(NumBits);
    return std::cos(angle);
  }

  // Get the number of bits in the signature
  static constexpr auto numBits() -> std::size_t { return NumBits; }

private:
  // Mix hash value with word index to get independent bits for each 64-bit word
  // Uses SplitMix64 for high-quality mixing
  static constexpr auto mixHash(std::size_t hash, std::size_t word_idx)
      -> std::uint64_t {
    std::size_t x = hash ^ (word_idx * 0x9e3779b97f4a7c15ULL);
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
    return x ^ (x >> 31);
  }

  // Convert accumulator sums to final signature
  static auto buildSignature(const std::array<double, NumBits>& sums)
      -> Signature {
    if constexpr (kNumWords == 1) {
      std::uint64_t sig = 0;
      for (std::size_t i = 0; i < 64; ++i) {
        if (sums[i] > 0) {
          sig |= (1ULL << i);
        }
      }
      return sig;
    } else {
      Signature sig{};
      for (std::size_t word = 0; word < kNumWords; ++word) {
        std::uint64_t word_sig = 0;
        for (std::size_t bit = 0; bit < 64; ++bit) {
          if (sums[word * 64 + bit] > 0) {
            word_sig |= (1ULL << bit);
          }
        }
        sig[word] = word_sig;
      }
      return sig;
    }
  }
};

}  // namespace tempura
