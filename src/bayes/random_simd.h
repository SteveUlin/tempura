#pragma once

#include "bayes/random.h"
#include "simd/simd.h"

namespace tempura {

auto makeXorShiftSimd() -> XorShiftFn<Vec8i64, ShiftDirection::Left> {
  XorShiftOptions<Vec8i64> options = {
      .a1 = Vec8i64{21, 20, 17, 11, 14, 30, 21, 21},
      .a2 = Vec8i64{35, 41, 31, 29, 29, 35, 37, 43},
      .a3 = Vec8i64{4, 5, 8, 14, 11, 13, 4, 4},
  };

  return XorShiftFn<Vec8i64, ShiftDirection::Left>{options};
}

auto makeXorShiftRightSimd() -> XorShiftFn<Vec8i64, ShiftDirection::Right> {
  // Values taken from makeXorShiftSimd() but shifted so that we don't use
  // the exact same values in the same simd lane
  XorShiftOptions<Vec8i64> options = {
      .a1 = Vec8i64{20, 17, 11, 14, 30, 21, 21, 21},
      .a2 = Vec8i64{41, 31, 29, 29, 35, 37, 43, 35},
      .a3 = Vec8i64{5, 8, 14, 11, 13, 4, 4, 4},
  };

  return XorShiftFn<Vec8i64, ShiftDirection::Right>{options};
}

auto makeMultiplyWithCarrySimd() -> MultiplyWithCarryFn<Vec8i64> {
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

  return MultiplyWithCarryFn{a};
}

auto makeLinearCongruentialSimd() -> LinearCongruentialFn<Vec8i64> {
  LinearCongruentialOptions<Vec8i64> options = {
      .a = Vec8i64{0xd1342543de82ef95},
      .c = Vec8i64{
          1ULL << 60 | 1,
          2ULL << 60 | 1,
          3ULL << 60 | 1,
          4ULL << 60 | 1,
          5ULL << 60 | 1,
          6ULL << 60 | 1,
          7ULL << 60 | 1,
          8LL << 60 | 1
      },
  };

  return LinearCongruentialFn<Vec8i64>{options};
}

auto makeSimdRand() {
  const Vec8i64 seed{
      7073242132491550564,
      1179269353366884230,
      3941578509859010014,
      4437109666059500420,
      4035966242879597485,
      3373052566401125716,
      1556011196226971778,
      1235654174036890696,
  };
  auto left_shift = makeXorShiftSimd();
  auto mwc_rng = RNG{seed, makeMultiplyWithCarrySimd()};
  auto lc_rng = RNG{seed, makeLinearCongruentialSimd()};
  auto right_shift_rng = RNG{seed, makeXorShiftRightSimd()};

  return [left_shift, mwc_rng, lc_rng, right_shift_rng]() mutable {
    return (left_shift(lc_rng()) + right_shift_rng()) ^ mwc_rng();
  };
}

}  // namespace tempura
