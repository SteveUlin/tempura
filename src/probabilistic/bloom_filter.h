#pragma once

#include <bitset>
#include <cmath>
#include <cstddef>
#include <functional>

namespace tempura {

// ============================================================================
// Bloom Filter
// ============================================================================
//
// A Bloom filter is a space-efficient probabilistic data structure for set
// membership testing. It can definitively say "not in set" but only
// probabilistically say "maybe in set" (false positives are possible).
//
// STRUCTURE
// ---------
// A Bloom filter is a bit array of m bits, initially all zeros. To add an
// element, hash it with k independent hash functions to get k bit positions,
// and set those bits to 1. To query, hash the element the same way and check
// if all k bits are set. If any bit is 0, the element is definitely not in
// the set. If all bits are 1, the element is probably in the set.
//
//   Add "apple":  h1(apple)=2, h2(apple)=5, h3(apple)=7
//                 [0 0 1 0 0 1 0 1 0 0]
//                      ↑     ↑   ↑
//
//   Add "banana": h1(banana)=1, h2(banana)=5, h3(banana)=9
//                 [0 1 1 0 0 1 0 1 0 1]
//                    ↑       ↑       ↑
//
//   Query "cherry": h1=2, h2=4, h3=7
//                   Bit 4 is 0 → definitely NOT in set ✓
//
//   Query "date": h1=1, h2=5, h3=7
//                 All bits are 1 → MAYBE in set (could be false positive!)
//
// FALSE POSITIVE RATE
// -------------------
// After inserting n elements into an m-bit filter with k hash functions:
//   P(false positive) ≈ (1 - e^(-kn/m))^k
//
// For optimal k given m and n:
//   k_optimal = (m/n) * ln(2) ≈ 0.693 * (m/n)
//
// Example: m=1000 bits, n=100 elements → k_optimal ≈ 7, P(FP) ≈ 0.8%
//
// WHY USE A BLOOM FILTER?
// -----------------------
// vs Hash Set (std::unordered_set):
//   ✓ Much more space efficient - only ~10 bits per element for 1% FP rate
//   ✓ O(k) constant time operations (k is typically small, 3-10)
//   ✓ No need to store the actual elements
//   ✗ Cannot enumerate elements or delete them (standard Bloom filter)
//   ✗ False positives - "maybe in set" is not "definitely in set"
//
// vs Exact Set:
//   ✓ 10x-100x smaller memory footprint
//   ✗ Probabilistic membership (but zero false negatives)
//
// WHEN TO CHOOSE A BLOOM FILTER
// -----------------------------
// • You can tolerate false positives but not false negatives
// • Memory is constrained and elements are large (URLs, file paths, etc.)
// • You want a fast "definitely not" check before expensive operations
// • Examples: cache lookup, spell checkers, network routers, databases
//
// TEMPLATE PARAMETERS
// -------------------
//   NumBits   - Size of the bit array (m). Larger = fewer false positives.
//   NumHashes - Number of hash functions (k). Optimal is ~0.693 * (m/n).
//
// COMPLEXITY
// ----------
//   add, contains:  O(k) where k = NumHashes
//   clear:          O(m) where m = NumBits
//   merge:          O(m) where m = NumBits
//   Space:          m bits (NumBits / 8 bytes)
//
template <std::size_t NumBits, std::size_t NumHashes>
class BloomFilter {
  static_assert(NumBits > 0, "NumBits must be positive");
  static_assert(NumHashes > 0, "NumHashes must be positive");

 public:
  // Add an item to the filter.
  template <typename T>
  void add(const T& item) {
    for (std::size_t i = 0; i < NumHashes; ++i) {
      bits_.set(hash(item, i));
    }
  }

  // Check if an item might be in the set.
  // Returns false: definitely not in set (no false negatives)
  // Returns true: maybe in set (false positives possible)
  template <typename T>
  auto contains(const T& item) const -> bool {
    for (std::size_t i = 0; i < NumHashes; ++i) {
      if (!bits_.test(hash(item, i))) {
        return false;
      }
    }
    return true;
  }

  // Reset the filter to empty state.
  void clear() { bits_.reset(); }

  // Combine two filters (OR operation). The result contains all elements
  // from both filters. Both filters must have the same parameters.
  void merge(const BloomFilter& other) { bits_ |= other.bits_; }

  // Get the number of bits set to 1.
  auto count() const -> std::size_t { return bits_.count(); }

  // Get the total number of bits.
  static constexpr auto size() -> std::size_t { return NumBits; }

  // Get the number of hash functions.
  static constexpr auto numHashes() -> std::size_t { return NumHashes; }

  // Estimate the false positive probability given n inserted elements.
  // P(FP) ≈ (1 - e^(-kn/m))^k
  static constexpr auto estimateFalsePositiveRate(std::size_t n) -> double {
    double k = static_cast<double>(NumHashes);
    double m = static_cast<double>(NumBits);
    double exponent = -k * static_cast<double>(n) / m;
    // Using a simple approximation: (1 - e^x)^k
    double base = 1.0 - std::exp(exponent);
    double result = 1.0;
    for (std::size_t i = 0; i < NumHashes; ++i) {
      result *= base;
    }
    return result;
  }

 private:
  // SplitMix64 - a fast, high-quality integer hash function.
  // Used to generate independent hash values from a seed.
  static constexpr auto splitmix64(std::size_t x) -> std::size_t {
    x += 0x9e3779b97f4a7c15ULL;
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
    return x ^ (x >> 31);
  }

  // Generate k different hash values using enhanced double hashing.
  // Uses double hashing: h_i(x) = (h1(x) + i * h2(x) + i*i) mod m
  // The i*i term (quadratic probing) improves distribution when h2 is small.
  template <typename T>
  auto hash(const T& item, std::size_t seed) const -> std::size_t {
    std::size_t h1 = std::hash<T>{}(item);
    // Apply splitmix64 to get h2 - ensures good bit mixing
    std::size_t h2 = splitmix64(h1);
    // Ensure h2 is odd (coprime to any power-of-2 NumBits) for better coverage
    h2 |= 1;
    return (h1 + seed * h2 + seed * seed) % NumBits;
  }

  std::bitset<NumBits> bits_;
};

}  // namespace tempura
