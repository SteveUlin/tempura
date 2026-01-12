#pragma once

// ============================================================================
// CRC32C: Hardware-Accelerated Hashing via SSE4.2
// ============================================================================
//
// CRC32C is a cyclic redundancy check that's built directly into x86 CPUs.
// The "C" stands for "Castagnoli" - the polynomial used (0x1EDC6F41),
// which has better error-detection properties than the older CRC32-IEEE.
//
// WHY USE HARDWARE CRC32C FOR HASHING?
// ====================================
//
// The CRC32 instruction is EXTREMELY fast:
//
//   - Throughput: ~0.12 cycles per byte (8 bytes per cycle on modern CPUs)
//   - Latency: 3 cycles per 8-byte operation
//   - Speed: 30+ GB/s on a 4 GHz processor
//
// This makes it one of the fastest hash functions available - faster than
// wyhash for large inputs, and competitive for small ones.
//
// THE MATHEMATICS: CRC AS POLYNOMIAL DIVISION
// ===========================================
//
// CRC treats input data as coefficients of a polynomial over GF(2):
//
//   Data bytes [0x83, 0x01] → x^15 + x^7 + x^1 + x^0
//
// The CRC is the remainder when dividing this polynomial by a fixed
// "generator polynomial" G(x). For CRC32C:
//
//   G(x) = x^32 + x^28 + x^27 + x^26 + x^25 + x^23 + x^22 + x^20 +
//          x^19 + x^18 + x^14 + x^13 + x^11 + x^10 + x^9 + x^8 + x^6 + 1
//
// The hardware instruction computes this division in a single cycle.
//
// WHY CRC MAKES A DECENT HASH
// ===========================
//
// CRCs were designed for ERROR DETECTION, not hashing, but they share
// key properties:
//
//   ✓ Fast: Hardware instruction, highly optimized
//   ✓ Deterministic: Same input → same output
//   ✓ Good avalanche for small changes (single bit errors)
//
// Limitations for hashing:
//
//   ✗ Linear: CRC(A ⊕ B) = CRC(A) ⊕ CRC(B) - attackers can craft collisions
//   ✗ Not uniform: Distribution isn't as good as MurmurHash/wyhash
//   ✗ Only 32 bits: Higher collision probability than 64-bit hashes
//
// Best uses:
//   - Checksums (what it's designed for)
//   - Hash tables with TRUSTED input (internal data structures)
//   - Combining with other operations for speed
//
// Avoid when:
//   - Processing untrusted/adversarial input (HashDoS vulnerable)
//   - Need strong uniformity guarantees
//
// THE SSE4.2 INSTRUCTIONS
// =======================
//
// SSE4.2 (introduced with Intel Nehalem, 2008) provides:
//
//   _mm_crc32_u8(crc, data)   - Process 1 byte
//   _mm_crc32_u16(crc, data)  - Process 2 bytes
//   _mm_crc32_u32(crc, data)  - Process 4 bytes
//   _mm_crc32_u64(crc, data)  - Process 8 bytes (64-bit mode only)
//
// The 64-bit version is fastest, so we use it for bulk processing and
// fall back to smaller versions for the tail.
//
// ADVANCED: PARALLEL CRC FOR EVEN MORE SPEED
// ==========================================
//
// The CRC32 instruction has latency (3 cycles) but can issue every cycle.
// To hide latency, we can compute MULTIPLE CRCs in parallel over different
// parts of the data, then combine them at the end. This can achieve
// ~3x speedup for large inputs.
//
// This implementation uses a simpler sequential approach for clarity,
// but the parallel technique is used in production systems like Ceph
// and LevelDB.
//
// ============================================================================

#include <cstddef>
#include <cstdint>
#include <nmmintrin.h>  // SSE4.2 intrinsics
#include <string_view>

#include "hash/hash.h"

namespace tempura {

// ----------------------------------------------------------------------------
// CRC32C Core Implementation
// ----------------------------------------------------------------------------
//
// Process data 8 bytes at a time using the hardware instruction.
// The initial CRC value can be used as a seed for different hash spaces.

inline auto crc32c(const std::uint8_t* data, std::size_t len,
                   std::uint32_t initial = 0) noexcept -> std::uint32_t {
  // Start with bitwise inverse of initial value (CRC convention)
  std::uint64_t crc = ~static_cast<std::uint64_t>(initial);

  // Process 8 bytes at a time for maximum throughput
  // The _mm_crc32_u64 instruction processes 8 bytes in 3 cycles
  while (len >= 8) {
    // Unaligned load is fine - the instruction handles it
    std::uint64_t chunk;
    __builtin_memcpy(&chunk, data, 8);
    crc = _mm_crc32_u64(crc, chunk);
    data += 8;
    len -= 8;
  }

  // Process remaining 4 bytes if present
  if (len >= 4) {
    std::uint32_t chunk;
    __builtin_memcpy(&chunk, data, 4);
    crc = _mm_crc32_u32(static_cast<std::uint32_t>(crc), chunk);
    data += 4;
    len -= 4;
  }

  // Process remaining 2 bytes if present
  if (len >= 2) {
    std::uint16_t chunk;
    __builtin_memcpy(&chunk, data, 2);
    crc = _mm_crc32_u16(static_cast<std::uint32_t>(crc), chunk);
    data += 2;
    len -= 2;
  }

  // Process final byte if present
  if (len >= 1) {
    crc = _mm_crc32_u8(static_cast<std::uint32_t>(crc), *data);
  }

  // Return bitwise inverse (CRC convention)
  return static_cast<std::uint32_t>(~crc);
}

// ----------------------------------------------------------------------------
// CRC32C Hasher Class (std::hash compatible)
// ----------------------------------------------------------------------------

template <typename T>
struct Crc32cHash;

template <>
struct Crc32cHash<std::string_view> {
  auto operator()(std::string_view s) const noexcept -> std::size_t {
    return crc32c(reinterpret_cast<const std::uint8_t*>(s.data()), s.size());
  }
};

template <>
struct Crc32cHash<std::uint64_t> {
  auto operator()(std::uint64_t x) const noexcept -> std::size_t {
    // For a single integer, just use the instruction directly
    return static_cast<std::uint32_t>(~_mm_crc32_u64(~0ULL, x));
  }
};

template <>
struct Crc32cHash<std::uint32_t> {
  auto operator()(std::uint32_t x) const noexcept -> std::size_t {
    return static_cast<std::uint32_t>(~_mm_crc32_u32(~0U, x));
  }
};

template <>
struct Crc32cHash<std::int64_t> {
  auto operator()(std::int64_t x) const noexcept -> std::size_t {
    return Crc32cHash<std::uint64_t>{}(static_cast<std::uint64_t>(x));
  }
};

template <>
struct Crc32cHash<std::int32_t> {
  auto operator()(std::int32_t x) const noexcept -> std::size_t {
    return Crc32cHash<std::uint32_t>{}(static_cast<std::uint32_t>(x));
  }
};

// ----------------------------------------------------------------------------
// Combining CRC32C with Finalization
// ----------------------------------------------------------------------------
//
// Raw CRC32C has weak mixing for hash table use. For better distribution,
// we can apply a finalizer (like MurmurHash3's fmix32).
//
// This gives us CRC32C's speed with better hash table behavior.

inline auto crc32cMixed(const std::uint8_t* data, std::size_t len,
                        std::uint32_t seed = 0) noexcept -> std::uint32_t {
  return fmix32(crc32c(data, len, seed));
}

// ----------------------------------------------------------------------------
// 64-bit Extension: CRC32C + Mixing
// ----------------------------------------------------------------------------
//
// CRC32C only produces 32 bits. For a 64-bit hash, we can:
//   1. Compute CRC of first half
//   2. Compute CRC of second half with different seed
//   3. Combine them
//
// This is a simple approach; more sophisticated methods exist.

inline auto crc32c_64(const std::uint8_t* data, std::size_t len,
                      std::uint64_t seed = 0) noexcept -> std::uint64_t {
  // Split seed into two 32-bit seeds
  std::uint32_t seed_lo = static_cast<std::uint32_t>(seed);
  std::uint32_t seed_hi = static_cast<std::uint32_t>(seed >> 32);

  // Compute two CRCs with different starting points
  std::uint32_t crc_lo = crc32c(data, len, seed_lo);
  std::uint32_t crc_hi = crc32c(data, len, seed_hi ^ 0xDEADBEEF);

  // Combine into 64 bits
  return (static_cast<std::uint64_t>(crc_hi) << 32) | crc_lo;
}

}  // namespace tempura
