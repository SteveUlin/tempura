#pragma once

#include <algorithm>
#include <array>
#include <bit>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace tempura {

// ============================================================================
// HyperLogLog Cardinality Estimator
// ============================================================================
//
// HyperLogLog estimates the number of distinct elements in a stream using
// sublinear space. With 2^P registers (default P=14 gives 16KB), it achieves
// ~1.04/sqrt(m) relative error - about 0.81% for 16384 registers.
//
// THE KEY INSIGHT
// ---------------
// Consider flipping a fair coin until you get heads. The expected number of
// flips is 2. But if you flip many times and record the MAXIMUM number of
// consecutive tails before heads, that maximum grows logarithmically with
// the number of trials. Specifically, if you've seen a run of k tails,
// you've probably done ~2^k trials.
//
// HyperLogLog applies this to hashed values: the position of the first 1-bit
// (leading zeros + 1) in a hash acts like "consecutive tails before heads."
// If you've seen a hash with 10 leading zeros, you've probably hashed ~2^10
// distinct items.
//
// WHY MULTIPLE REGISTERS?
// -----------------------
// A single maximum is too noisy - you might get lucky with one hash. Instead,
// we split items into m = 2^P buckets using the first P bits of the hash, and
// track the maximum leading-zeros-plus-one in each bucket. The cardinality
// estimate combines these maxima using the harmonic mean.
//
// Why harmonic mean? It's robust to outliers. One bucket seeing an unusually
// high max won't skew the estimate as badly as with arithmetic mean.
//
// ALGORITHM
// ---------
// For each item with hash h:
//   1. Use first P bits as bucket index j ∈ [0, 2^P)
//   2. Count leading zeros in remaining (64-P) bits, call it ρ
//   3. Update: registers[j] = max(registers[j], ρ + 1)
//
// To estimate cardinality:
//   1. Compute indicator: Z = Σ 2^(-registers[i])
//   2. Raw estimate: E = α_m * m^2 / Z
//   3. Apply bias corrections for small and large cardinalities
//
// BIAS CORRECTION
// ---------------
// The raw harmonic mean estimate has systematic bias at extremes:
//
// Small cardinalities (E < 2.5m): Many registers are still 0, indicating
// very few items. Use linear counting: n* = m * ln(m / V), where V is the
// count of zero registers. This leverages the birthday paradox.
//
// Large cardinalities (E > 2^32 / 30): Hash collisions become significant.
// Apply: n* = -2^32 * ln(1 - E/2^32). We skip this since we use 64-bit hashes.
//
// The α_m constant corrects multiplicative bias in the harmonic mean:
//   α_m ≈ 0.7213 / (1 + 1.079/m)  for m >= 128
//
// TEMPLATE PARAMETERS
// -------------------
//   Precision - Number of bits for bucket indexing (default 14)
//               m = 2^Precision registers, each storing max leading zeros
//               Memory: 2^Precision bytes (16KB at P=14)
//               Error:  ~1.04 / sqrt(2^Precision)
//
// COMPLEXITY
// ----------
//   add():    O(1)
//   count():  O(m) where m = 2^Precision
//   merge():  O(m)
//   Space:    O(2^Precision) bytes
//
// REQUIREMENTS
// ------------
// Input to add() must be pre-hashed with a good hash function (e.g., xxHash,
// MurmurHash). Poor hash functions cause correlated bits and bad estimates.
//
template <std::size_t Precision = 14>
class HyperLogLog {
  static_assert(Precision >= 4 && Precision <= 18,
                "Precision must be in [4, 18] - lower wastes accuracy, higher wastes memory");

public:
  static constexpr std::size_t kNumRegisters = std::size_t{1} << Precision;
  static constexpr std::size_t kRegisterBits = 64 - Precision;

  HyperLogLog() { registers_.fill(0); }

  // Add a pre-hashed item to the estimator.
  // IMPORTANT: Input must be well-hashed (uniform distribution of bits).
  void add(std::uint64_t hash) {
    // First P bits determine the bucket
    std::size_t bucket = hash >> kRegisterBits;

    // Remaining bits: count leading zeros + 1
    // Mask out the bucket bits, then count leading zeros in the remainder
    std::uint64_t remainder = (hash << Precision) | (std::uint64_t{1} << (Precision - 1));
    // The OR ensures we never count more than kRegisterBits zeros
    // (handles the case where remainder is all zeros)

    std::uint8_t rank = static_cast<std::uint8_t>(std::countl_zero(remainder) + 1);

    registers_[bucket] = std::max(registers_[bucket], rank);
  }

  // Estimate the cardinality (number of distinct items added).
  // Returns 0.0 for empty estimator.
  [[nodiscard]] auto count() const -> double {
    // Compute the indicator function Z = Σ 2^(-M[j])
    double indicator = 0.0;
    std::size_t zeros = 0;

    for (std::size_t i = 0; i < kNumRegisters; ++i) {
      indicator += std::exp2(-static_cast<double>(registers_[i]));
      if (registers_[i] == 0) {
        ++zeros;
      }
    }

    // Raw estimate using harmonic mean
    double alpha = computeAlpha();
    double estimate = alpha * static_cast<double>(kNumRegisters * kNumRegisters) / indicator;

    // Small range correction using linear counting
    // When many registers are zero, we haven't seen enough items to trust HLL
    if (estimate <= 2.5 * static_cast<double>(kNumRegisters) && zeros > 0) {
      // Linear counting: -m * ln(V/m) where V is number of empty buckets
      estimate = static_cast<double>(kNumRegisters) *
                 std::log(static_cast<double>(kNumRegisters) / static_cast<double>(zeros));
    }

    // Large range correction (for 32-bit hashes) - not needed with 64-bit hashes
    // since 2^64 is far beyond practical cardinalities

    return estimate;
  }

  // Merge another HyperLogLog into this one.
  // After merge, this estimator represents the union of both sets.
  void merge(const HyperLogLog& other) {
    for (std::size_t i = 0; i < kNumRegisters; ++i) {
      registers_[i] = std::max(registers_[i], other.registers_[i]);
    }
  }

  // Reset the estimator to empty state.
  void clear() { registers_.fill(0); }

  // Access individual registers (for testing/debugging)
  [[nodiscard]] auto registerAt(std::size_t idx) const -> std::uint8_t {
    assert(idx < kNumRegisters);
    return registers_[idx];
  }

  // Number of registers (for testing)
  [[nodiscard]] static constexpr auto numRegisters() -> std::size_t {
    return kNumRegisters;
  }

  // Theoretical relative standard error: 1.04 / sqrt(m)
  [[nodiscard]] static constexpr auto theoreticalError() -> double {
    return 1.04 / std::sqrt(static_cast<double>(kNumRegisters));
  }

private:
  // Bias correction constant α_m
  // Derivation: The harmonic mean has multiplicative bias that depends on m.
  // For large m: α_m ≈ 0.7213 / (1 + 1.079/m)
  // For small m, exact values are used.
  [[nodiscard]] static constexpr auto computeAlpha() -> double {
    if constexpr (kNumRegisters == 16) {
      return 0.673;
    } else if constexpr (kNumRegisters == 32) {
      return 0.697;
    } else if constexpr (kNumRegisters == 64) {
      return 0.709;
    } else {
      // For m >= 128: α_m = 0.7213 / (1 + 1.079/m)
      return 0.7213 / (1.0 + 1.079 / static_cast<double>(kNumRegisters));
    }
  }

  std::array<std::uint8_t, kNumRegisters> registers_;
};

}  // namespace tempura
