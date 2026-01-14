#pragma once

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <ranges>

namespace tempura {

// ============================================================================
// b-bit MinHash for Space-Efficient Jaccard Similarity
// ============================================================================
//
// Standard MinHash stores 64 bits per hash value (k hashes = 64k bits).
// b-bit MinHash stores only the lowest b bits of each min-hash value,
// achieving massive space savings at the cost of slightly more complex
// similarity estimation.
//
// SPACE SAVINGS
// -------------
// Storage comparison for k=256 hashes:
//   Standard MinHash: 256 × 64 bits = 2048 bytes
//   b-bit (b=1):      256 × 1 bits  = 32 bytes   (64× compression!)
//   b-bit (b=2):      256 × 2 bits  = 64 bytes   (32× compression)
//   b-bit (b=4):      256 × 4 bits  = 128 bytes  (16× compression)
//
// THE KEY INSIGHT
// ---------------
// When we store only the lowest b bits, the probability of a match depends on
// both the true Jaccard similarity AND random chance collision:
//
// Let s = J(A, B) be the true Jaccard similarity. For b-bit MinHash:
//
//   P(b lowest bits match) = s + (1-s)/2^b
//
// This formula arises because:
// - With probability s, both sets share the same minimum (from intersection)
//   so the b bits match with certainty
// - With probability (1-s), the minimums are from different elements
//   (independent random values), so b bits match by chance with P = 1/2^b
//
// EXAMPLES
// --------
// For b=1:  P(match) = s + (1-s)/2 = (s+1)/2
//           At s=0: P(match) = 0.5 (random coin flip)
//           At s=1: P(match) = 1.0 (identical)
//
// For b=2:  P(match) = s + (1-s)/4 = (3s+1)/4
//           At s=0: P(match) = 0.25
//           At s=1: P(match) = 1.0
//
// For b=8:  P(match) = s + (1-s)/256
//           At s=0: P(match) ≈ 0.004
//           At s=1: P(match) = 1.0
//
// INVERTING TO ESTIMATE SIMILARITY
// ---------------------------------
// Given r = matches/k (the fraction of matching b-bit values), solve for s:
//
//   r = s + (1-s)/2^b
//   r = s(1 - 1/2^b) + 1/2^b
//   r - 1/2^b = s(1 - 1/2^b)
//   s = (r - 1/2^b) / (1 - 1/2^b)
//   s = (r·2^b - 1) / (2^b - 1)
//
// For b=1:
//   s_hat = (2r - 1) / (2 - 1) = 2r - 1 = 2(matches/k) - 1
//
// For b=8:
//   s_hat = (256r - 1) / 255
//
// VARIANCE AND ACCURACY
// ---------------------
// b-bit MinHash has higher variance than standard MinHash for the same k.
// For b=1, the variance is approximately:
//
//   Var(s_hat) ≈ (1+s)³(1-s) / (4ks²)
//
// To achieve the same variance as standard MinHash with k hashes, b-bit
// needs more hashes. But the space per hash is smaller, so overall:
//
//   - b=1 typically needs 2-3× more hashes for same accuracy
//   - But each hash uses 1/64 the space, so still 20-30× space savings
//
// WHEN TO USE b-bit MINHASH
// -------------------------
// • Memory is severely constrained (embedded, high-volume streaming)
// • You have many signatures to store (similarity search at scale)
// • Can tolerate slightly higher estimation error
// • Comparing near-duplicate documents (high similarity estimates are stable)
//
// When NOT to use:
// • Very low similarity regions (s < 0.1) - variance explodes
// • When you need the merge operation (b-bit signatures don't merge cleanly)
//
// TEMPLATE PARAMETERS
// -------------------
//   NumHashes - Number of hash functions (k). Default 256.
//               More hashes = better accuracy, more space.
//   NumBits   - Bits stored per hash (b). Default 1.
//               Fewer bits = more compression, higher variance.
//
// COMPLEXITY
// ----------
//   computeSignature:    O(|S| × k) where |S| is set size
//   estimateSimilarity:  O(k/64) - operates on packed uint64_t words
//   countMatches:        O(k/64)
//   Space per signature: ceil(k × b / 64) × 8 bytes
//                        For k=256, b=1: 32 bytes (vs 2048 for standard)
//
template <std::size_t NumHashes = 256, std::size_t NumBits = 1>
class BbitMinHash {
  static_assert(NumHashes > 0, "NumHashes must be positive");
  static_assert(NumBits >= 1 && NumBits <= 64, "NumBits must be in [1, 64]");

 public:
  // Number of uint64_t words needed to store all the packed bits.
  // Each word holds 64/NumBits hash values (or parts thereof).
  static constexpr std::size_t kTotalBits = NumHashes * NumBits;
  static constexpr std::size_t kNumWords = (kTotalBits + 63) / 64;

  // Signature is an array of packed uint64_t words.
  using Signature = std::array<std::uint64_t, kNumWords>;

  // Mask for extracting NumBits from a hash value.
  static constexpr std::uint64_t kBitMask = (std::uint64_t{1} << NumBits) - 1;

  // Constants for the b-bit correction formula: s = (r*2^b - 1) / (2^b - 1)
  static constexpr double kTwoPowB = static_cast<double>(std::uint64_t{1} << NumBits);
  static constexpr double kDenominator = kTwoPowB - 1.0;  // 2^b - 1

  // Compute the b-bit MinHash signature for a range of elements.
  // For each hash function, finds the minimum hash value, then stores
  // only the lowest NumBits bits in a packed format.
  template <std::ranges::input_range Range>
  auto computeSignature(Range&& range) const -> Signature {
    // First compute full 64-bit minimums (like standard MinHash).
    std::array<std::uint64_t, NumHashes> min_hashes;
    min_hashes.fill(std::numeric_limits<std::uint64_t>::max());

    for (const auto& element : range) {
      std::uint64_t base_hash =
          std::hash<std::ranges::range_value_t<Range>>{}(element);
      for (std::size_t i = 0; i < NumHashes; ++i) {
        std::uint64_t h = hashWithSeed(base_hash, i);
        min_hashes[i] = std::min(min_hashes[i], h);
      }
    }

    // Now pack only the lowest NumBits of each minimum into the signature.
    return packSignature(min_hashes);
  }

  // Estimate Jaccard similarity from two b-bit MinHash signatures.
  // Uses the b-bit correction formula: s_hat = (r*2^b - 1) / (2^b - 1)
  // where r = matches/k is the fraction of matching b-bit values.
  static auto estimateSimilarity(const Signature& sig_a, const Signature& sig_b)
      -> double {
    std::size_t matches = countMatches(sig_a, sig_b);
    double r = static_cast<double>(matches) / static_cast<double>(NumHashes);

    // s_hat = (r * 2^b - 1) / (2^b - 1)
    double s_hat = (r * kTwoPowB - 1.0) / kDenominator;

    // Clamp to [0, 1] range (estimation can slightly exceed bounds due to variance).
    return std::clamp(s_hat, 0.0, 1.0);
  }

  // Count how many b-bit hash values match between two signatures.
  static auto countMatches(const Signature& sig_a, const Signature& sig_b)
      -> std::size_t {
    if constexpr (NumBits == 1) {
      // Special fast path for b=1: use popcount on XNORed words.
      return countMatchesSingleBit(sig_a, sig_b);
    } else {
      // General case: compare each b-bit value individually.
      return countMatchesMultiBit(sig_a, sig_b);
    }
  }

  // Get the number of hash functions used.
  static constexpr auto numHashes() -> std::size_t { return NumHashes; }

  // Get the number of bits stored per hash.
  static constexpr auto numBits() -> std::size_t { return NumBits; }

  // Get the signature size in bytes.
  static constexpr auto signatureSizeBytes() -> std::size_t {
    return kNumWords * sizeof(std::uint64_t);
  }

  // Compression ratio vs standard MinHash (storing full 64-bit values).
  static constexpr auto compressionRatio() -> double {
    return 64.0 / static_cast<double>(NumBits);
  }

 private:
  // Generate a hash value using a seed to simulate k independent hash functions.
  static auto hashWithSeed(std::uint64_t base_hash, std::size_t seed)
      -> std::uint64_t {
    // Combine base hash with seed, then apply SplitMix64 for good mixing.
    std::uint64_t x = base_hash ^ (seed * 0x9e3779b97f4a7c15ULL);
    x += 0x9e3779b97f4a7c15ULL;
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
    return x ^ (x >> 31);
  }

  // Pack an array of full 64-bit minimums into a b-bit signature.
  static auto packSignature(const std::array<std::uint64_t, NumHashes>& min_hashes)
      -> Signature {
    Signature sig{};

    if constexpr (NumBits == 1) {
      // Fast path for b=1: pack one bit per hash into consecutive bits.
      for (std::size_t i = 0; i < NumHashes; ++i) {
        std::size_t word_idx = i / 64;
        std::size_t bit_idx = i % 64;
        std::uint64_t bit = min_hashes[i] & 1;
        sig[word_idx] |= (bit << bit_idx);
      }
    } else if constexpr (64 % NumBits == 0) {
      // Medium path: NumBits divides 64 evenly (2, 4, 8, 16, 32, 64).
      constexpr std::size_t kValuesPerWord = 64 / NumBits;
      for (std::size_t i = 0; i < NumHashes; ++i) {
        std::size_t word_idx = i / kValuesPerWord;
        std::size_t slot_idx = i % kValuesPerWord;
        std::uint64_t bits = min_hashes[i] & kBitMask;
        sig[word_idx] |= (bits << (slot_idx * NumBits));
      }
    } else {
      // General case: bits may span word boundaries.
      std::size_t bit_offset = 0;
      for (std::size_t i = 0; i < NumHashes; ++i) {
        std::uint64_t bits = min_hashes[i] & kBitMask;
        std::size_t word_idx = bit_offset / 64;
        std::size_t in_word_offset = bit_offset % 64;

        sig[word_idx] |= (bits << in_word_offset);

        // Handle spillover to next word.
        if (in_word_offset + NumBits > 64 && word_idx + 1 < kNumWords) {
          std::size_t spillover = in_word_offset + NumBits - 64;
          sig[word_idx + 1] |= (bits >> (NumBits - spillover));
        }

        bit_offset += NumBits;
      }
    }

    return sig;
  }

  // Extract the i-th b-bit value from a signature.
  static auto extractValue(const Signature& sig, std::size_t i) -> std::uint64_t {
    if constexpr (NumBits == 1) {
      std::size_t word_idx = i / 64;
      std::size_t bit_idx = i % 64;
      return (sig[word_idx] >> bit_idx) & 1;
    } else if constexpr (64 % NumBits == 0) {
      constexpr std::size_t kValuesPerWord = 64 / NumBits;
      std::size_t word_idx = i / kValuesPerWord;
      std::size_t slot_idx = i % kValuesPerWord;
      return (sig[word_idx] >> (slot_idx * NumBits)) & kBitMask;
    } else {
      std::size_t bit_offset = i * NumBits;
      std::size_t word_idx = bit_offset / 64;
      std::size_t in_word_offset = bit_offset % 64;

      std::uint64_t value = (sig[word_idx] >> in_word_offset) & kBitMask;

      // Handle values spanning word boundaries.
      if (in_word_offset + NumBits > 64 && word_idx + 1 < kNumWords) {
        std::size_t bits_in_first = 64 - in_word_offset;
        std::size_t bits_in_second = NumBits - bits_in_first;
        std::uint64_t mask_second = (std::uint64_t{1} << bits_in_second) - 1;
        value |= (sig[word_idx + 1] & mask_second) << bits_in_first;
      }

      return value;
    }
  }

  // Fast match counting for b=1 using XNOR and popcount.
  static auto countMatchesSingleBit(const Signature& sig_a, const Signature& sig_b)
      -> std::size_t {
    std::size_t matches = 0;

    // For complete words, use popcount on XNOR result.
    constexpr std::size_t kCompleteWords = NumHashes / 64;
    for (std::size_t w = 0; w < kCompleteWords; ++w) {
      std::uint64_t xnor_result = ~(sig_a[w] ^ sig_b[w]);
      matches += std::popcount(xnor_result);
    }

    // Handle remaining bits in the last partial word.
    constexpr std::size_t kRemainingBits = NumHashes % 64;
    if constexpr (kRemainingBits > 0) {
      constexpr std::uint64_t kPartialMask =
          (std::uint64_t{1} << kRemainingBits) - 1;
      std::uint64_t xnor_result = ~(sig_a[kCompleteWords] ^ sig_b[kCompleteWords]);
      matches += std::popcount(xnor_result & kPartialMask);
    }

    return matches;
  }

  // General match counting for b > 1.
  static auto countMatchesMultiBit(const Signature& sig_a, const Signature& sig_b)
      -> std::size_t {
    std::size_t matches = 0;
    for (std::size_t i = 0; i < NumHashes; ++i) {
      if (extractValue(sig_a, i) == extractValue(sig_b, i)) {
        ++matches;
      }
    }
    return matches;
  }
};

}  // namespace tempura
