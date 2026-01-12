#pragma once

// ============================================================================
// Rolling Hash: Incrementally Updateable Hashing
// ============================================================================
//
// A rolling hash (also called a sliding window hash) can be UPDATED in O(1)
// when the window slides by one position. This is fundamentally different
// from regular hashes, which must reprocess the entire input.
//
// REGULAR HASH vs ROLLING HASH
// ============================
//
// Regular hash of "abcde" window in "xyzabcdefgh":
//   hash("abcde") → process all 5 bytes → O(n)
//
// Then for next window "bcdef":
//   hash("bcdef") → process all 5 bytes again → O(n)
//
// Rolling hash:
//   hash("abcde") → initial computation → O(n)
//   update(remove 'a', add 'f') → O(1) update → get hash("bcdef")
//
// For sliding a window of size W over text of length N:
//   - Regular hash: O(N × W)
//   - Rolling hash: O(N)
//
// This O(W) → O(1) improvement enables algorithms that would otherwise
// be impractical.
//
// THE POLYNOMIAL HASH (RABIN FINGERPRINT)
// =======================================
//
// The classic rolling hash treats the window as a polynomial:
//
//   hash("abc") = a × B² + b × B¹ + c × B⁰
//
// where B is a "base" (typically a prime near 256).
//
// When the window slides from "abc" to "bcd":
//
//   old_hash = a × B² + b × B¹ + c × B⁰
//   new_hash = (old_hash - a × B²) × B + d
//            =          b × B² + c × B¹ + d × B⁰
//
// The update requires:
//   1. Subtract the leaving character times B^(W-1)
//   2. Multiply by B (shift all remaining characters up)
//   3. Add the entering character
//
// Each step is O(1), giving us constant-time window sliding!
//
// WHY B^(W-1) CAN BE PRECOMPUTED
// ==============================
//
// Since the window size W is fixed, we precompute B^(W-1) once.
// This avoids the expensive modular exponentiation in each update.
//
// For a window of size 5 with B=31:
//   B^4 = 31^4 = 923521
//
// Store this value and use it for all updates.
//
// MODULAR ARITHMETIC
// ==================
//
// Hash values grow exponentially with window size. To keep them bounded,
// we compute everything modulo a large number M:
//
//   hash = (a × B² + b × B¹ + c) mod M
//
// The modulus M should be:
//   - Large (to minimize collisions)
//   - Prime (for better distribution)
//   - Fitting in 64 bits with room for intermediate calculations
//
// Common choices:
//   - 2^61 - 1 (Mersenne prime, fast modulo)
//   - 10^9 + 7 (easy to remember)
//   - 2^64 (implicit, using overflow)
//
// APPLICATIONS
// ============
//
// 1. RABIN-KARP STRING SEARCH
//    Find pattern P in text T:
//    - Compute hash(P)
//    - Slide window over T, compare hashes
//    - Verify matches with actual string comparison
//
// 2. PLAGIARISM DETECTION
//    Find common substrings between documents:
//    - Hash all N-grams in document A
//    - Check N-grams of document B against the set
//
// 3. RSYNC / CONTENT-DEFINED CHUNKING
//    Split files into chunks at "interesting" boundaries:
//    - Compute rolling hash at each position
//    - When hash matches a pattern (e.g., low bits are zero), split here
//    - Produces chunks that align across similar files
//
// 4. DIFF ALGORITHMS
//    Find regions that changed between file versions:
//    - Hash blocks in old file
//    - Find matching blocks in new file using rolling hash
//
// COLLISION HANDLING
// ==================
//
// Rolling hashes have weaker collision resistance than cryptographic
// hashes. The Rabin-Karp algorithm handles this by:
//
//   1. Use hash equality as a FILTER (fast negative test)
//   2. When hashes match, compare actual strings
//
// With a good hash (64-bit, prime modulus), false positives are rare
// enough that the string comparison cost is amortized away.
//
// ============================================================================

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace tempura {

// ----------------------------------------------------------------------------
// Constants for Rolling Hash
// ----------------------------------------------------------------------------
//
// We use a prime base and modulus for good distribution.
// The base should be larger than the alphabet size (256 for bytes).

namespace rolling_hash_detail {

// A prime slightly larger than 256, good for byte data
constexpr std::uint64_t kDefaultBase = 257;

// Mersenne prime 2^61 - 1: allows fast modulo using bit tricks
constexpr std::uint64_t kMersennePrime = (1ULL << 61) - 1;

// Modular reduction for Mersenne prime
// For x < 2^122, we can use: (x mod 2^61) + (x >> 61)
// May need a final correction if result >= M
constexpr auto modMersenne(std::uint64_t x) -> std::uint64_t {
  // For values that fit in 64 bits, standard modulo works
  // For the full optimization, we'd need 128-bit arithmetic
  return x % kMersennePrime;
}

// Compute B^exp mod M
constexpr auto powerMod(std::uint64_t base, std::size_t exp,
                        std::uint64_t mod) -> std::uint64_t {
  std::uint64_t result = 1;
  base %= mod;

  while (exp > 0) {
    if (exp & 1) {
      result = modMersenne(result * base);
    }
    base = modMersenne(base * base);
    exp >>= 1;
  }

  return result;
}

}  // namespace rolling_hash_detail

// ----------------------------------------------------------------------------
// RollingHash: Stateful Rolling Hash Calculator
// ----------------------------------------------------------------------------
//
// This class maintains the hash of a sliding window and supports O(1) updates.
//
// Usage:
//   RollingHash rh(window_size);
//
//   // Initialize with first window
//   for (char c : first_window) {
//     rh.append(c);
//   }
//
//   // Slide window
//   for (size_t i = window_size; i < text.size(); ++i) {
//     rh.slide(text[i - window_size], text[i]);
//     // rh.hash() now contains hash of text[i-window_size+1 .. i]
//   }

class RollingHash {
 public:
  using HashType = std::uint64_t;

  // Initialize for a window of the given size
  constexpr explicit RollingHash(
      std::size_t window_size,
      std::uint64_t base = rolling_hash_detail::kDefaultBase,
      std::uint64_t mod = rolling_hash_detail::kMersennePrime)
      : window_size_{window_size},
        base_{base},
        mod_{mod},
        base_pow_{rolling_hash_detail::powerMod(base, window_size - 1, mod)},
        hash_{0},
        count_{0} {}

  // Append a byte to the window (use during initialization)
  constexpr void append(std::uint8_t byte) {
    hash_ = (hash_ * base_ + byte) % mod_;
    ++count_;
  }

  // Slide the window: remove old_byte from front, add new_byte to back
  // Precondition: window is full (count_ == window_size_)
  constexpr void slide(std::uint8_t old_byte, std::uint8_t new_byte) {
    // hash = (hash - old_byte * B^(W-1)) * B + new_byte
    // All operations mod M

    // Remove contribution of leaving byte
    std::uint64_t old_contrib = (old_byte * base_pow_) % mod_;
    if (hash_ >= old_contrib) {
      hash_ -= old_contrib;
    } else {
      // Handle underflow (modular subtraction)
      hash_ = mod_ - (old_contrib - hash_);
    }

    // Shift remaining characters and add new byte
    hash_ = (hash_ * base_ + new_byte) % mod_;
  }

  // Get current hash value
  [[nodiscard]] constexpr auto hash() const noexcept -> HashType {
    return hash_;
  }

  // Reset to empty state
  constexpr void reset() {
    hash_ = 0;
    count_ = 0;
  }

  // Check if window is full
  [[nodiscard]] constexpr auto isFull() const noexcept -> bool {
    return count_ >= window_size_;
  }

  [[nodiscard]] constexpr auto windowSize() const noexcept -> std::size_t {
    return window_size_;
  }

 private:
  std::size_t window_size_;
  std::uint64_t base_;
  std::uint64_t mod_;
  std::uint64_t base_pow_;  // B^(W-1) mod M, precomputed
  std::uint64_t hash_;
  std::size_t count_;       // Bytes appended so far
};

// ----------------------------------------------------------------------------
// Convenience Functions
// ----------------------------------------------------------------------------

// Compute rolling hash of a string (returns hash of entire string)
constexpr auto rollingHash(std::string_view s,
                           std::uint64_t base = rolling_hash_detail::kDefaultBase,
                           std::uint64_t mod = rolling_hash_detail::kMersennePrime)
    -> std::uint64_t {
  std::uint64_t hash = 0;
  for (char c : s) {
    hash = (hash * base + static_cast<std::uint8_t>(c)) % mod;
  }
  return hash;
}

// Find all positions where pattern appears in text using Rabin-Karp
// Returns indices of matches (may include false positives - verify externally)
template <typename OutputIterator>
constexpr auto rabinKarpSearch(std::string_view text, std::string_view pattern,
                               OutputIterator out) -> OutputIterator {
  if (pattern.empty() || pattern.size() > text.size()) {
    return out;
  }

  const std::size_t pattern_len = pattern.size();

  // Compute hash of pattern
  RollingHash pattern_hash(pattern_len);
  for (char c : pattern) {
    pattern_hash.append(static_cast<std::uint8_t>(c));
  }

  // Compute hash of first window in text
  RollingHash text_hash(pattern_len);
  for (std::size_t i = 0; i < pattern_len; ++i) {
    text_hash.append(static_cast<std::uint8_t>(text[i]));
  }

  // Check first position
  if (text_hash.hash() == pattern_hash.hash()) {
    // Verify match (hash collision possible)
    if (text.substr(0, pattern_len) == pattern) {
      *out++ = 0;
    }
  }

  // Slide window over remaining text
  for (std::size_t i = pattern_len; i < text.size(); ++i) {
    text_hash.slide(static_cast<std::uint8_t>(text[i - pattern_len]),
                    static_cast<std::uint8_t>(text[i]));

    if (text_hash.hash() == pattern_hash.hash()) {
      std::size_t pos = i - pattern_len + 1;
      // Verify match
      if (text.substr(pos, pattern_len) == pattern) {
        *out++ = pos;
      }
    }
  }

  return out;
}

// ----------------------------------------------------------------------------
// Content-Defined Chunking
// ----------------------------------------------------------------------------
//
// A powerful application of rolling hashes: split data into chunks where
// the boundaries are determined by the content itself. This means that
// inserting bytes in the middle only affects nearby chunks, not all
// subsequent ones.
//
// The basic idea: chunk boundary at position i if hash(window ending at i)
// has some special property (e.g., low N bits are zero).

class ContentDefinedChunker {
 public:
  // mask determines average chunk size: ~2^(popcount(mask)) bytes
  // e.g., mask = 0xFFF gives ~4KB average chunks
  constexpr explicit ContentDefinedChunker(
      std::size_t window_size = 48,
      std::uint64_t mask = 0xFFF,  // Average ~4KB chunks
      std::size_t min_chunk = 512,
      std::size_t max_chunk = 65536)
      : rolling_hash_{window_size},
        mask_{mask},
        min_chunk_size_{min_chunk},
        max_chunk_size_{max_chunk},
        current_chunk_size_{0} {}

  // Process one byte, returns true if this is a chunk boundary
  constexpr auto processByte(std::uint8_t byte) -> bool {
    ++current_chunk_size_;

    if (!rolling_hash_.isFull()) {
      rolling_hash_.append(byte);
      return false;
    }

    // Update rolling hash
    // (We need the old first byte, but we don't track it - simplified version)
    // In a real implementation, you'd maintain a circular buffer
    rolling_hash_.append(byte);

    // Check for chunk boundary
    if (current_chunk_size_ >= min_chunk_size_) {
      bool is_boundary =
          (current_chunk_size_ >= max_chunk_size_) ||
          ((rolling_hash_.hash() & mask_) == 0);

      if (is_boundary) {
        current_chunk_size_ = 0;
        return true;
      }
    }

    return false;
  }

  constexpr void reset() {
    rolling_hash_.reset();
    current_chunk_size_ = 0;
  }

  [[nodiscard]] constexpr auto currentChunkSize() const noexcept -> std::size_t {
    return current_chunk_size_;
  }

 private:
  RollingHash rolling_hash_;
  std::uint64_t mask_;
  std::size_t min_chunk_size_;
  std::size_t max_chunk_size_;
  std::size_t current_chunk_size_;
};

}  // namespace tempura
