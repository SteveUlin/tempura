#pragma once

#include <array>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <utility>

namespace tempura {

// Polynomial evaluation toolkit — PURE EVALUATION (no fitting, no allocation, no runtime
// basis change). Coefficients are a std::array in LOW-TO-HIGH order: c[i] is the
// coefficient of xⁱ, so p(x) = Σ c[i]·xⁱ. Where coefficients come from (minimax/Remez,
// Chebyshev, Taylor) is an offline design choice; this header only evaluates them.
//
// Scheme guide: horner is the default (fewest ops, one rounding per step, FMA-fused).
// hornerSecondOrder / evalEven / evalOdd handle parity and latency (poly.h, next). The
// accuracy escape-hatch (compHorner) and the Chebyshev-basis Clenshaw form live alongside.

// FMA-Horner: p(x) with a single rounding per step (std::fma fuses the multiply-add).
template <std::floating_point T, std::size_t N>
constexpr auto horner(const std::array<T, N>& c, T x) -> T {
  if constexpr (N == 0) {
    return T{0};
  } else {
    T acc = c[N - 1];
    for (std::size_t i = N - 1; i-- > 0;) acc = std::fma(acc, x, c[i]);  // N==1 ⇒ no iterations
    return acc;
  }
}

// Evaluate p(x) AND p'(x) in one pass (the classic Newton-polishing / error-analysis
// primitive). dp accumulates the running value before p advances, so it ends as the
// derivative. ~2× one Horner.
template <std::floating_point T, std::size_t N>
constexpr auto hornerWithDeriv(const std::array<T, N>& c, T x) -> std::pair<T, T> {
  if constexpr (N == 0) {
    return {T{0}, T{0}};
  } else {
    T p = c[N - 1];
    T dp{0};
    for (std::size_t i = N - 1; i-- > 0;) {
      dp = std::fma(dp, x, p);     // derivative uses the OLD p
      p = std::fma(p, x, c[i]);
    }
    return {p, dp};
  }
}

// Coefficients of p'(x): c'[i] = (i+1)·c[i+1]. A constant (N≤1) differentiates to the empty
// polynomial, which horner evaluates to 0.
template <std::floating_point T, std::size_t N>
constexpr auto derivative(const std::array<T, N>& c) -> std::array<T, (N > 0 ? N - 1 : 0)> {
  std::array<T, (N > 0 ? N - 1 : 0)> d{};
  for (std::size_t i = 1; i < N; ++i) d[i - 1] = static_cast<T>(i) * c[i];
  return d;
}

// ── parity forms: sin/cos/atan/erf have only even or only odd powers, so the coefficient
// array holds just that parity and is evaluated in x² (half the degree, half the work). ──

// Σ c[k]·x^(2k) — an even function.
template <std::floating_point T, std::size_t M>
constexpr auto evalEven(const std::array<T, M>& c, T x) -> T {
  return horner(c, x * x);
}
// x·Σ c[k]·x^(2k) — an odd function. Factoring x OUT (not folding it into the coefficients)
// makes evalOdd(c, −x) == −evalOdd(c, x) hold bit-exactly: horner sees x² either way, and
// the single outer multiply by x carries the sign. That exact antisymmetry is the point.
template <std::floating_point T, std::size_t M>
constexpr auto evalOdd(const std::array<T, M>& c, T x) -> T {
  return x * horner(c, x * x);
}

// Second-order (Dorn) Horner: split the full coefficient array by index parity into two
// independent Horner chains in x², recombine E(x²) + x·O(x²). Same op count as horner but
// ~half the critical-path length — pick it when a longer polynomial is latency-bound.
template <std::floating_point T, std::size_t N>
constexpr auto hornerSecondOrder(const std::array<T, N>& c, T x) -> T {
  if constexpr (N == 0) {
    return T{0};
  } else {
    const T x2 = x * x;
    std::array<T, (N + 1) / 2> even{};
    std::array<T, N / 2> odd{};
    for (std::size_t k = 0; k < even.size(); ++k) even[k] = c[2 * k];
    for (std::size_t k = 0; k < odd.size(); ++k) odd[k] = c[2 * k + 1];
    return std::fma(x, horner(odd, x2), horner(even, x2));
  }
}

}  // namespace tempura
