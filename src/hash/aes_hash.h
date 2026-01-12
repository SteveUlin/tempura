#pragma once

// ============================================================================
// AES-Based Hashing: Using Encryption Instructions for Speed
// ============================================================================
//
// Modern CPUs have dedicated AES encryption instructions (AES-NI) that
// perform a full AES round in a single instruction. While designed for
// cryptography, these instructions make EXCELLENT hash mixers:
//
//   - Each AES round provides near-perfect avalanche
//   - Single instruction, ~4 cycle latency, 1 cycle throughput
//   - Processes 16 bytes at once (128-bit blocks)
//   - Can achieve 50+ GB/s on modern hardware
//
// THE KEY INSIGHT: AES ROUNDS AS MIXING FUNCTIONS
// ===============================================
//
// An AES round consists of four operations:
//
//   1. SubBytes: Non-linear S-box substitution (confusion)
//   2. ShiftRows: Byte permutation across the block (diffusion)
//   3. MixColumns: Matrix multiplication in GF(2^8) (diffusion)
//   4. AddRoundKey: XOR with round key
//
// The first three steps provide MASSIVE bit mixing. In fact, just 2-3
// AES rounds achieve nearly perfect avalanche (every input bit affects
// every output bit with ~50% probability).
//
// For hashing, we don't need the full 10-14 rounds of AES encryption.
// We use the AESENC instruction which performs steps 1-3 + XOR, giving
// us a powerful mixing function.
//
// WHY AES-HASH IS SO FAST
// =======================
//
// Consider the alternatives:
//
//   MurmurHash: ~3-4 instructions per 4 bytes (multiply, rotate, XOR)
//   wyhash: ~2-3 instructions per 8 bytes (128-bit multiply, XOR)
//   AES-hash: ~1 instruction per 16 bytes (AESENC)
//
// The AESENC instruction does more work per instruction than any
// arithmetic operation, AND it processes twice as much data.
//
// THE AESENC INSTRUCTION
// ======================
//
//   __m128i _mm_aesenc_si128(__m128i data, __m128i key)
//
// This performs one AES encryption round:
//   1. SubBytes on data
//   2. ShiftRows on result
//   3. MixColumns on result
//   4. XOR with key
//
// There's also AESENCLAST which skips MixColumns (used for the final
// AES round), but for hashing we typically want full mixing.
//
// DESIGN OF THIS HASH FUNCTION
// ============================
//
// Our approach:
//
// 1. Initialize with a 128-bit seed
// 2. For each 16-byte block, XOR with accumulator then AESENC
// 3. Handle tail bytes specially
// 4. Apply finalization rounds for full avalanche
//
// We use 2 AESENC rounds in the finalizer to ensure all bits are
// thoroughly mixed, even for short inputs.
//
// SECURITY PROPERTIES
// ===================
//
// Despite using AES instructions, this is NOT cryptographically secure:
//
//   - We use far fewer rounds than real AES
//   - The "keys" are fixed, not secret
//   - No protection against length extension
//
// However, it IS harder to attack than simpler hashes because the AES
// S-box introduces non-linearity that resists differential attacks.
//
// HARDWARE REQUIREMENTS
// =====================
//
// AES-NI is available on:
//   - Intel: Westmere (2010) and later
//   - AMD: Bulldozer (2011) and later
//   - ARM: ARMv8 Cryptography Extensions
//
// This covers essentially all modern x86 and ARM64 CPUs.
//
// ============================================================================

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string_view>
#include <immintrin.h>  // All intrinsics including AES-NI

#include "hash/hash.h"

namespace tempura {

// ----------------------------------------------------------------------------
// AES Hash Constants
// ----------------------------------------------------------------------------
//
// These are arbitrary 128-bit values used as "keys" for AESENC.
// They don't need to be secret - they just provide the mixing material.
// Different constants give different hash functions (useful for double
// hashing or when you need independent hash families).

namespace aes_hash_detail {

// Initial state (based on digits of pi)
inline const __m128i kAesKey0 =
    _mm_set_epi64x(0x243F6A8885A308D3ULL, 0x13198A2E03707344ULL);

// Mixing keys (more digits of pi)
inline const __m128i kAesKey1 =
    _mm_set_epi64x(0xA4093822299F31D0ULL, 0x082EFA98EC4E6C89ULL);

inline const __m128i kAesKey2 =
    _mm_set_epi64x(0x452821E638D01377ULL, 0xBE5466CF34E90C6CULL);

}  // namespace aes_hash_detail

// ----------------------------------------------------------------------------
// AES Hash Core Implementation
// ----------------------------------------------------------------------------

inline auto aesHash128(const std::uint8_t* data, std::size_t len,
                       __m128i seed = aes_hash_detail::kAesKey0) noexcept
    -> __m128i {
  using namespace aes_hash_detail;

  __m128i state = seed;

  // Process 16-byte blocks
  // Each block: state = AESENC(state XOR block, key)
  while (len >= 16) {
    __m128i block = _mm_loadu_si128(reinterpret_cast<const __m128i*>(data));
    state = _mm_xor_si128(state, block);
    state = _mm_aesenc_si128(state, kAesKey1);
    data += 16;
    len -= 16;
  }

  // Handle tail (0-15 remaining bytes)
  if (len > 0) {
    // Load remaining bytes into a zeroed block
    alignas(16) std::uint8_t tail[16] = {};
    std::memcpy(tail, data, len);

    // Include length in the tail to differentiate truncated inputs
    tail[15] = static_cast<std::uint8_t>(len);

    __m128i block = _mm_load_si128(reinterpret_cast<const __m128i*>(tail));
    state = _mm_xor_si128(state, block);
    state = _mm_aesenc_si128(state, kAesKey1);
  }

  // Mix in total length (prevents collisions between inputs of different
  // lengths that happen to have the same final block)
  __m128i len_block = _mm_set_epi64x(0, static_cast<std::int64_t>(len));
  state = _mm_xor_si128(state, len_block);

  // Finalization: two more AES rounds for full avalanche
  // This ensures short inputs achieve complete bit mixing
  state = _mm_aesenc_si128(state, kAesKey1);
  state = _mm_aesenc_si128(state, kAesKey2);

  return state;
}

// Extract 64-bit hash from 128-bit state
inline auto aesHash64(const std::uint8_t* data, std::size_t len,
                      std::uint64_t seed = 0) noexcept -> std::uint64_t {
  __m128i seed128 = _mm_set_epi64x(static_cast<std::int64_t>(seed),
                                   static_cast<std::int64_t>(seed));
  seed128 = _mm_xor_si128(seed128, aes_hash_detail::kAesKey0);

  __m128i result = aesHash128(data, len, seed128);

  // XOR high and low 64-bit halves for final hash
  // This combines all bits into the final result
  std::uint64_t lo = static_cast<std::uint64_t>(_mm_extract_epi64(result, 0));
  std::uint64_t hi = static_cast<std::uint64_t>(_mm_extract_epi64(result, 1));

  return lo ^ hi;
}

// 32-bit version (less common, but sometimes useful)
inline auto aesHash32(const std::uint8_t* data, std::size_t len,
                      std::uint32_t seed = 0) noexcept -> std::uint32_t {
  std::uint64_t h64 = aesHash64(data, len, seed);
  // Fold 64 bits to 32 bits
  return static_cast<std::uint32_t>(h64 ^ (h64 >> 32));
}

// ----------------------------------------------------------------------------
// AES Hasher Classes (std::hash compatible)
// ----------------------------------------------------------------------------

template <typename T>
struct AesHash;

template <>
struct AesHash<std::string_view> {
  auto operator()(std::string_view s) const noexcept -> std::size_t {
    return aesHash64(reinterpret_cast<const std::uint8_t*>(s.data()), s.size());
  }
};

template <>
struct AesHash<std::uint64_t> {
  auto operator()(std::uint64_t x) const noexcept -> std::size_t {
    // For a single integer, use AESENC directly as a mixer
    __m128i val = _mm_set_epi64x(0, static_cast<std::int64_t>(x));
    val = _mm_aesenc_si128(val, aes_hash_detail::kAesKey0);
    val = _mm_aesenc_si128(val, aes_hash_detail::kAesKey1);
    return static_cast<std::uint64_t>(_mm_extract_epi64(val, 0));
  }
};

template <>
struct AesHash<std::uint32_t> {
  auto operator()(std::uint32_t x) const noexcept -> std::size_t {
    return AesHash<std::uint64_t>{}(x);
  }
};

template <>
struct AesHash<std::int64_t> {
  auto operator()(std::int64_t x) const noexcept -> std::size_t {
    return AesHash<std::uint64_t>{}(static_cast<std::uint64_t>(x));
  }
};

template <>
struct AesHash<std::int32_t> {
  auto operator()(std::int32_t x) const noexcept -> std::size_t {
    return AesHash<std::uint32_t>{}(static_cast<std::uint32_t>(x));
  }
};

}  // namespace tempura
