#pragma once

#include <array>
#include <cmath>
#include <utility>

#include "bayes/random.h"
#include "simd/simd.h"

namespace tempura {

// SIMD versions of the random number generators that operate on 8 parallel
// streams. Each lane uses different parameters to avoid correlations.
//
// Available functionality:
// - XorShiftSimd, MultiplyWithCarrySimd, LinearCongruentialSimd: Basic RNG algorithms
// - makeSimdRandom(): High-quality combined generator (returns Vec8i64)
// - toUniform(Vec8i64): Convert to uniform [0, 1) doubles (returns Vec8d)
// - toUniformRange(Vec8i64, a, b): Convert to uniform [a, b) doubles
// - boxMuller(Vec8i64, Vec8i64): Box-Muller transform for N(0,1) normal distribution
//   - Overloads for custom mean/stddev (scalar or per-lane)
//
// Example usage:
//   auto rand = makeSimdRandom();
//   auto uniform = toUniform(rand());                    // 8 uniform [0,1) values
//   auto range = toUniformRange(rand(), -5.0, 5.0);      // 8 uniform [-5,5) values
//   auto [z0, z1] = boxMuller(rand(), rand());           // 16 N(0,1) values

// XorShift parameters for SIMD (8 different parameter sets)
struct XorShiftSimdOptions {
  Vec8i64 a1;
  Vec8i64 a2;
  Vec8i64 a3;
};

template <ShiftDirection dir>
class XorShiftSimd {
 public:
  constexpr XorShiftSimd(XorShiftSimdOptions options) : options_{options} {}

  [[nodiscard]] constexpr auto operator()(Vec8i64 x) const -> Vec8i64 {
    if constexpr (dir == ShiftDirection::Left) {
      x ^= x >> options_.a1;
      x ^= x << options_.a2;
      x ^= x >> options_.a3;
    } else {
      x ^= x << options_.a1;
      x ^= x >> options_.a2;
      x ^= x << options_.a3;
    }
    return x;
  }

 private:
  XorShiftSimdOptions options_;
};

inline auto makeXorShiftSimd() -> XorShiftSimd<ShiftDirection::Left> {
  // Use parameters from XorShiftPresets::kAll, one per lane
  XorShiftSimdOptions options = {
      .a1 = Vec8i64{21, 20, 17, 11, 14, 30, 21, 21},
      .a2 = Vec8i64{35, 41, 31, 29, 29, 35, 37, 43},
      .a3 = Vec8i64{4, 5, 8, 14, 11, 13, 4, 4},
  };

  return XorShiftSimd<ShiftDirection::Left>{options};
}

inline auto makeXorShiftRightSimd() -> XorShiftSimd<ShiftDirection::Right> {
  // Values rotated so that we don't use the exact same values in the same SIMD lane
  XorShiftSimdOptions options = {
      .a1 = Vec8i64{20, 17, 11, 14, 30, 21, 21, 21},
      .a2 = Vec8i64{41, 31, 29, 29, 35, 37, 43, 35},
      .a3 = Vec8i64{5, 8, 14, 11, 13, 4, 4, 4},
  };

  return XorShiftSimd<ShiftDirection::Right>{options};
}

class MultiplyWithCarrySimd {
 public:
  constexpr MultiplyWithCarrySimd(Vec8i64 a) : a_{a} {}

  [[nodiscard]] constexpr auto operator()(Vec8i64 x) const -> Vec8i64 {
    x = a_ * (x & Vec8i64{0xFFFFFFFF}) + (x >> Vec8i64{32});
    return x;
  }

 private:
  Vec8i64 a_;
};

inline auto makeMultiplyWithCarrySimd() -> MultiplyWithCarrySimd {
  // Use parameters from MultiplyWithCarryPresets::kAll, one per lane
  Vec8i64 a{
      4294957665,
      4294963023,
      4162943475,
      3947008974,
      3874257210,
      2936881968,
      2811536238,
      2654432763,
  };

  return MultiplyWithCarrySimd{a};
}

struct LinearCongruentialSimdOptions {
  Vec8i64 a;
  Vec8i64 c;
};

class LinearCongruentialSimd {
 public:
  constexpr LinearCongruentialSimd(LinearCongruentialSimdOptions options)
      : a_{options.a}, c_{options.c} {}

  [[nodiscard]] constexpr auto operator()(Vec8i64 x) const -> Vec8i64 {
    x = a_ * x + c_;
    return x;
  }

 private:
  Vec8i64 a_;
  Vec8i64 c_;
};

inline auto makeLinearCongruentialSimd() -> LinearCongruentialSimd {
  // Use same multiplier for all lanes, but different additive constants
  // to ensure each lane produces independent sequences
  // Note: We use smaller shifts to avoid narrowing conversion issues
  LinearCongruentialSimdOptions options = {
      .a = Vec8i64{0xd1342543de82ef95ULL},
      .c = Vec8i64{
          int64_t(1ULL << 50 | 1),
          int64_t(2ULL << 50 | 1),
          int64_t(3ULL << 50 | 1),
          int64_t(4ULL << 50 | 1),
          int64_t(5ULL << 50 | 1),
          int64_t(6ULL << 50 | 1),
          int64_t(7ULL << 50 | 1),
          int64_t(8ULL << 50 | 1)
      },
  };

  return LinearCongruentialSimd{options};
}

// Default seed for SIMD random generator (8 different seeds for 8 lanes)
inline const Vec8i64 kDefaultSimdRandomSeed{
    7073242132491550564ULL,
    1179269353366884230ULL,
    3941578509859010014ULL,
    4437109666059500420ULL,
    4035966242879597485ULL,
    3373052566401125716ULL,
    1556011196226971778ULL,
    1235654174036890696ULL,
};

inline auto makeSimdRandom(Vec8i64 seed = kDefaultSimdRandomSeed) {
  auto left_shift = makeXorShiftSimd();
  auto mwc_gen = Generator{seed, makeMultiplyWithCarrySimd()};
  auto lcg_gen = Generator{seed, makeLinearCongruentialSimd()};
  auto right_shift_gen = Generator{seed, makeXorShiftRightSimd()};

  return [=]() mutable -> Vec8i64 {
    return (left_shift(lcg_gen()) + right_shift_gen()) ^ mwc_gen();
  };
}

// Convert Vec8i64 random integers to Vec8d uniform doubles in [0, 1)
// Uses the upper 53 bits of the uint64_t values to create IEEE 754 doubles
inline auto toUniform(Vec8i64 random_ints) -> Vec8d {
  // Extract values as array
  std::array<int64_t, 8> int_vals;
  for (std::size_t i = 0; i < 8; ++i) {
    int_vals[i] = random_ints[i];
  }

  // Convert to doubles in [0, 1)
  // Use the full range of uint64_t and divide by 2^64
  std::array<double, 8> double_vals;
  constexpr double kScale = 1.0 / 18446744073709551616.0;  // 1.0 / 2^64

  for (std::size_t i = 0; i < 8; ++i) {
    // Reinterpret as unsigned to avoid negative values
    uint64_t unsigned_val = static_cast<uint64_t>(int_vals[i]);
    double_vals[i] = static_cast<double>(unsigned_val) * kScale;
  }

  return Vec8d{double_vals[0], double_vals[1], double_vals[2], double_vals[3],
               double_vals[4], double_vals[5], double_vals[6], double_vals[7]};
}

// Convert Vec8i64 random integers to Vec8d uniform doubles in [a, b)
inline auto toUniformRange(Vec8i64 random_ints, double a, double b) -> Vec8d {
  Vec8d uniform_01 = toUniform(random_ints);
  Vec8d range = Vec8d{b - a};
  Vec8d offset = Vec8d{a};
  return uniform_01 * range + offset;
}

// Convert Vec8i64 random integers to Vec8d uniform doubles in [a, b) with per-lane ranges
inline auto toUniformRange(Vec8i64 random_ints, Vec8d a, Vec8d b) -> Vec8d {
  Vec8d uniform_01 = toUniform(random_ints);
  Vec8d range = b - a;
  return uniform_01 * range + a;
}

// Box-Muller transform to generate SIMD normal (Gaussian) random numbers
// Returns a pair of Vec8d with independent N(0,1) distributed values
// Each call consumes two Vec8i64 random values and produces 16 normal variates (2 Vec8d)
inline auto boxMuller(Vec8i64 rand1, Vec8i64 rand2)
    -> std::pair<Vec8d, Vec8d> {
  // Convert to uniform [0, 1)
  Vec8d u1 = toUniform(rand1);
  Vec8d u2 = toUniform(rand2);

  // Extract values to arrays for scalar math operations
  std::array<double, 8> u1_arr;
  std::array<double, 8> u2_arr;
  std::array<double, 8> z0_arr;
  std::array<double, 8> z1_arr;

  for (std::size_t i = 0; i < 8; ++i) {
    u1_arr[i] = u1[i];
    u2_arr[i] = u2[i];
  }

  // Apply Box-Muller transform
  // z0 = sqrt(-2 * ln(u1)) * cos(2π * u2)
  // z1 = sqrt(-2 * ln(u1)) * sin(2π * u2)
  constexpr double kTwoPi = 6.283185307179586476925286766559;

  for (std::size_t i = 0; i < 8; ++i) {
    // Ensure u1 > 0 to avoid log(0)
    double u1_safe = u1_arr[i] < 1e-10 ? 1e-10 : u1_arr[i];
    double radius = std::sqrt(-2.0 * std::log(u1_safe));
    double theta = kTwoPi * u2_arr[i];

    z0_arr[i] = radius * std::cos(theta);
    z1_arr[i] = radius * std::sin(theta);
  }

  Vec8d z0{z0_arr[0], z0_arr[1], z0_arr[2], z0_arr[3],
           z0_arr[4], z0_arr[5], z0_arr[6], z0_arr[7]};
  Vec8d z1{z1_arr[0], z1_arr[1], z1_arr[2], z1_arr[3],
           z1_arr[4], z1_arr[5], z1_arr[6], z1_arr[7]};

  return {z0, z1};
}

// Generate SIMD normal random numbers with custom mean and standard deviation
inline auto boxMuller(Vec8i64 rand1, Vec8i64 rand2, double mean, double stddev)
    -> std::pair<Vec8d, Vec8d> {
  auto [z0, z1] = boxMuller(rand1, rand2);
  Vec8d mean_vec{mean};
  Vec8d stddev_vec{stddev};
  return {z0 * stddev_vec + mean_vec, z1 * stddev_vec + mean_vec};
}

// Generate SIMD normal random numbers with per-lane mean and standard deviation
inline auto boxMuller(Vec8i64 rand1, Vec8i64 rand2, Vec8d mean, Vec8d stddev)
    -> std::pair<Vec8d, Vec8d> {
  auto [z0, z1] = boxMuller(rand1, rand2);
  return {z0 * stddev + mean, z1 * stddev + mean};
}

}  // namespace tempura
