#pragma once

// ============================================================================
// Fibonacci Hashing: The Right Way to Index Power-of-Two Tables
// ============================================================================
//
// When mapping hash values to table indices, the naive approach is:
//
//   index = hash % table_size
//
// For power-of-two table sizes, this becomes:
//
//   index = hash & (table_size - 1)  // Fast bitwise AND
//
// But there's a problem: this only uses the LOW BITS of the hash. If your
// hash function has any weakness in its low bits (many do!), you get poor
// distribution.
//
// THE PROBLEM WITH LOW-BIT MASKING
// ================================
//
// Consider sequential integer keys with a mediocre hash:
//
//   hash(0) = 0x...00  → index 0
//   hash(1) = 0x...01  → index 1
//   hash(2) = 0x...02  → index 2
//   hash(3) = 0x...03  → index 3
//
// If the hash just passes through low bits (like identity or a weak mixer),
// you get sequential indices - terrible for probing-based tables!
//
// THE FIBONACCI HASHING SOLUTION
// ==============================
//
// Instead of masking, MULTIPLY by a special constant then use HIGH bits:
//
//   index = (hash * GOLDEN_RATIO) >> (64 - log2(table_size))
//
// The constant is 2^64 / φ (golden ratio), specifically:
//
//   2^64 / φ ≈ 11400714819323198485 (0x9E3779B97F4A7C15)
//
// WHY THE GOLDEN RATIO?
// =====================
//
// The golden ratio φ = (1 + √5) / 2 ≈ 1.618... has a special property:
//
//   Multiples of 1/φ are maximally spread out in [0, 1)
//
// This is related to the theory of Diophantine approximation. When you
// place points at positions 0, 1/φ, 2/φ, 3/φ, ... (mod 1), they spread
// out MORE EVENLY than any other multiplier!
//
// For any other constant k, eventually k×n gets close to a previous value.
// The golden ratio delays this clustering as long as possible.
//
// THE MULTIPLICATION TRICK
// ========================
//
// Multiplying by 2^64/φ and taking high bits has these effects:
//
// 1. EVERY input bit affects the output
//    Unlike masking which ignores high bits, multiplication spreads
//    influence from all positions.
//
// 2. Good distribution from golden ratio properties
//    Sequential inputs map to well-separated indices.
//
// 3. Fast on modern CPUs
//    A single 64-bit multiply + shift. CPUs are highly optimized for this.
//
// EXAMPLE: Sequential Keys
// ========================
//
//   hash(0) * φ → 0x0000... → index 0
//   hash(1) * φ → 0x9E37... → index 9  (for 16-bucket table)
//   hash(2) * φ → 0x3C6E... → index 3
//   hash(3) * φ → 0xDAA6... → index 13
//
// The indices jump around the table instead of clustering!
//
// WHEN TO USE FIBONACCI HASHING
// =============================
//
// Use it when:
//   - Table size is a power of 2 (required)
//   - Hash function may have weak low bits
//   - You want defense-in-depth against poor distribution
//
// Don't bother when:
//   - Using a prime table size (modulo already mixes bits)
//   - Hash function is already excellent (wyhash, xxHash3)
//   - Table size isn't power of 2
//
// KNUTH'S RECOMMENDATION
// ======================
//
// This technique comes from Donald Knuth's "The Art of Computer Programming"
// Volume 3, Section 6.4 on hashing. He calls it "multiplicative hashing"
// and analyzes why the golden ratio is optimal.
//
// The technique is sometimes called "Fibonacci hashing" because the golden
// ratio is intimately connected to the Fibonacci sequence:
//
//   lim(n→∞) F(n+1)/F(n) = φ
//
// ============================================================================

#include <bit>
#include <concepts>
#include <cstddef>
#include <cstdint>

namespace tempura {

// ----------------------------------------------------------------------------
// The Magic Constant
// ----------------------------------------------------------------------------
//
// 2^64 / φ, rounded to odd (odd numbers are coprime to 2^64, ensuring
// the multiplication is a bijection over the full 64-bit space).

constexpr std::uint64_t kFibonacciMultiplier = 0x9E3779B97F4A7C15ULL;

// 32-bit version: 2^32 / φ
constexpr std::uint32_t kFibonacciMultiplier32 = 0x9E3779B9U;

// ----------------------------------------------------------------------------
// Core Functions
// ----------------------------------------------------------------------------

// Map a 64-bit hash to an index in a table of size 2^shift
// shift should be log2(table_size)
constexpr auto fibonacciIndex64(std::uint64_t hash, unsigned shift) noexcept
    -> std::size_t {
  return static_cast<std::size_t>((hash * kFibonacciMultiplier) >> (64 - shift));
}

// Map a 32-bit hash to an index in a table of size 2^shift
constexpr auto fibonacciIndex32(std::uint32_t hash, unsigned shift) noexcept
    -> std::size_t {
  return static_cast<std::size_t>((hash * kFibonacciMultiplier32) >> (32 - shift));
}

// Convenience: map hash to index given table_size (must be power of 2)
constexpr auto fibonacciIndex(std::uint64_t hash,
                              std::size_t table_size) noexcept -> std::size_t {
  // table_size must be power of 2 for this to work correctly
  unsigned shift = static_cast<unsigned>(std::bit_width(table_size) - 1);
  return fibonacciIndex64(hash, shift);
}

// ----------------------------------------------------------------------------
// FibonacciReducer: Stateful Version for Repeated Use
// ----------------------------------------------------------------------------
//
// When doing many lookups with the same table size, precompute the shift
// to avoid repeated bit_width calls.

class FibonacciReducer {
 public:
  // table_size must be a power of 2
  constexpr explicit FibonacciReducer(std::size_t table_size) noexcept
      : shift_{static_cast<unsigned>(std::bit_width(table_size) - 1)} {}

  constexpr auto reduce(std::uint64_t hash) const noexcept -> std::size_t {
    return static_cast<std::size_t>((hash * kFibonacciMultiplier) >> (64 - shift_));
  }

  // Alias for reduce
  constexpr auto operator()(std::uint64_t hash) const noexcept -> std::size_t {
    return reduce(hash);
  }

 private:
  unsigned shift_;
};

// ----------------------------------------------------------------------------
// Alternative: Fast Range Reduction
// ----------------------------------------------------------------------------
//
// Another technique for mapping hash to [0, n): the "fast range" method.
// Instead of modulo, use: (hash * n) >> 64
//
// This works for ANY n (not just powers of 2) and is faster than division.
//
// The idea: hash/2^64 ≈ uniform in [0,1), so (hash/2^64)*n ≈ uniform in [0,n)
// Implemented as: (hash * n) >> 64 using 128-bit arithmetic.

constexpr auto fastRangeReduce(std::uint64_t hash, std::uint64_t range) noexcept
    -> std::uint64_t {
  // We need (hash * range) >> 64, which requires 128-bit multiplication
  // Use compiler builtin or split into high/low

#ifdef __SIZEOF_INT128__
  // If 128-bit integers available, use them directly
  return static_cast<std::uint64_t>(
      (static_cast<__uint128_t>(hash) * range) >> 64);
#else
  // Fallback: compute high 64 bits of 64x64->128 multiplication
  // Split each value into high and low 32-bit parts

  std::uint64_t hash_lo = hash & 0xFFFFFFFF;
  std::uint64_t hash_hi = hash >> 32;
  std::uint64_t range_lo = range & 0xFFFFFFFF;
  std::uint64_t range_hi = range >> 32;

  // Full product = hash_hi*range_hi*2^64 + (hash_hi*range_lo +
  // hash_lo*range_hi)*2^32 + hash_lo*range_lo
  // We need bits 64-127

  std::uint64_t ll = hash_lo * range_lo;
  std::uint64_t lh = hash_lo * range_hi;
  std::uint64_t hl = hash_hi * range_lo;
  std::uint64_t hh = hash_hi * range_hi;

  std::uint64_t mid = (ll >> 32) + (lh & 0xFFFFFFFF) + (hl & 0xFFFFFFFF);
  std::uint64_t hi = hh + (lh >> 32) + (hl >> 32) + (mid >> 32);

  return hi;
#endif
}

// Convenience wrapper
class FastRangeReducer {
 public:
  constexpr explicit FastRangeReducer(std::uint64_t range) noexcept
      : range_{range} {}

  constexpr auto reduce(std::uint64_t hash) const noexcept -> std::uint64_t {
    return fastRangeReduce(hash, range_);
  }

  constexpr auto operator()(std::uint64_t hash) const noexcept -> std::uint64_t {
    return reduce(hash);
  }

 private:
  std::uint64_t range_;
};

// ----------------------------------------------------------------------------
// Integer Mixer (Bijective)
// ----------------------------------------------------------------------------
//
// Sometimes you need to mix an integer's bits without using it as a table
// index. The Fibonacci multiplier is a bijection over 64-bit integers
// (because it's odd, hence coprime to 2^64).
//
// This is useful as a cheap but effective integer hash, especially for
// sequential keys.

constexpr auto fibonacciMix64(std::uint64_t x) noexcept -> std::uint64_t {
  return x * kFibonacciMultiplier;
}

constexpr auto fibonacciMix32(std::uint32_t x) noexcept -> std::uint32_t {
  return x * kFibonacciMultiplier32;
}

// With XOR-shift for better avalanche
constexpr auto fibonacciHash64(std::uint64_t x) noexcept -> std::uint64_t {
  x *= kFibonacciMultiplier;
  x ^= x >> 32;
  x *= kFibonacciMultiplier;
  x ^= x >> 32;
  return x;
}

constexpr auto fibonacciHash32(std::uint32_t x) noexcept -> std::uint32_t {
  x *= kFibonacciMultiplier32;
  x ^= x >> 16;
  x *= kFibonacciMultiplier32;
  x ^= x >> 16;
  return x;
}

}  // namespace tempura
