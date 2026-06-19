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

}  // namespace tempura
