#pragma once

// ============================================================================
// wyhash: The Fastest Quality Hash Function
// ============================================================================
//
// wyhash was created by Wang Yi in 2019 and has quickly become one of the
// most influential hash functions. It's now the default hash in Zig, V, Nim,
// and Go (since 1.17). Its appeal: extreme simplicity AND extreme speed.
//
// THE KEY INSIGHT: MUM (MUltiply and Mix)
// =======================================
//
// wyhash is built on a single elegant primitive called "mum" (multiply-mix):
//
//   mum(a, b) = lo64(a × b) XOR hi64(a × b)
//
// where a × b is a full 64×64 → 128-bit multiplication, then we XOR the
// high and low 64-bit halves together.
//
// Why is this so powerful?
//
// 1. FULL MULTIPLICATION
//    64×64 multiplication produces 128 bits of output. Every input bit
//    influences nearly every output bit through the cascade of carries.
//    This provides excellent avalanche in a single operation.
//
// 2. XOR FOLDING
//    XORing the high and low halves combines information that would
//    otherwise be lost (if we just took the low 64 bits). This preserves
//    the influence of high-order input bits.
//
// 3. HARDWARE ACCELERATION
//    Modern CPUs have fast 64×64→128 multiplication (MULX on x86, UMULH on
//    ARM). What seems expensive is actually very efficient.
//
// THE ALGORITHM
// =============
//
// wyhash processes input in 48-byte chunks (for parallelism), with a
// simplified path for shorter inputs:
//
// Short inputs (≤16 bytes):
//   1. Read input with special handling for length
//   2. Mix with seed using mum()
//
// Medium inputs (17-48 bytes):
//   1. Read three 16-byte blocks (with overlap for short inputs)
//   2. Combine using mum() operations
//
// Long inputs (>48 bytes):
//   1. Process 48-byte chunks in parallel (3 accumulators)
//   2. Combine accumulators at the end
//
// THE MAGIC PRIMES
// ================
//
// wyhash uses four prime numbers as mixing constants:
//
//   p0 = 0xa0761d6478bd642f
//   p1 = 0xe7037ed1a0b428db
//   p2 = 0x8ebc6af09c88c6e3
//   p3 = 0x589965cc75374cc3
//
// These were selected because:
//   - They're odd (invertible in modular arithmetic)
//   - They have good bit distribution (~50% ones)
//   - They're large enough to ensure full mixing in multiplication
//
// Unlike MurmurHash, the constants aren't from complex optimization -
// they're simply well-distributed odd numbers. The mum operation is so
// powerful that constant selection matters less.
//
// SECRET/SALT MECHANISM
// =====================
//
// wyhash supports a "secret" (or "salt") - a set of four 64-bit values
// that can be used instead of the default primes. This provides:
//
//   - Protection against hash-flooding attacks (when secret is random)
//   - Different hash spaces for different use cases
//   - Some resistance to crafted collision attacks
//
// However, this is still NOT cryptographically secure. A determined
// attacker can still find collisions. For real security, use SipHash.
//
// PERFORMANCE CHARACTERISTICS
// ===========================
//
// wyhash excels in several dimensions:
//
//   - Speed: 10-20 GB/s on modern hardware (faster than memcpy!)
//   - Latency: Very fast for short keys (critical for hash tables)
//   - Code size: The entire implementation fits in ~100 lines
//   - Portability: Works on 32/64-bit, big/little endian
//
// The speed comes from:
//   - Exploiting fast 128-bit multiply hardware
//   - Processing multiple streams in parallel
//   - Minimal operations per byte
//   - Good instruction-level parallelism
//
// COMPARISON WITH OTHER HASHES
// ============================
//
//   FNV-1a:    Simple but slow (byte-by-byte), poor avalanche
//   MurmurHash: Good quality, moderate speed (superseded)
//   xxHash:    Excellent speed and quality (close competitor)
//   wyhash:    Fastest, excellent quality, simplest code
//
// wyhash has essentially obsoleted MurmurHash for new applications.
// xxHash remains competitive and has stronger security properties
// (xxHash3 is designed to resist more attacks).
//
// ============================================================================

#include <cstddef>
#include <cstdint>
#include <string_view>

#include "hash/hash.h"

namespace tempura {

// ----------------------------------------------------------------------------
// wyhash Constants
// ----------------------------------------------------------------------------
//
// These primes were selected for good bit distribution. They're not magic -
// any well-distributed odd 64-bit numbers would work reasonably well.

namespace wyhash_detail {

constexpr std::uint64_t kWyP0 = 0xa0761d6478bd642fULL;
constexpr std::uint64_t kWyP1 = 0xe7037ed1a0b428dbULL;
constexpr std::uint64_t kWyP2 = 0x8ebc6af09c88c6e3ULL;
constexpr std::uint64_t kWyP3 = 0x589965cc75374cc3ULL;

// Default secret (can be replaced with random values for hash-flooding defense)
constexpr std::uint64_t kDefaultSecret[4] = {kWyP0, kWyP1, kWyP2, kWyP3};

}  // namespace wyhash_detail

// ----------------------------------------------------------------------------
// 128-bit Multiplication Primitive
// ----------------------------------------------------------------------------
//
// This is the heart of wyhash: multiply two 64-bit numbers to get 128 bits,
// then XOR the high and low halves. Modern CPUs do this in ~3-4 cycles.
//
// The implementation splits each 64-bit value into high and low 32-bit halves:
//   a = ah * 2^32 + al
//   b = bh * 2^32 + bl
//
// Then: a * b = ah*bh * 2^64 + (ah*bl + al*bh) * 2^32 + al*bl
//
// The 128-bit result is accumulated and XOR-folded to 64 bits.

constexpr auto wymum(std::uint64_t a, std::uint64_t b) noexcept
    -> std::uint64_t {
  // Split into 32-bit halves
  const std::uint64_t al = static_cast<std::uint32_t>(a);
  const std::uint64_t ah = a >> 32;
  const std::uint64_t bl = static_cast<std::uint32_t>(b);
  const std::uint64_t bh = b >> 32;

  // Four 32×32 → 64 multiplications
  const std::uint64_t ll = al * bl;  // Low × Low
  const std::uint64_t hh = ah * bh;  // High × High
  const std::uint64_t lh = al * bh;  // Low × High
  const std::uint64_t hl = ah * bl;  // High × Low

  // Combine to get high and low 64-bit halves of 128-bit result
  // lo64 = ll + ((lh + hl) << 32)
  // hi64 = hh + ((lh + hl) >> 32) + carries

  const std::uint64_t mid = lh + hl;
  const std::uint64_t lo = ll + (mid << 32);
  const std::uint64_t hi =
      hh + (mid >> 32) + (lo < ll ? 1 : 0) + (mid < lh ? (1ULL << 32) : 0);

  // XOR-fold: combine both halves
  return lo ^ hi;
}

// Alternative: wymix for when we want separate handling
// Returns XOR of inputs and their mum
constexpr auto wymix(std::uint64_t a, std::uint64_t b) noexcept
    -> std::uint64_t {
  return wymum(a ^ wyhash_detail::kWyP0, b ^ wyhash_detail::kWyP1);
}

// ----------------------------------------------------------------------------
// Reading Bytes with Protection
// ----------------------------------------------------------------------------
//
// wyhash uses a clever trick for reading partial blocks: it always reads
// 8 bytes but shifts/masks to handle shorter inputs. This avoids branches
// in the hot path.

constexpr auto wyread8(const std::uint8_t* p) noexcept -> std::uint64_t {
  return read64LE(p);
}

constexpr auto wyread4(const std::uint8_t* p) noexcept -> std::uint64_t {
  return read32LE(p);
}

// Read 1-3 bytes safely (protects against reading past buffer end)
constexpr auto wyread3(const std::uint8_t* p, std::size_t k) noexcept
    -> std::uint64_t {
  // k is 1, 2, or 3 (never 0)
  // Reads: p[0], p[k>>1], p[k-1] (overlapping for k=1,2)
  return (static_cast<std::uint64_t>(p[0]) << 16) |
         (static_cast<std::uint64_t>(p[k >> 1]) << 8) |
         static_cast<std::uint64_t>(p[k - 1]);
}

// ----------------------------------------------------------------------------
// wyhash Core Function
// ----------------------------------------------------------------------------
//
// The hash function processes input in three phases based on length:
//   - ≤8 bytes: Special fast path
//   - 9-16 bytes: Two 8-byte reads
//   - >16 bytes: Bulk processing with accumulator

constexpr auto wyhash(const std::uint8_t* data, std::size_t len,
                      std::uint64_t seed = 0) noexcept -> std::uint64_t {
  using namespace wyhash_detail;

  const std::uint8_t* p = data;

  // XOR seed with a prime to ensure non-zero starting state
  seed ^= kWyP0;

  std::uint64_t a;
  std::uint64_t b;

  if (len <= 16) {
    // ------------------------------------------------------------------------
    // Short inputs: ≤16 bytes
    // ------------------------------------------------------------------------
    // These are the most common case in hash tables (keys like integers,
    // short strings). We optimize heavily for low latency.

    if (len >= 4) {
      // 4-16 bytes: read from both ends with overlap
      a = (wyread4(p) << 32) | wyread4(p + ((len >> 3) << 2));
      b = (wyread4(p + len - 4) << 32) | wyread4(p + len - 4 - ((len >> 3) << 2));
    } else if (len > 0) {
      // 1-3 bytes: pack carefully to avoid buffer overread
      // Use direct mixing rather than wymum(a, 0) which would be 0
      a = wyread3(p, len);
      return wymum(a ^ kWyP0, len ^ kWyP1);
    } else {
      // Empty input: return hash of just the seed and length
      return wymum(seed, kWyP1);
    }
  } else {
    // ------------------------------------------------------------------------
    // Medium and long inputs: >16 bytes
    // ------------------------------------------------------------------------

    if (len <= 48) {
      // 17-48 bytes: process in up to three 16-byte blocks
      a = wyread8(p) ^ kWyP1;
      b = wyread8(p + 8) ^ seed;
      std::uint64_t r = wymum(a, b);

      if (len > 32) {
        a = wyread8(p + 16) ^ kWyP2;
        b = wyread8(p + 24) ^ r;
        r = wymum(a, b);
      }

      a = wyread8(p + len - 16) ^ kWyP3;
      b = wyread8(p + len - 8) ^ r;
    } else {
      // >48 bytes: use three parallel accumulators for ILP
      std::uint64_t see1 = seed;
      std::uint64_t see2 = seed;

      // Process 48-byte chunks
      while (len > 48) {
        seed = wymum(wyread8(p) ^ kWyP1, wyread8(p + 8) ^ seed);
        see1 = wymum(wyread8(p + 16) ^ kWyP2, wyread8(p + 24) ^ see1);
        see2 = wymum(wyread8(p + 32) ^ kWyP3, wyread8(p + 40) ^ see2);
        p += 48;
        len -= 48;
      }

      // Combine accumulators
      seed ^= see1 ^ see2;

      // Process remaining 1-48 bytes (recursive logic, but we inline it)
      if (len > 32) {
        a = wyread8(p) ^ kWyP1;
        b = wyread8(p + 8) ^ seed;
        seed = wymum(a, b);

        a = wyread8(p + 16) ^ kWyP2;
        b = wyread8(p + 24) ^ seed;
        seed = wymum(a, b);
      } else if (len > 16) {
        a = wyread8(p) ^ kWyP1;
        b = wyread8(p + 8) ^ seed;
        seed = wymum(a, b);
      }

      // Final 16 bytes (always present for len > 48)
      a = wyread8(data + len - 16) ^ kWyP3;
      b = wyread8(data + len - 8) ^ seed;
    }
  }

  // Final mixing: combine with length for uniqueness
  return wymum(kWyP1 ^ len, wymum(a, b));
}

// ----------------------------------------------------------------------------
// Wyhash Hasher Classes (std::hash compatible)
// ----------------------------------------------------------------------------

template <typename T>
struct WyHash;

template <>
struct WyHash<std::string_view> {
  constexpr auto operator()(std::string_view s) const noexcept -> std::size_t {
    return wyhash(reinterpret_cast<const std::uint8_t*>(s.data()), s.size());
  }
};

template <>
struct WyHash<std::uint64_t> {
  constexpr auto operator()(std::uint64_t x) const noexcept -> std::size_t {
    // For integers, wyhash can be simplified: just use wymum directly
    // This is much faster than serializing to bytes
    return wymum(x ^ wyhash_detail::kWyP0, wyhash_detail::kWyP1);
  }
};

template <>
struct WyHash<std::uint32_t> {
  constexpr auto operator()(std::uint32_t x) const noexcept -> std::size_t {
    return wymum(x ^ wyhash_detail::kWyP0, wyhash_detail::kWyP1);
  }
};

template <>
struct WyHash<std::int64_t> {
  constexpr auto operator()(std::int64_t x) const noexcept -> std::size_t {
    return WyHash<std::uint64_t>{}(static_cast<std::uint64_t>(x));
  }
};

template <>
struct WyHash<std::int32_t> {
  constexpr auto operator()(std::int32_t x) const noexcept -> std::size_t {
    return WyHash<std::uint32_t>{}(static_cast<std::uint32_t>(x));
  }
};

}  // namespace tempura
