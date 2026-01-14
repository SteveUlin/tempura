#pragma once

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>

namespace tempura {

// ============================================================================
// Count-Min Sketch
// ============================================================================
//
// A probabilistic data structure for frequency estimation in streaming data.
// It answers "how many times have I seen item X?" using sublinear space, at
// the cost of potential overestimation (never underestimation).
//
// STRUCTURE
// ---------
// The sketch maintains a 2D array of counters: Depth rows x Width columns.
// Each row uses an independent hash function. When you add an item:
//
//   Row 0:  [   ] [   ] [ +1 ] [   ] [   ] ...   <- hash0(item) = 2
//   Row 1:  [   ] [ +1 ] [   ] [   ] [   ] ...   <- hash1(item) = 1
//   Row 2:  [   ] [   ] [   ] [ +1 ] [   ] ...   <- hash2(item) = 3
//   Row 3:  [ +1 ] [   ] [   ] [   ] [   ] ...   <- hash3(item) = 0
//
// To estimate frequency, hash the item in each row and return the MINIMUM
// counter value. The minimum is key: collisions can only increase counters,
// never decrease them, so the true count is always <= minimum estimate.
//
// WHY MINIMUM?
// ------------
// Different items may hash to the same cell (collision), inflating that
// counter. But an item's true count can't exceed any of its counters -
// at least one row likely has minimal collision with that specific item.
// Taking the minimum gives the tightest upper bound.
//
// ERROR BOUNDS
// ------------
// With probability >= 1 - (1/e)^Depth:
//   estimate(x) <= true_count(x) + (total_count / Width)
//
// Intuition: each row adds at most (total/Width) overcounting in expectation.
// More depth = more independent chances for a "clean" row with few collisions.
//
// WHEN TO USE A COUNT-MIN SKETCH
// ------------------------------
// * Streaming frequency estimation (can't store all items)
// * Heavy hitter detection (find items above some frequency threshold)
// * Network traffic monitoring, click counting, word frequencies
// * When you need guaranteed upper bounds (never underestimates)
// * When approximate answers are acceptable (typically within 1-5% error)
//
// TEMPLATE PARAMETERS
// -------------------
//   Width  - Number of columns per row (default 65536 = 2^16)
//            Larger = more accuracy, more memory
//   Depth  - Number of hash functions/rows (default 4)
//            Larger = higher confidence, diminishing returns past ~5-7
//
// COMPLEXITY
// ----------
//   add, estimate:  O(Depth)
//   merge:          O(Width * Depth)
//   Space:          O(Width * Depth) counters = Width * Depth * 4 bytes
//
// HASH FUNCTION STRATEGY
// ----------------------
// Uses a single 64-bit multiply-xor-shift hash with different seeds per row.
// Seeds are chosen to be large primes to minimize correlation between rows.
// This is simpler and faster than MurmurHash/xxHash while being adequate
// for non-adversarial inputs.
//
template <std::size_t Width = 65536, std::size_t Depth = 4>
class CountMinSketch {
  static_assert(Width > 0, "Width must be positive");
  static_assert(Depth > 0, "Depth must be positive");
  static_assert((Width & (Width - 1)) == 0, "Width must be a power of 2");

public:
  CountMinSketch() { clear(); }

  // Add an item with optional count (default 1).
  void add(uint64_t item, uint32_t count = 1) {
    for (std::size_t row = 0; row < Depth; ++row) {
      std::size_t col = hash(item, row);
      // Saturating add to prevent overflow
      uint64_t new_val =
          static_cast<uint64_t>(counters_[row][col]) + count;
      counters_[row][col] =
          static_cast<uint32_t>(std::min(new_val, static_cast<uint64_t>(UINT32_MAX)));
    }
    total_count_ += count;
  }

  // Estimate the frequency of an item. Returns the minimum counter across
  // all rows. This is guaranteed to be >= true count (never underestimates).
  [[nodiscard]] auto estimate(uint64_t item) const -> uint32_t {
    uint32_t min_count = UINT32_MAX;
    for (std::size_t row = 0; row < Depth; ++row) {
      std::size_t col = hash(item, row);
      min_count = std::min(min_count, counters_[row][col]);
    }
    return min_count;
  }

  // Merge another sketch into this one. Both sketches must have the same
  // dimensions (enforced by template parameters). After merge, estimates
  // reflect combined frequencies from both data streams.
  void merge(const CountMinSketch& other) {
    for (std::size_t row = 0; row < Depth; ++row) {
      for (std::size_t col = 0; col < Width; ++col) {
        // Saturating add
        uint64_t new_val = static_cast<uint64_t>(counters_[row][col]) +
                           other.counters_[row][col];
        counters_[row][col] =
            static_cast<uint32_t>(std::min(new_val, static_cast<uint64_t>(UINT32_MAX)));
      }
    }
    total_count_ += other.total_count_;
  }

  // Reset all counters to zero.
  void clear() {
    for (auto& row : counters_) {
      row.fill(0);
    }
    total_count_ = 0;
  }

  // Total count of all additions (sum of all count arguments to add()).
  [[nodiscard]] auto totalCount() const -> uint64_t { return total_count_; }

  // Accessors for dimensions (useful for testing/debugging).
  [[nodiscard]] static constexpr auto width() -> std::size_t { return Width; }
  [[nodiscard]] static constexpr auto depth() -> std::size_t { return Depth; }

private:
  // Hash function: multiply-xor-shift with row-specific seed.
  // Mask ensures result is in [0, Width).
  [[nodiscard]] auto hash(uint64_t item, std::size_t row) const -> std::size_t {
    // Large primes for each row to ensure independence
    static constexpr std::array<uint64_t, 8> kSeeds = {
        0x9E3779B97F4A7C15ULL,  // Golden ratio fractional
        0xC6A4A7935BD1E995ULL,  // MurmurHash constant
        0xBF58476D1CE4E5B9ULL,  // Splitmix64 constant
        0x94D049BB133111EBULL,  // Splitmix64 constant
        0x6C8E9CF570932BD5ULL,  // Random prime
        0x5555555555555555ULL,  // Alternating bits
        0xAB1CD3E7F90A5B6DULL,  // Random prime
        0x1234567890ABCDEFULL,  // Sequential
    };

    uint64_t h = item ^ kSeeds[row % kSeeds.size()];
    h *= 0xFF51AFD7ED558CCDULL;
    h ^= h >> 33;
    h *= 0xC4CEB9FE1A85EC53ULL;
    h ^= h >> 33;

    return static_cast<std::size_t>(h & (Width - 1));
  }

  std::array<std::array<uint32_t, Width>, Depth> counters_;
  uint64_t total_count_ = 0;
};

}  // namespace tempura
