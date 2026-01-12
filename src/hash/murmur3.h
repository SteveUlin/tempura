#pragma once

// ============================================================================
// MurmurHash3: A Fast, High-Quality Non-Cryptographic Hash
// ============================================================================
//
// MurmurHash3 was designed by Austin Appleby in 2008 and finalized in 2011.
// The name comes from the two core operations: MUltiply and Rotate (murmur).
// It's one of the most widely-used hash functions, powering systems from
// Elasticsearch to Cassandra to libstdc++.
//
// DESIGN PHILOSOPHY
// =================
//
// MurmurHash3 processes input in blocks, with each block undergoing a
// "mixing" step that spreads bit influence. The key insight is that
// multiplication followed by rotation provides excellent avalanche:
//
//   k *= c1;          // Multiplication spreads bits via carries
//   k = rotl(k, r1);  // Rotation prevents bit loss at boundaries
//   k *= c2;          // Another multiply completes the mixing
//
// This pattern (multiply-rotate-multiply) is the heart of MurmurHash.
//
// THE ALGORITHM (32-bit version)
// ==============================
//
// 1. INITIALIZATION
//    Start with a seed value (allows different hash spaces)
//
// 2. BODY: Process 4-byte blocks
//    For each 4-byte block:
//      k = getblock(i)      // Read 4 bytes as uint32
//      k *= c1              // c1 = 0xcc9e2d51
//      k = rotl(k, 15)
//      k *= c2              // c2 = 0x1b873593
//      h ^= k
//      h = rotl(h, 13)
//      h = h * 5 + 0xe6546b64
//
// 3. TAIL: Handle remaining 1-3 bytes
//    Process leftover bytes that don't fill a complete block
//
// 4. FINALIZATION
//    h ^= length           // Include length in hash
//    h = fmix32(h)         // Final mixing to complete avalanche
//
// WHY THESE SPECIFIC CONSTANTS?
// =============================
//
// The constants (c1, c2, rotations, multipliers) were found through
// exhaustive testing and simulated annealing optimization. The goals:
//
//   - Maximize avalanche (each input bit affects ~50% of output bits)
//   - Minimize bias (output bits are independently distributed)
//   - Pass the SMHasher test suite (comprehensive hash quality tests)
//
// The magic constants 0xcc9e2d51 and 0x1b873593 are odd (invertible),
// have good bit patterns, and were empirically selected for avalanche.
//
// THE FINALIZER
// =============
//
// The finalizer is crucial for short inputs. Without it, a single-byte
// input wouldn't fully avalanche to all 32 output bits. The finalizer:
//
//   h ^= h >> 16;
//   h *= 0x85ebca6b;
//   h ^= h >> 13;
//   h *= 0xc2b2ae35;
//   h ^= h >> 16;
//
// This pattern (xor-shift + multiply, repeated) achieves full avalanche
// with minimal operations. The constants were found by simulated annealing
// to minimize avalanche bias to within 0.25%.
//
// VARIANTS
// ========
//
// MurmurHash3 comes in three variants:
//   - MurmurHash3_x86_32:  32-bit output, optimized for 32-bit CPUs
//   - MurmurHash3_x86_128: 128-bit output, optimized for 32-bit CPUs
//   - MurmurHash3_x64_128: 128-bit output, optimized for 64-bit CPUs
//
// This file implements the 32-bit and 64-bit→32-bit variants.
//
// COLLISION RESISTANCE
// ====================
//
// MurmurHash3 is NOT cryptographically secure. In 2012, researchers showed
// that MurmurHash (even with random seeds) is vulnerable to differential
// cryptanalysis. An attacker can craft inputs that collide, enabling
// hash-flooding DoS attacks.
//
// For untrusted input, use SipHash (designed to resist hash-flooding).
//
// ============================================================================

#include <cstddef>
#include <cstdint>
#include <string_view>

#include "hash/hash.h"

namespace tempura {

// ----------------------------------------------------------------------------
// MurmurHash3 Constants
// ----------------------------------------------------------------------------
//
// These constants were found through extensive testing. They're not arbitrary:
//   - c1, c2 are odd (ensuring they're invertible mod 2^32)
//   - Their bit patterns provide good mixing properties
//   - The rotation amounts (15, 13) were optimized empirically

struct Murmur3Constants32 {
  static constexpr std::uint32_t kC1 = 0xcc9e2d51;
  static constexpr std::uint32_t kC2 = 0x1b873593;
  static constexpr int kR1 = 15;
  static constexpr int kR2 = 13;
  static constexpr std::uint32_t kM = 5;
  static constexpr std::uint32_t kN = 0xe6546b64;
};

// ----------------------------------------------------------------------------
// MurmurHash3_x86_32
// ----------------------------------------------------------------------------
//
// The 32-bit variant, processing 4 bytes at a time.
// This is the most commonly used MurmurHash3 variant.

constexpr auto murmurHash3_32(const std::uint8_t* data, std::size_t len,
                              std::uint32_t seed = 0) noexcept
    -> std::uint32_t {
  using C = Murmur3Constants32;

  const std::size_t nblocks = len / 4;
  std::uint32_t h = seed;

  // --------------------------------------------------------------------------
  // Body: Process 4-byte blocks
  // --------------------------------------------------------------------------
  // Each block undergoes the "murmur" mixing:
  //   1. Multiply by c1 (spreads bits via carries)
  //   2. Rotate left by 15 (prevents bit loss, spreads influence)
  //   3. Multiply by c2 (completes mixing)
  //   4. XOR into accumulator
  //   5. Rotate accumulator and add linear feedback

  for (std::size_t i = 0; i < nblocks; ++i) {
    std::uint32_t k = read32LE(data + i * 4);

    k *= C::kC1;
    k = rotl32(k, C::kR1);
    k *= C::kC2;

    h ^= k;
    h = rotl32(h, C::kR2);
    h = h * C::kM + C::kN;
  }

  // --------------------------------------------------------------------------
  // Tail: Handle remaining 1-3 bytes
  // --------------------------------------------------------------------------
  // We can't read a full 4-byte block, so we manually assemble the tail.
  // The fall-through cases accumulate bytes into k1.

  const std::uint8_t* tail = data + nblocks * 4;
  std::uint32_t k1 = 0;

  // Process remaining bytes (fall-through is intentional)
  switch (len & 3) {
    case 3:
      k1 ^= static_cast<std::uint32_t>(tail[2]) << 16;
      [[fallthrough]];
    case 2:
      k1 ^= static_cast<std::uint32_t>(tail[1]) << 8;
      [[fallthrough]];
    case 1:
      k1 ^= tail[0];
      k1 *= C::kC1;
      k1 = rotl32(k1, C::kR1);
      k1 *= C::kC2;
      h ^= k1;
      break;
    default:
      break;
  }

  // --------------------------------------------------------------------------
  // Finalization
  // --------------------------------------------------------------------------
  // XOR in the length (so strings of different lengths hash differently)
  // then apply the finalizer for full avalanche.

  h ^= static_cast<std::uint32_t>(len);
  h = fmix32(h);

  return h;
}

// ----------------------------------------------------------------------------
// MurmurHash3 64-bit Variant
// ----------------------------------------------------------------------------
//
// This is a simplified 64-bit hash derived from the x64_128 variant.
// It processes 8 bytes at a time for better performance on 64-bit CPUs.

constexpr auto murmurHash3_64(const std::uint8_t* data, std::size_t len,
                              std::uint64_t seed = 0) noexcept
    -> std::uint64_t {
  // Constants for 64-bit mixing
  constexpr std::uint64_t kC1 = 0x87c37b91114253d5ULL;
  constexpr std::uint64_t kC2 = 0x4cf5ad432745937fULL;

  const std::size_t nblocks = len / 8;
  std::uint64_t h = seed;

  // Process 8-byte blocks
  for (std::size_t i = 0; i < nblocks; ++i) {
    std::uint64_t k = read64LE(data + i * 8);

    k *= kC1;
    k = rotl64(k, 31);
    k *= kC2;

    h ^= k;
    h = rotl64(h, 27);
    h = h * 5 + 0x52dce729;
  }

  // Handle remaining bytes
  const std::uint8_t* tail = data + nblocks * 8;
  std::uint64_t k1 = 0;

  switch (len & 7) {
    case 7:
      k1 ^= static_cast<std::uint64_t>(tail[6]) << 48;
      [[fallthrough]];
    case 6:
      k1 ^= static_cast<std::uint64_t>(tail[5]) << 40;
      [[fallthrough]];
    case 5:
      k1 ^= static_cast<std::uint64_t>(tail[4]) << 32;
      [[fallthrough]];
    case 4:
      k1 ^= static_cast<std::uint64_t>(tail[3]) << 24;
      [[fallthrough]];
    case 3:
      k1 ^= static_cast<std::uint64_t>(tail[2]) << 16;
      [[fallthrough]];
    case 2:
      k1 ^= static_cast<std::uint64_t>(tail[1]) << 8;
      [[fallthrough]];
    case 1:
      k1 ^= tail[0];
      k1 *= kC1;
      k1 = rotl64(k1, 31);
      k1 *= kC2;
      h ^= k1;
      break;
    default:
      break;
  }

  // Finalization
  h ^= len;
  h = fmix64(h);

  return h;
}

// ----------------------------------------------------------------------------
// Hasher Classes (std::hash compatible)
// ----------------------------------------------------------------------------

template <typename T>
struct MurmurHash3_32;

template <typename T>
struct MurmurHash3_64;

template <>
struct MurmurHash3_32<std::string_view> {
  constexpr auto operator()(std::string_view s) const noexcept -> std::size_t {
    return murmurHash3_32(reinterpret_cast<const std::uint8_t*>(s.data()),
                          s.size());
  }
};

template <>
struct MurmurHash3_64<std::string_view> {
  constexpr auto operator()(std::string_view s) const noexcept -> std::size_t {
    return murmurHash3_64(reinterpret_cast<const std::uint8_t*>(s.data()),
                          s.size());
  }
};

// Integer specializations
template <>
struct MurmurHash3_32<std::uint32_t> {
  constexpr auto operator()(std::uint32_t x) const noexcept -> std::size_t {
    const std::uint8_t bytes[4] = {
        static_cast<std::uint8_t>(x),
        static_cast<std::uint8_t>(x >> 8),
        static_cast<std::uint8_t>(x >> 16),
        static_cast<std::uint8_t>(x >> 24),
    };
    return murmurHash3_32(bytes, 4);
  }
};

template <>
struct MurmurHash3_64<std::uint64_t> {
  constexpr auto operator()(std::uint64_t x) const noexcept -> std::size_t {
    const std::uint8_t bytes[8] = {
        static_cast<std::uint8_t>(x),
        static_cast<std::uint8_t>(x >> 8),
        static_cast<std::uint8_t>(x >> 16),
        static_cast<std::uint8_t>(x >> 24),
        static_cast<std::uint8_t>(x >> 32),
        static_cast<std::uint8_t>(x >> 40),
        static_cast<std::uint8_t>(x >> 48),
        static_cast<std::uint8_t>(x >> 56),
    };
    return murmurHash3_64(bytes, 8);
  }
};

template <>
struct MurmurHash3_32<std::int32_t> {
  constexpr auto operator()(std::int32_t x) const noexcept -> std::size_t {
    return MurmurHash3_32<std::uint32_t>{}(static_cast<std::uint32_t>(x));
  }
};

template <>
struct MurmurHash3_64<std::int64_t> {
  constexpr auto operator()(std::int64_t x) const noexcept -> std::size_t {
    return MurmurHash3_64<std::uint64_t>{}(static_cast<std::uint64_t>(x));
  }
};

// Default MurmurHash3: use 64-bit on 64-bit systems
template <typename T>
struct MurmurHash3 : MurmurHash3_64<T> {};

}  // namespace tempura
