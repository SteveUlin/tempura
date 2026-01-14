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
// MinHash for Jaccard Similarity Estimation
// ============================================================================
//
// MinHash is a locality-sensitive hashing technique for efficiently estimating
// the Jaccard similarity between sets without comparing them element by element.
//
// JACCARD SIMILARITY
// ------------------
// The Jaccard similarity of two sets A and B is:
//   J(A, B) = |A ∩ B| / |A ∪ B|
//
// This measures the overlap between sets, ranging from 0 (disjoint) to 1 (identical).
// Computing it directly requires O(|A| + |B|) time per comparison.
//
// THE KEY INSIGHT
// ---------------
// For any random permutation π of the universe:
//   P(min{π(A)} = min{π(B)}) = J(A, B)
//
// In other words: if you randomly shuffle all possible elements, the probability
// that A and B share the same minimum element equals their Jaccard similarity.
//
// Why? Consider the element x with the smallest π(x) in A ∪ B. This element
// is in A ∩ B with probability |A ∩ B| / |A ∪ B| = J(A, B). And if x ∈ A ∩ B,
// then min{π(A)} = min{π(B)} = π(x).
//
// MINHASH SIGNATURE
// -----------------
// Instead of using actual permutations (expensive), we use k independent hash
// functions h₁, h₂, ..., hₖ to simulate random permutations. For each set S:
//
//   signature(S) = [min{h₁(x) : x ∈ S}, min{h₂(x) : x ∈ S}, ..., min{hₖ(x) : x ∈ S}]
//
// Each component is the minimum hash value over all elements in the set.
//
// SIMILARITY ESTIMATION
// ---------------------
// Given signatures sig_A and sig_B, estimate Jaccard similarity by:
//   J_estimate(A, B) = (# positions where sig_A[i] = sig_B[i]) / k
//
// By the key insight, each position agrees with probability J(A, B), so the
// fraction of agreements is an unbiased estimator of J(A, B).
//
// ACCURACY
// --------
// With k hash functions, the standard error is:
//   SE = √(J(1-J)/k)
//
// For 95% confidence: J_estimate ± 2·SE
//
// Example with k=128 and true J=0.5:
//   SE = √(0.5 × 0.5 / 128) ≈ 0.044
//   95% CI: [0.41, 0.59]
//
// UNION PROPERTY
// --------------
// MinHash signatures can be merged to represent set unions:
//   signature(A ∪ B) = elementwise_min(signature(A), signature(B))
//
// This enables efficient similarity queries against unions of sets.
//
// WHY USE MINHASH?
// ----------------
// vs Direct Comparison:
//   ✓ O(k) similarity estimation instead of O(|A| + |B|)
//   ✓ Signatures are fixed-size - compare million-element sets in microseconds
//   ✓ Signatures can be stored/transmitted compactly (k × 8 bytes)
//   ✗ Only estimates similarity - not exact
//   ✗ Only works for Jaccard similarity, not other distance metrics
//
// vs Locality-Sensitive Hashing:
//   MinHash IS a form of LSH - signatures can be banded for approximate
//   nearest neighbor search in sub-linear time.
//
// WHEN TO CHOOSE MINHASH
// ----------------------
// • You need to compare many sets for similarity (O(n²) comparisons)
// • Sets are large but you only need approximate similarity
// • You want to find similar pairs among millions of documents
// • You're building near-duplicate detection or clustering systems
// • Examples: plagiarism detection, news deduplication, recommendation systems
//
// TEMPLATE PARAMETERS
// -------------------
//   NumHashes - Number of hash functions (k). More = better accuracy, more space.
//               Default 128 gives ~4.4% standard error for J=0.5.
//
// COMPLEXITY
// ----------
//   computeSignature: O(|S| × k) where |S| is set size and k = NumHashes
//   estimateSimilarity: O(k)
//   merge: O(k)
//   Space per signature: k × 8 bytes (k uint64_t values)
//
template <std::size_t NumHashes = 128>
class MinHash {
  static_assert(NumHashes > 0, "NumHashes must be positive");

 public:
  using Signature = std::array<std::uint64_t, NumHashes>;

  // Compute the MinHash signature for a range of elements.
  // Each element is hashed with k different hash functions, and the minimum
  // hash value for each function forms the signature.
  template <std::ranges::input_range Range>
  auto computeSignature(Range&& range) const -> Signature {
    Signature sig;
    sig.fill(std::numeric_limits<std::uint64_t>::max());

    for (const auto& element : range) {
      std::uint64_t base_hash = std::hash<std::ranges::range_value_t<Range>>{}(element);
      for (std::size_t i = 0; i < NumHashes; ++i) {
        std::uint64_t h = hashWithSeed(base_hash, i);
        sig[i] = std::min(sig[i], h);
      }
    }

    return sig;
  }

  // Estimate Jaccard similarity from two MinHash signatures.
  // Returns the fraction of hash functions where both signatures agree.
  // This is an unbiased estimator of J(A, B) = |A ∩ B| / |A ∪ B|.
  static auto estimateSimilarity(const Signature& sig_a, const Signature& sig_b)
      -> double {
    std::size_t matches = 0;
    for (std::size_t i = 0; i < NumHashes; ++i) {
      if (sig_a[i] == sig_b[i]) {
        ++matches;
      }
    }
    return static_cast<double>(matches) / static_cast<double>(NumHashes);
  }

  // Merge two signatures to produce the signature of their union.
  // signature(A ∪ B) = elementwise_min(signature(A), signature(B))
  static auto merge(const Signature& sig_a, const Signature& sig_b) -> Signature {
    Signature result;
    for (std::size_t i = 0; i < NumHashes; ++i) {
      result[i] = std::min(sig_a[i], sig_b[i]);
    }
    return result;
  }

  // Get the number of hash functions used.
  static constexpr auto numHashes() -> std::size_t { return NumHashes; }

 private:
  // Generate a hash value using a seed to simulate k independent hash functions.
  // Uses a combination of the base hash and seed with good mixing.
  static auto hashWithSeed(std::uint64_t base_hash, std::size_t seed)
      -> std::uint64_t {
    // Combine base hash with seed, then apply SplitMix64 for good mixing
    std::uint64_t x = base_hash ^ (seed * 0x9e3779b97f4a7c15ULL);
    // SplitMix64 mixing
    x += 0x9e3779b97f4a7c15ULL;
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
    return x ^ (x >> 31);
  }
};

}  // namespace tempura
