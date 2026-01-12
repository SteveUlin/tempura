#pragma once

// ============================================================================
// FNV-1a: Fowler-Noll-Vo Hash Function
// ============================================================================
//
// FNV is one of the simplest practical hash functions, created in 1991 by
// Glenn Fowler, Landon Curt Noll, and Phong Vo. Despite its simplicity, it
// provides reasonable distribution for hash tables.
//
// THE ALGORITHM
// =============
//
// FNV-1a processes input byte-by-byte:
//
//   hash = FNV_OFFSET_BASIS
//   for each byte:
//       hash = hash XOR byte
//       hash = hash * FNV_PRIME
//   return hash
//
// That's it. The entire algorithm is two operations per byte.
//
// FNV-1 vs FNV-1a
// ===============
//
// The original FNV-1 does multiply-then-XOR:
//   hash = (hash * prime) XOR byte
//
// FNV-1a reverses this to XOR-then-multiply:
//   hash = (hash XOR byte) * prime
//
// FNV-1a has slightly better avalanche properties because the XOR happens
// before multiplication. In FNV-1, if the byte is 0, the XOR does nothing
// and the multiply just scales the existing hash. In FNV-1a, even a zero
// byte gets multiplied into the mix.
//
// THE MAGIC CONSTANTS
// ===================
//
// The algorithm requires two carefully chosen constants:
//
// FNV_OFFSET_BASIS: The initial hash value. Must be non-zero to avoid
//                   the degenerate case where hash=0, byte=0 stays 0.
//
// FNV_PRIME: The multiplication constant. Must satisfy:
//   - Be prime (provides good distribution)
//   - Be close to a power of 2 (fast on CPUs that optimize 2^n mul)
//   - Have a specific pattern: p ≡ 1 (mod 8) and p ≡ 5 (mod 12)
//
// For 32-bit FNV:
//   FNV_OFFSET_BASIS = 2166136261 (0x811c9dc5)
//   FNV_PRIME = 16777619 (0x01000193)
//
// For 64-bit FNV:
//   FNV_OFFSET_BASIS = 14695981039346656037 (0xcbf29ce484222325)
//   FNV_PRIME = 1099511628211 (0x00000100000001b3)
//
// WHY DOES IT WORK?
// =================
//
// The genius of FNV lies in how multiplication spreads bit influence:
//
// Consider multiplying by the 64-bit prime 0x00000100000001b3:
//   In binary: 0000000100000000000000000001101100110011
//
// This is approximately 2^40 + 435. When we multiply:
//   result = input * (2^40 + 435)
//          = input * 2^40 + input * 435
//          = (input << 40) + input * 435
//
// The shift-and-add nature spreads each input bit to multiple output
// positions. After several iterations, every input byte influences
// essentially every output bit.
//
// STRENGTHS AND WEAKNESSES
// ========================
//
// ✓ Extremely simple (easy to implement correctly)
// ✓ No lookup tables (good for embedded/constexpr)
// ✓ Streaming (can hash incrementally)
// ✓ Reasonable distribution for most data
//
// ✗ Slower than modern hash functions (one byte at a time)
// ✗ Weaker avalanche than MurmurHash/xxHash
// ✗ Vulnerable to crafted collisions (hash-flooding DoS)
// ✗ Not suitable for cryptographic use
//
// USE CASES
// =========
//
// FNV is ideal when you need:
//   - A simple, portable hash with no dependencies
//   - Constexpr evaluation
//   - Good-enough distribution for non-adversarial inputs
//
// Avoid FNV when:
//   - Processing untrusted input (use SipHash)
//   - Performance-critical bulk hashing (use xxHash/wyhash)
//
// ============================================================================

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string_view>

#include "hash/hash.h"

namespace tempura {

// ----------------------------------------------------------------------------
// FNV-1a Constants
// ----------------------------------------------------------------------------
//
// These specific values were discovered through analysis of prime number
// properties and empirical testing. The relationship between offset and
// prime is: offset = FNV-0("chongo <Landon Curt Noll>'s cute cat")
// where FNV-0 is FNV with offset=0.

struct Fnv32Constants {
  static constexpr std::uint32_t kOffsetBasis = 0x811c9dc5;
  static constexpr std::uint32_t kPrime = 0x01000193;
};

struct Fnv64Constants {
  static constexpr std::uint64_t kOffsetBasis = 0xcbf29ce484222325ULL;
  static constexpr std::uint64_t kPrime = 0x00000100000001b3ULL;
};

// ----------------------------------------------------------------------------
// FNV-1a Core Implementation
// ----------------------------------------------------------------------------

// Hash raw bytes
constexpr auto fnv1a32(const std::uint8_t* data, std::size_t len) noexcept
    -> std::uint32_t {
  std::uint32_t hash = Fnv32Constants::kOffsetBasis;

  for (std::size_t i = 0; i < len; ++i) {
    // XOR the byte into the hash
    hash ^= data[i];
    // Multiply by the prime to spread bit influence
    hash *= Fnv32Constants::kPrime;
  }

  return hash;
}

constexpr auto fnv1a64(const std::uint8_t* data, std::size_t len) noexcept
    -> std::uint64_t {
  std::uint64_t hash = Fnv64Constants::kOffsetBasis;

  for (std::size_t i = 0; i < len; ++i) {
    hash ^= data[i];
    hash *= Fnv64Constants::kPrime;
  }

  return hash;
}

// ----------------------------------------------------------------------------
// FNV-1a Hasher Classes (std::hash compatible)
// ----------------------------------------------------------------------------
//
// These wrapper classes provide a consistent interface matching std::hash,
// allowing them to be used as drop-in replacements in hash containers.

template <typename T>
struct Fnv1aHash32;

template <typename T>
struct Fnv1aHash64;

// String view specialization
template <>
struct Fnv1aHash32<std::string_view> {
  constexpr auto operator()(std::string_view s) const noexcept -> std::size_t {
    return fnv1a32(reinterpret_cast<const std::uint8_t*>(s.data()), s.size());
  }
};

template <>
struct Fnv1aHash64<std::string_view> {
  constexpr auto operator()(std::string_view s) const noexcept -> std::size_t {
    return fnv1a64(reinterpret_cast<const std::uint8_t*>(s.data()), s.size());
  }
};

// Integer specializations: hash the bytes of the integer
// Note: This produces different results on big-endian vs little-endian systems
// For portable hashes, serialize to a fixed byte order first.

template <>
struct Fnv1aHash32<std::uint32_t> {
  constexpr auto operator()(std::uint32_t x) const noexcept -> std::size_t {
    // Treat the integer as 4 bytes
    const std::uint8_t bytes[4] = {
        static_cast<std::uint8_t>(x),
        static_cast<std::uint8_t>(x >> 8),
        static_cast<std::uint8_t>(x >> 16),
        static_cast<std::uint8_t>(x >> 24),
    };
    return fnv1a32(bytes, 4);
  }
};

template <>
struct Fnv1aHash64<std::uint64_t> {
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
    return fnv1a64(bytes, 8);
  }
};

// Signed integer versions (cast to unsigned to avoid sign issues)
template <>
struct Fnv1aHash32<std::int32_t> {
  constexpr auto operator()(std::int32_t x) const noexcept -> std::size_t {
    return Fnv1aHash32<std::uint32_t>{}(static_cast<std::uint32_t>(x));
  }
};

template <>
struct Fnv1aHash64<std::int64_t> {
  constexpr auto operator()(std::int64_t x) const noexcept -> std::size_t {
    return Fnv1aHash64<std::uint64_t>{}(static_cast<std::uint64_t>(x));
  }
};

// Default FNV-1a hash: use 64-bit on 64-bit systems
template <typename T>
struct Fnv1aHash : Fnv1aHash64<T> {};

}  // namespace tempura
