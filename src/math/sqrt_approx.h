#pragma once

#include <bit>
#include <cmath>
#include <cstdint>
#include <limits>

#include "float_bits.h"

namespace tempura {

// √x and 1/√x by hand — the canonical "bootstrap with only +−×÷ and bit tricks" exercise.
// Hardware sqrtsd is one instruction and wins for √x (see sqrt_bench); the point is to see
// what it takes WITHOUT it, and that 1/√x — which has no single double-precision hardware
// instruction — is the case where the bit-hack actually earns its keep.

// √x via Newton–Raphson (Heron): gₙ₊₁ = ½(gₙ + x/gₙ), quadratic. The initial guess halves
// the exponent with a bit trick (add the bias, shift the whole pattern right by one) — a few
// % accurate, so ~5 iterations reach full double precision. Each iteration costs a divide.
constexpr auto sqrtNewton(double x) -> double {
  if (!(x > 0.0)) return x == 0.0 ? x : std::numeric_limits<double>::quiet_NaN();  // ±0→±0, <0/nan→nan
  if (std::isinf(x)) return x;
  std::uint64_t i = std::bit_cast<std::uint64_t>(x);
  i = (i + (std::uint64_t{kExpBias} << kExpShift)) >> 1;  // exponent-halving guess
  double g = std::bit_cast<double>(i);
  for (int it = 0; it < 5; ++it) g = 0.5 * (g + x / g);
  return g;
}

// 1/√x via the Quake "fast inverse square root": a magic-constant bit hack for the guess,
// refined by Newton for the reciprocal-sqrt — yₙ₊₁ = yₙ(1.5 − ½x·yₙ²), which uses NO
// division (the whole trick). 0x5f3759df is the float legend; this is its binary64 analogue.
constexpr auto fastInvSqrt(double x) -> double {
  std::uint64_t i = std::bit_cast<std::uint64_t>(x);
  i = 0x5fe6eb50c7b537a9ULL - (i >> 1);  // double-precision magic constant
  double y = std::bit_cast<double>(i);
  for (int it = 0; it < 4; ++it) y = y * (1.5 - 0.5 * x * y * y);  // division-free refinement
  return y;
}

}  // namespace tempura
