#pragma once

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <tuple>

namespace tempura {

// ============================================================================
// Weighted MinHash for Generalized Jaccard Similarity
// ============================================================================
//
// Standard MinHash estimates the Jaccard similarity of binary sets (element
// present or absent). Weighted MinHash extends this to weighted sets, where
// each element has an associated non-negative weight.
//
// WEIGHTED JACCARD SIMILARITY
// ---------------------------
// For weighted sets A and B where w_a(x) and w_b(x) are the weights of x:
//
//   J_weighted(A, B) = sum(min(w_a(x), w_b(x))) / sum(max(w_a(x), w_b(x)))
//
// This generalizes binary Jaccard:
//   - Binary sets: weights are 0 or 1, reduces to |A ∩ B| / |A ∪ B|
//   - Weighted sets: measures overlap accounting for magnitudes
//
// Example: Document word counts
//   A = {apple: 3, banana: 2}
//   B = {apple: 1, banana: 4}
//   Numerator = min(3,1) + min(2,4) = 1 + 2 = 3
//   Denominator = max(3,1) + max(2,4) = 3 + 4 = 7
//   J_weighted = 3/7 ≈ 0.43
//
// THE ICWS ALGORITHM
// ------------------
// Improved Consistent Weighted Sampling (ICWS) by Ioffe (2010) enables MinHash
// for weighted sets. The key insight: transform weights into a space where
// standard min-hashing works.
//
// For each (feature, weight) pair and each hash function k:
//
//   1. Generate r_k, c_k ~ Gamma(2,1), beta_k ~ Uniform(0,1) from hash(feature, k)
//   2. t_k = floor(log(weight) / r_k + beta_k)
//   3. y_k = exp(r_k * (t_k - beta_k))
//   4. a_k = c_k / y_k
//
// The signature for hash k is the (feature_hash, t_k) pair with minimum a_k.
//
// WHY THIS WORKS
// --------------
// The transformation creates a Poisson point process where:
//   - Higher weights produce more "hash opportunities"
//   - P(feature x has min a for hash k) ∝ w(x) / sum(weights)
//   - Two sets share the same (hash, t) when they agree on the dominant feature
//
// The probability of signature match equals the weighted Jaccard similarity.
//
// SIGNATURE STRUCTURE
// -------------------
// Unlike standard MinHash (which stores just the minimum hash), weighted MinHash
// signatures store (feature_hash, t_value) pairs. Two signatures match when
// BOTH components are equal.
//
// ACCURACY
// --------
// Same as standard MinHash: with k hash functions, standard error is:
//   SE = sqrt(J(1-J)/k)
//
// WHEN TO USE WEIGHTED MINHASH
// ----------------------------
// Use when elements have meaningful weights:
//   - TF-IDF document vectors
//   - Histogram comparison (image features, audio spectra)
//   - User preference vectors (item ratings)
//   - Any bag-of-words with counts
//
// Use standard MinHash when:
//   - Elements are purely present/absent (binary)
//   - All non-zero weights are equal
//
// TEMPLATE PARAMETERS
// -------------------
//   NumHashes - Number of hash functions (default 64). More = better accuracy.
//
// COMPLEXITY
// ----------
//   computeSignature: O(|S| × k) where |S| is weighted set size
//   estimateSimilarity: O(k)
//   Space per signature: k × 16 bytes (k pairs of uint64_t + int64_t)
//
// REFERENCES
// ----------
//   Ioffe, S. (2010). "Improved Consistent Sampling, Weighted Minhash and L1
//   Sketching". IEEE ICDM.
//
template <std::size_t NumHashes = 64>
class WeightedMinHash {
  static_assert(NumHashes > 0, "NumHashes must be positive");

 public:
  // A weighted feature: (identifier, weight)
  struct WeightedFeature {
    std::uint64_t feature_id;
    double weight;
  };

  // A single element of the signature: (feature_hash, t_value)
  // Two signatures match at position k if both hash AND t match
  struct SignatureElement {
    std::uint64_t hash;
    std::int64_t t_value;

    auto operator==(const SignatureElement& other) const -> bool {
      return hash == other.hash && t_value == other.t_value;
    }
  };

  using Signature = std::array<SignatureElement, NumHashes>;

  // Compute the weighted MinHash signature for a span of weighted features.
  // Features with non-positive weights are ignored.
  auto computeSignature(std::span<const WeightedFeature> features) const
      -> Signature {
    Signature sig;

    // Initialize with invalid values (infinite a)
    for (std::size_t k = 0; k < NumHashes; ++k) {
      sig[k] = {0, 0};
    }

    // Track minimum a values for each hash function
    std::array<double, NumHashes> min_a;
    min_a.fill(std::numeric_limits<double>::infinity());

    for (const auto& [feature_id, weight] : features) {
      if (weight <= 0.0) {
        continue;  // Skip non-positive weights
      }

      double log_weight = std::log(weight);

      for (std::size_t k = 0; k < NumHashes; ++k) {
        // Generate pseudo-random r, c, beta from (feature_id, k)
        auto [r, c, beta] = generateParams(feature_id, k);

        // ICWS formula
        double t_real = std::floor(log_weight / r + beta);
        auto t = static_cast<std::int64_t>(t_real);
        double y = std::exp(r * (t_real - beta));
        double a = c / y;

        // Track minimum a
        if (a < min_a[k]) {
          min_a[k] = a;
          sig[k].hash = hashFeature(feature_id);
          sig[k].t_value = t;
        }
      }
    }

    return sig;
  }

  // Estimate weighted Jaccard similarity from two signatures.
  // Returns the fraction of positions where (hash, t) pairs match.
  static auto estimateSimilarity(const Signature& sig_a, const Signature& sig_b)
      -> double {
    std::size_t matches = 0;
    for (std::size_t k = 0; k < NumHashes; ++k) {
      if (sig_a[k] == sig_b[k]) {
        ++matches;
      }
    }
    return static_cast<double>(matches) / static_cast<double>(NumHashes);
  }

  // Get the number of hash functions used.
  static constexpr auto numHashes() -> std::size_t { return NumHashes; }

 private:
  // Generate (r, c, beta) parameters from (feature_id, k)
  // r, c ~ Gamma(2, 1), beta ~ Uniform(0, 1)
  // Must be consistent for the same (feature, hash_index) pair
  static auto generateParams(std::uint64_t feature_id, std::size_t k)
      -> std::tuple<double, double, double> {
    // Generate 4 independent uniform values in (0, 1)
    // Two pairs for Gamma(2,1) via sum of exponentials, one for beta
    std::uint64_t seed1 = hashCombine(feature_id, k * 4 + 0);
    std::uint64_t seed2 = hashCombine(feature_id, k * 4 + 1);
    std::uint64_t seed3 = hashCombine(feature_id, k * 4 + 2);
    std::uint64_t seed4 = hashCombine(feature_id, k * 4 + 3);
    std::uint64_t seed5 = hashCombine(feature_id, k * 4 + 4);

    // Convert to (0, 1) range, avoiding exact 0 to prevent -inf in log
    double u1 = toUnit(seed1);
    double u2 = toUnit(seed2);
    double u3 = toUnit(seed3);
    double u4 = toUnit(seed4);
    double u5 = toUnit(seed5);

    // Gamma(2, 1) = sum of 2 independent Exp(1)
    // Exp(1) = -ln(U) where U ~ Uniform(0,1)
    double r = -std::log(u1) - std::log(u2);
    double c = -std::log(u3) - std::log(u4);
    double beta = u5;

    return {r, c, beta};
  }

  // Convert uint64 to uniform (0, 1), avoiding 0
  static auto toUnit(std::uint64_t x) -> double {
    // Use high bits, ensure non-zero
    constexpr double kScale =
        1.0 / (static_cast<double>(std::numeric_limits<std::uint64_t>::max()) + 1.0);
    return (static_cast<double>(x) + 0.5) * kScale;
  }

  // Hash a feature_id for the signature
  static auto hashFeature(std::uint64_t feature_id) -> std::uint64_t {
    return splitmix64(feature_id);
  }

  // Combine two values into a hash (for generating per-hash-function params)
  static auto hashCombine(std::uint64_t a, std::size_t b) -> std::uint64_t {
    std::uint64_t x = a ^ (static_cast<std::uint64_t>(b) * 0x9e3779b97f4a7c15ULL);
    return splitmix64(x);
  }

  // SplitMix64 mixing function for high-quality pseudo-random values
  static auto splitmix64(std::uint64_t x) -> std::uint64_t {
    x += 0x9e3779b97f4a7c15ULL;
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
    return x ^ (x >> 31);
  }
};

}  // namespace tempura
