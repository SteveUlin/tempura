#pragma once

// ============================================================================
// Hash Functions: Theory and Practice
// ============================================================================
//
// A hash function maps data of arbitrary size to fixed-size values. While this
// sounds simple, designing a good hash function involves deep insights from
// number theory, information theory, and computer architecture.
//
// WHAT MAKES A HASH FUNCTION "GOOD"?
// ==================================
//
// 1. UNIFORMITY (Distribution)
//    The hash values should be spread evenly across the output range. If we
//    hash N random keys into M buckets, each bucket should receive roughly
//    N/M keys. Poor distribution causes clustering, which degrades hash table
//    performance from O(1) to O(n).
//
//    Mathematical framing: An ideal hash function approximates a random oracle
//    - for any input, the output appears uniformly distributed over the range.
//
// 2. AVALANCHE EFFECT
//    A single bit change in the input should flip approximately half the output
//    bits. This is formalized as: for all input bits i and output bits j,
//    changing input bit i should change output bit j with probability ~0.5.
//
//    Why this matters: Without avalanche, similar keys hash to similar values,
//    causing clustering. A good avalanche effect ensures that keys like "cat"
//    and "car" land in completely different buckets despite differing by one
//    character.
//
// 3. SPEED
//    Hash functions are called frequently (every lookup, insert, delete), so
//    they must be fast. Modern hash functions exploit CPU features like:
//    - Instruction-level parallelism (independent operations)
//    - Wide multipliers (64-bit × 64-bit → 128-bit)
//    - SIMD (processing multiple values simultaneously)
//
// CRYPTOGRAPHIC VS NON-CRYPTOGRAPHIC HASHES
// =========================================
//
// This library implements NON-CRYPTOGRAPHIC hash functions, optimized for
// speed in hash tables. Cryptographic hashes (SHA-256, BLAKE3) provide
// additional security guarantees but are much slower.
//
// Non-cryptographic hashes:
//   ✓ Fast (2-10 GB/s)
//   ✓ Good distribution
//   ✓ Good avalanche
//   ✗ NOT collision resistant (attackers can craft collisions)
//   ✗ NOT preimage resistant (can't hide input)
//
// Cryptographic hashes:
//   ✓ Collision resistant
//   ✓ Preimage resistant
//   ✓ Second preimage resistant
//   ✗ Slower (100 MB/s - 1 GB/s)
//
// SECURITY WARNING: Never use these functions for passwords, signatures,
// or any application requiring collision resistance. Use bcrypt/scrypt for
// passwords and SHA-256/BLAKE3 for integrity verification.
//
// KEY MIXING TECHNIQUES
// =====================
//
// Hash functions are built from simple operations that "mix" bits together.
// The key insight: multiplication and rotation together provide excellent
// mixing because:
//
//   - Multiplication diffuses high bits downward (through carries)
//   - Rotation prevents bit loss and spreads influence across positions
//
// Common primitives:
//
//   XOR (⊕): Combines bits without losing information (reversible)
//            x ⊕ 0 = x,  x ⊕ x = 0,  x ⊕ y = y ⊕ x
//
//   Rotation (ROL/ROR): Moves bits circularly, preventing loss
//            ROL(0b10110, 2) = 0b11010 (bits wrap around)
//
//   Multiplication: Spreads influence of each bit to many positions
//            Each input bit affects many output bits through carries
//            Works best with odd numbers (to be invertible mod 2^n)
//
// FINALIZATION
// ============
//
// After processing all input, hash functions apply a "finalizer" - extra
// mixing that ensures short inputs achieve full avalanche. Without this,
// short strings might not flip enough output bits.
//
// A classic finalizer pattern (from MurmurHash3):
//
//   h ^= h >> 16;
//   h *= 0x85ebca6b;
//   h ^= h >> 13;
//   h *= 0xc2b2ae35;
//   h ^= h >> 16;
//
// Each xor-shift spreads bit influence; each multiply completes the mixing.
// The magic constants were found by optimization to minimize avalanche bias.
//
// ============================================================================

#include <concepts>
#include <cstddef>
#include <cstdint>

namespace tempura {

// ----------------------------------------------------------------------------
// Hash Function Concept
// ----------------------------------------------------------------------------
//
// A Hasher is any callable that takes a key type and returns a size_t.
// This matches std::hash's interface, allowing drop-in compatibility.

template <typename H, typename K>
concept Hasher = requires(H h, K k) {
  { h(k) } -> std::convertible_to<std::size_t>;
};

// ----------------------------------------------------------------------------
// Utility: rotl (Rotate Left)
// ----------------------------------------------------------------------------
//
// Bit rotation is fundamental to hash mixing. Unlike shifts, no bits are lost:
//   rotl(0b1001, 1) = 0b0011  (the high bit wraps to the low position)
//
// Modern CPUs have dedicated rotation instructions (ROL/ROR), and good
// compilers recognize this pattern and emit them.

constexpr auto rotl32(std::uint32_t x, int r) -> std::uint32_t {
  return (x << r) | (x >> (32 - r));
}

constexpr auto rotl64(std::uint64_t x, int r) -> std::uint64_t {
  return (x << r) | (x >> (64 - r));
}

// ----------------------------------------------------------------------------
// Utility: Hash Combining
// ----------------------------------------------------------------------------
//
// Often we need to hash composite objects (structs, pairs). The naive approach
// of XORing component hashes is WRONG because:
//   hash(a, b) == hash(b, a)  (order independence)
//   hash(x, x) == 0           (self-cancellation)
//
// The boost::hash_combine pattern fixes this:
//   combined ^= hash(value) + constant + (combined << a) + (combined >> b)
//
// The magic constant is 2^n / φ (golden ratio reciprocal):
//   64-bit: 0x9e3779b97f4a7c15 = 2^64 / φ
//   32-bit: 0x9e3779b9 = 2^32 / φ
//
// Why this works:
//   - The addition ensures x,x doesn't cancel
//   - The shifts mix the existing state into the new hash
//   - The constant prevents simple patterns from aligning
//
// Shift values are tuned to the word size for proper bit diffusion.

// 64-bit version (primary, for std::size_t on 64-bit systems)
template <typename T, Hasher<T> H>
constexpr void hashCombine(std::size_t& seed, const T& value, H hasher = {}) {
  seed ^= hasher(value) + 0x9e3779b97f4a7c15ULL + (seed << 12) + (seed >> 4);
}

// Convenience overload using std::hash
template <typename T>
constexpr void hashCombine(std::size_t& seed, const T& value) {
  seed ^= std::hash<T>{}(value) + 0x9e3779b97f4a7c15ULL + (seed << 12) + (seed >> 4);
}

// 32-bit explicit versions for when you need a 32-bit hash
template <typename T, Hasher<T> H>
constexpr void hashCombine32(std::uint32_t& seed, const T& value, H hasher = {}) {
  seed ^= static_cast<std::uint32_t>(hasher(value)) + 0x9e3779b9U + (seed << 6) + (seed >> 2);
}

template <typename T>
constexpr void hashCombine32(std::uint32_t& seed, const T& value) {
  seed ^= static_cast<std::uint32_t>(std::hash<T>{}(value)) + 0x9e3779b9U + (seed << 6) + (seed >> 2);
}

// ----------------------------------------------------------------------------
// Utility: Reading Bytes
// ----------------------------------------------------------------------------
//
// Hash functions process input as bytes, but the input might not be aligned
// or might have different endianness. These helpers handle that portably.
//
// Note: In practice, most inputs are aligned and little-endian (x86/ARM),
// so compilers optimize memcpy to simple loads. The explicit byte reads
// ensure correctness on all platforms.

constexpr auto read32LE(const std::uint8_t* p) -> std::uint32_t {
  // Little-endian: least significant byte first
  return static_cast<std::uint32_t>(p[0]) |
         (static_cast<std::uint32_t>(p[1]) << 8) |
         (static_cast<std::uint32_t>(p[2]) << 16) |
         (static_cast<std::uint32_t>(p[3]) << 24);
}

constexpr auto read64LE(const std::uint8_t* p) -> std::uint64_t {
  return static_cast<std::uint64_t>(p[0]) |
         (static_cast<std::uint64_t>(p[1]) << 8) |
         (static_cast<std::uint64_t>(p[2]) << 16) |
         (static_cast<std::uint64_t>(p[3]) << 24) |
         (static_cast<std::uint64_t>(p[4]) << 32) |
         (static_cast<std::uint64_t>(p[5]) << 40) |
         (static_cast<std::uint64_t>(p[6]) << 48) |
         (static_cast<std::uint64_t>(p[7]) << 56);
}

// ----------------------------------------------------------------------------
// Finalizers: fmix32 / fmix64
// ----------------------------------------------------------------------------
//
// These are the classic MurmurHash3 finalizers - carefully tuned mixing
// functions that achieve near-perfect avalanche (each input bit affects
// each output bit with ~50% probability).
//
// The pattern is: xor-shift spreads bits, multiply completes mixing.
// The magic constants and shift amounts were found by simulated annealing
// to minimize avalanche bias to within 0.25%.
//
// These are useful beyond MurmurHash - any time you need to mix a hash
// value for better distribution (e.g., after CRC32C), these are the
// go-to finishers.

constexpr auto fmix32(std::uint32_t h) noexcept -> std::uint32_t {
  h ^= h >> 16;
  h *= 0x85ebca6b;
  h ^= h >> 13;
  h *= 0xc2b2ae35;
  h ^= h >> 16;
  return h;
}

constexpr auto fmix64(std::uint64_t h) noexcept -> std::uint64_t {
  h ^= h >> 33;
  h *= 0xff51afd7ed558ccdULL;
  h ^= h >> 33;
  h *= 0xc4ceb9fe1a85ec53ULL;
  h ^= h >> 33;
  return h;
}

}  // namespace tempura
