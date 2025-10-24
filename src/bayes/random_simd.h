#pragma once

#include "bayes/random.h"
#include "simd/simd.h"

namespace tempura {

// SIMD versions of the random number generators that operate on 8 parallel
// streams. Each lane uses different parameters to avoid correlations.

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

}  // namespace tempura
