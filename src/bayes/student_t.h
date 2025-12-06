#pragma once

#include <cassert>
#include <cmath>
#include <numbers>
#include <random>
#include <type_traits>

#include "bayes/gamma.h"
#include "bayes/normal.h"
#include "bayes/numeric_traits.h"
#include "special/gamma.h"

namespace tempura::bayes {

// Student's t-distribution: t(ν)
//
// Intuition:
//   A bell-shaped distribution with heavier tails than the normal. Arises when
//   estimating the mean of a normally distributed population with unknown
//   variance from a small sample. As ν → ∞, approaches the standard normal.
//
// Common uses:
//   - t-tests for comparing means
//   - Confidence intervals for small samples
//   - Bayesian inference (posterior distribution for normal mean with
//     unknown variance)
//   - Robust modeling (less sensitive to outliers than normal)
//
// PDF: p(x|ν) = Γ((ν+1)/2) / (√(νπ) Γ(ν/2)) · (1 + x²/ν)^(-(ν+1)/2)
// Support: x ∈ (-∞, +∞)
// Parameter: ν > 0 (degrees of freedom)
//
// Special cases:
//   ν = 1: Cauchy distribution
//   ν = ∞: Standard normal N(0, 1)
//
template <typename T = double>
class StudentT {
  static_assert(
      !std::is_integral_v<T>,
      "StudentT is a continuous distribution - integer types are not "
      "supported. Use a floating-point type (float, double, long double) or a "
      "custom numeric type.");

 public:
  constexpr StudentT(T ν)
      : ν_{ν},
        log_normalizer_{[&] {
          using std::lgamma;
          using std::log;
          return lgamma((ν + T{1}) / T{2}) - lgamma(ν / T{2}) -
                 T{0.5} * log(ν * T{std::numbers::pi});
        }()} {
    assert(ν > T{0} && "StudentT distribution requires ν > 0");
  }

  // Sampling via ratio of normal and chi-squared
  //
  // If Z ~ N(0,1) and V ~ χ²(ν), then T = Z / √(V/ν) ~ t(ν)
  //
  // Since χ²(ν) = Gamma(ν/2, 1/2), we use:
  //   T = Z / √(2·G/ν) where G ~ Gamma(ν/2, 1)
  template <std::uniform_random_bit_generator Generator>
  constexpr auto sample(Generator& g) -> T {
    using std::sqrt;

    // Generate standard normal
    T z = Normal<T>{T{0}, T{1}}.sample(g);

    // Generate chi-squared(ν) via Gamma(ν/2, 1/2)
    // Note: Gamma(α, β) has mean α/β, so Gamma(ν/2, 1/2) has mean ν
    T chi_sq = Gamma<T>{ν_ / T{2}, T{0.5}}.sample(g);

    // T = Z / √(χ²/ν)
    return z / sqrt(chi_sq / ν_);
  }

  // Probability density function (PDF)
  //
  // Formula: p(x|ν) = Γ((ν+1)/2) / (√(νπ) Γ(ν/2)) · (1 + x²/ν)^(-(ν+1)/2)
  constexpr auto prob(T x) const -> T {
    using std::exp;
    return exp(logProb(x));
  }

  // Log probability density (log PDF)
  //
  // Formula: log p(x|ν) = log Γ((ν+1)/2) - log Γ(ν/2) - ½log(νπ)
  //                       - ((ν+1)/2) log(1 + x²/ν)
  //
  // Uses cached normalizing constant for efficiency.
  constexpr auto logProb(T x) const -> T {
    using std::log;
    return log_normalizer_ - ((ν_ + T{1}) / T{2}) * log(T{1} + x * x / ν_);
  }

  // Cumulative distribution function (CDF)
  //
  // Uses the regularized incomplete beta function:
  //   F(x) = ½ + ½ sign(x) I_{x²/(ν+x²)}(½, ν/2)  for x ≠ 0
  //   F(0) = ½
  //
  // Equivalently:
  //   F(x) = 1 - ½ I_{ν/(ν+x²)}(ν/2, ½)  for x ≥ 0
  //   F(x) = ½ I_{ν/(ν+x²)}(ν/2, ½)      for x < 0
  constexpr auto cdf(T x) const -> T {
    if (x == T{0}) {
      return T{0.5};
    }

    T x_sq = x * x;
    T t = ν_ / (ν_ + x_sq);

    // I_t(ν/2, 1/2) gives the tail probability (both tails)
    T tail = special::incompleteBeta(ν_ / T{2}, T{0.5}, t);

    if (x > T{0}) {
      return T{1} - tail / T{2};
    }
    return tail / T{2};
  }

  // Statistical properties

  // E[X] = 0 for ν > 1, undefined for ν ≤ 1
  constexpr auto mean() const -> T {
    if (ν_ <= T{1}) {
      return numeric_quiet_nan(T{});
    }
    return T{0};
  }

  // Var[X] = ν / (ν - 2) for ν > 2
  //        = ∞ for 1 < ν ≤ 2
  //        = undefined for ν ≤ 1
  constexpr auto variance() const -> T {
    if (ν_ <= T{1}) {
      return numeric_quiet_nan(T{});
    }
    if (ν_ <= T{2}) {
      return numeric_infinity(T{});
    }
    return ν_ / (ν_ - T{2});
  }

  // Parameter accessors

  // Degrees of freedom
  constexpr auto nu() const -> T { return ν_; }

 private:
  T ν_;

  // Cached log normalizing constant:
  // log Γ((ν+1)/2) - log Γ(ν/2) - ½ log(νπ)
  T log_normalizer_;
};

}  // namespace tempura::bayes
