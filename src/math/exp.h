#pragma once

#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <numbers>

#include "float_bits.h"
#include "poly.h"

namespace tempura {

// exp(r) ≈ Σ rⁱ/i! on the reduced interval |r| ≤ ln2/2 ≈ 0.347. Degree 13 puts the Taylor
// truncation (~r¹⁴/14! ≈ 1e-17) below one ULP at exp(r) ≈ 1; the Horner rounding then
// dominates, leaving the kernel at a couple ULP — validated by the sweep oracle, not asserted.
inline constexpr std::array<double, 14> kExpCoeffs = [] {
  std::array<double, 14> c{};
  double fact = 1.0;
  for (std::size_t i = 0; i < c.size(); ++i) {
    c[i] = 1.0 / fact;
    fact *= static_cast<double>(i + 1);
  }
  return c;
}();

// ln2 split into hi+lo (Cody–Waite), DERIVED rather than written as magic literals so each
// constant documents its meaning: kLn2Hi is ln2 with its low 26 mantissa bits cleared, so
// k·kLn2Hi is EXACT for any integer k up to ~2¹¹ (k·27-bit fits in 53); kLn2Lo is the
// residual ln2 − kLn2Hi captured in long double, so hi+lo reproduces ln2 beyond double
// precision. kInvLn2 = 1/ln2 to round x/ln2 to the nearest multiple. (Shared by log/pow later.)
inline constexpr double kLn2Hi = std::bit_cast<double>(
    std::bit_cast<std::uint64_t>(std::numbers::ln2_v<double>) & 0xFFFFFFFFFC000000ULL);
inline constexpr double kLn2Lo =
    static_cast<double>(std::numbers::ln2_v<long double> - static_cast<long double>(kLn2Hi));
inline constexpr double kInvLn2 = static_cast<double>(1.0L / std::numbers::ln2_v<long double>);

// Domain boundaries computed from their meaning: eˣ overflows past ln(maxdouble), and rounds
// to +0 below ln(½·smallest-subnormal) = ln(denorm_min) − ln2.
inline constexpr double kExpOverflow = std::log(std::numeric_limits<double>::max());
inline constexpr double kExpUnderflow =
    std::log(std::numeric_limits<double>::denorm_min()) - std::numbers::ln2_v<double>;

// e^x, computed (not delegated to libm): classify edges, Cody–Waite reduce x = k·ln2 + r,
// evaluate exp(r) by a polynomial, reconstruct exp(x) = exp(r)·2^k. constexpr, exception-free.
constexpr auto exp(double x) -> double {
  using std::isnan;
  if (isnan(x)) return x;
  if (x > kExpOverflow) return std::numeric_limits<double>::infinity();  // overflow → +inf
  if (x < kExpUnderflow) return 0.0;                                     // underflow → +0
  // |x| < 2⁻²⁸: x²/2 is below ½ ULP(1), so the series rounds to exactly 1, making 1+x the
  // correctly-rounded result (NOT an FP-flag trick — flags don't exist in a constexpr lib).
  if (x > -0x1p-28 && x < 0x1p-28) return 1.0 + x;

  const double k = std::round(x * kInvLn2);     // nearest integer multiple of ln2
  double r = std::fma(-k, kLn2Hi, x);           // r = x − k·ln2, in two exact-ish steps
  r = std::fma(-k, kLn2Lo, r);

  const double er = horner(kExpCoeffs, r);      // exp(r), |r| ≤ ln2/2
  const int ki = static_cast<int>(k);
  // exp(r)·2^k: ldexpFast for the common normal range, scalbn for the subnormal/overflow tail.
  return isNormalExponent(ki) ? ldexpFast(er, ki) : scalbn(er, ki);
}

}  // namespace tempura
