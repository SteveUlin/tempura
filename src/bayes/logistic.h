#pragma once

#include <cassert>
#include <cmath>
#include <numbers>
#include <random>
#include <type_traits>

#include "bayes/numeric_traits.h"

namespace tempura::bayes {

// Logistic distribution Logistic(μ, s)
// p(x|μ, s) = exp(-z) / (s(1 + exp(-z))²) where z = (x - μ) / s
//
// Intuition:
//   Similar to the normal distribution but with heavier tails (more probability
//   in extreme values). The CDF is the famous logistic sigmoid function used
//   extensively in machine learning and logistic regression. Models scenarios
//   where growth or change follows an S-curve: slow at extremes, rapid near the
//   center. Examples include population growth with limited resources, disease
//   spread, technology adoption, and binary classification probabilities.
//
// Compared to Normal(μ, σ):
//   - Same mean and median (μ), symmetric around μ
//   - Heavier tails: P(|X - μ| > k) decays exponentially vs. Gaussian (quadratic)
//   - Variance = (π²s²)/3 vs. σ² for normal (use s = σ√3/π for same variance)
//   - CDF has closed form (sigmoid) vs. requiring erf() for normal
//
// Requirements for custom types T:
//   - Arithmetic operators: +, -, *, / (both binary and unary +/-)
//   - Comparison: <, >, ==, !=
//   - Convertible from integer and floating-point literals (0, 1, 2, 0.5, etc.)
//
// For full support, provide these extension points (found via ADL):
//   namespace mylib {
//     constexpr auto log(MyType x) -> MyType { ... }
//     constexpr auto exp(MyType x) -> MyType { ... }
//     constexpr auto numeric_infinity(MyType) -> MyType { ... }
//   }
//
template <typename T = double>
class Logistic {
  static_assert(
      !std::is_integral_v<T>,
      "Logistic is a continuous distribution - integer types are not "
      "supported. "
      "Use a floating-point type (float, double, long double) or a custom "
      "numeric type.");

 public:
  constexpr Logistic(T mu, T sigma) : mu_{mu}, sigma_{sigma} {
    assert(sigma > T{0} && "Logistic distribution requires sigma > 0");
  }

  // Inverse transform sampling using logistic quantile function
  //
  // Formula: Q(u) = μ + s·log(u/(1-u))
  //
  // This transforms a uniform U(0,1) sample into a logistic sample.
  // Reject u = 0 or u = 1 to avoid log(0) and division by zero.
  template <std::uniform_random_bit_generator Generator>
  constexpr auto sample(Generator& g) -> T {
    constexpr T scale =
        T{1} / static_cast<T>(Generator::max() - Generator::min());
    T u = static_cast<T>(g()) * scale;
    while (u == T{0} || u == T{1}) [[unlikely]] {
      u = static_cast<T>(g()) * scale;
    }

    using std::log;
    return mu_ + sigma_ * log(u / (T{1} - u));
  }

  // Probability density function (PDF)
  //
  // Formula: p(x|μ,s) = exp(-z) / (s(1 + exp(-z))²) where z = (x - μ) / s
  //
  // Numerically stable form that avoids overflow:
  //   If z ≥ 0: exp(-z) / (s(1 + exp(-z))²)
  //   If z < 0: exp(z) / (s(exp(z) + 1)²)
  constexpr auto prob(T x) const -> T {
    using std::exp;
    const T z = (x - mu_) / sigma_;

    // Use numerically stable form based on sign of z
    if (z >= T{0}) {
      const T exp_neg_z = exp(-z);
      const T denom = T{1} + exp_neg_z;
      return exp_neg_z / (sigma_ * denom * denom);
    } else {
      const T exp_z = exp(z);
      const T denom = exp_z + T{1};
      return exp_z / (sigma_ * denom * denom);
    }
  }

  // Log probability density (log PDF)
  //
  // Formula: log p(x|μ,s) = -z - log(s) - 2·log(1 + exp(-|z|))
  // where z = (x - μ) / s
  //
  // Computing in log-space avoids underflow for very small probabilities.
  // Uses the stable form: log(1 + exp(-|z|)) which avoids overflow.
  constexpr auto logProb(T x) const -> T {
    using std::exp;
    using std::log;

    const T z = (x - mu_) / sigma_;
    const T abs_z = (z >= T{0}) ? z : -z;

    // log p(x) = -z - log(s) - 2·log(1 + exp(-|z|))
    // For z ≥ 0: -z - log(s) - 2·log(1 + exp(-z))
    // For z < 0: z - log(s) - 2·log(1 + exp(z))
    const T log_sum_exp = log(T{1} + exp(-abs_z));
    return -abs_z - log(sigma_) - T{2} * log_sum_exp;
  }

  // Cumulative distribution function (CDF)
  //
  // Formula: F(x|μ,s) = 1 / (1 + exp(-(x - μ) / s))
  //
  // This is the logistic sigmoid function. It represents the probability
  // that a sample from this distribution is ≤ x.
  constexpr auto cdf(T x) const -> T {
    using std::exp;
    const T z = (x - mu_) / sigma_;
    return T{1} / (T{1} + exp(-z));
  }

  // Statistical properties

  // E[X] = μ
  constexpr auto mean() const -> T { return mu_; }

  // Var[X] = (π²s²)/3
  constexpr auto variance() const -> T {
    const T pi = T{std::numbers::pi};
    return (pi * pi * sigma_ * sigma_) / T{3};
  }

  // Parameter accessors

  // Location parameter
  constexpr auto mu() const -> T { return mu_; }

  // Scale parameter
  constexpr auto sigma() const -> T { return sigma_; }

 private:
  T mu_;
  T sigma_;
};

}  // namespace tempura::bayes
