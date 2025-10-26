#pragma once

#include <cassert>
#include <cmath>
#include <random>
#include <type_traits>

#include "bayes/numeric_traits.h"

namespace tempura::bayes {

// Beta distribution Beta(α, β)
// p(x|α, β) = (x^(α-1) · (1-x)^(β-1)) / B(α,β)
// where B(α,β) = Γ(α)Γ(β)/Γ(α+β) is the beta function
//
// Intuition:
//   Models probabilities and proportions bounded in [0,1]. The shape parameters
//   α and β control the distribution's form: α affects the left side (near 0),
//   β affects the right side (near 1). Common uses include Bayesian priors for
//   unknown probabilities, modeling success rates, and representing uncertainty
//   in proportions.
//
// Special cases:
//   - Beta(1, 1) = Uniform(0, 1)
//   - Beta(α, β) with α=β gives symmetric distributions centered at 0.5
//   - Beta(α, β) with α<1 and β<1 gives U-shaped distributions
//   - Beta(α, β) with α>1 and β>1 gives bell-shaped distributions
//
template <typename T = double>
class Beta {
  static_assert(
      !std::is_integral_v<T>,
      "Beta is a continuous distribution - integer types are not supported. "
      "Use a floating-point type (float, double, long double) or a custom "
      "numeric type.");

 public:
  constexpr Beta(T alpha, T beta) : alpha_{alpha}, beta_{beta} {
    assert(alpha > T{0} && "Beta distribution requires alpha > 0");
    assert(beta > T{0} && "Beta distribution requires beta > 0");
  }

  // Sample using the ratio of two independent gamma variates
  //
  // Formula: If X ~ Gamma(α, 1) and Y ~ Gamma(β, 1), then X/(X+Y) ~ Beta(α, β)
  //
  // This is a standard method for sampling from the Beta distribution.
  // We generate two gamma samples with the same scale parameter, then take
  // their ratio to get a Beta sample.
  template <std::uniform_random_bit_generator Generator>
  constexpr auto sample(Generator& g) -> T {
    // Need to create distributions each time since they're not constexpr constructible
    std::gamma_distribution<T> gamma_alpha{alpha_, T{1}};
    std::gamma_distribution<T> gamma_beta{beta_, T{1}};

    const T x = gamma_alpha(g);
    const T y = gamma_beta(g);
    return x / (x + y);
  }

  // Probability density function (PDF)
  //
  // Formula: p(x|α,β) = (x^(α-1) · (1-x)^(β-1)) / B(α,β)
  //
  // Returns 0 for x outside [0,1].
  constexpr auto prob(T x) const -> T {
    if (x < T{0} || x > T{1}) {
      return T{0};
    }
    // Handle edge cases to avoid log(0) in the log-space computation
    if (x == T{0}) {
      return (alpha_ == T{1}) ? beta_ : T{0};
    }
    if (x == T{1}) {
      return (beta_ == T{1}) ? alpha_ : T{0};
    }

    using std::exp;
    return exp(unnormalizedLogProb(x) - logBeta(alpha_, beta_));
  }

  // Log probability density (log PDF)
  //
  // Formula: log p(x|α,β) = (α-1)log(x) + (β-1)log(1-x) - log B(α,β)
  //
  // Computing in log-space avoids underflow for small probabilities.
  // Returns -∞ for x outside [0,1].
  constexpr auto logProb(T x) const -> T {
    if (x < T{0} || x > T{1}) {
      return -numeric_infinity(T{});
    }
    // Handle edge cases where log would be undefined
    if ((x == T{0} && alpha_ < T{1}) || (x == T{1} && beta_ < T{1})) {
      return -numeric_infinity(T{});
    }
    if ((x == T{0} && alpha_ > T{1}) || (x == T{1} && beta_ > T{1})) {
      return -numeric_infinity(T{});
    }

    return unnormalizedLogProb(x) - logBeta(alpha_, beta_);
  }

  // Unnormalized log probability (log PDF without normalization constant)
  //
  // Formula: (α-1)log(x) + (β-1)log(1-x)
  //
  // Useful for MCMC and optimization where the normalization constant cancels out.
  constexpr auto unnormalizedLogProb(T x) const -> T {
    if (x < T{0} || x > T{1}) {
      return -numeric_infinity(T{});
    }
    // Handle edge cases
    if (x == T{0}) {
      return (alpha_ <= T{1}) ? T{0} : -numeric_infinity(T{});
    }
    if (x == T{1}) {
      return (beta_ <= T{1}) ? T{0} : -numeric_infinity(T{});
    }

    using std::log;
    return (alpha_ - T{1}) * log(x) + (beta_ - T{1}) * log(T{1} - x);
  }

  // Cumulative distribution function (CDF)
  //
  // Formula: F(x|α,β) = I_x(α, β) where I_x is the regularized incomplete beta function
  //
  // Not implemented: requires incomplete beta function which is not in standard library.
  // Use external libraries (Boost, GSL) or numerical integration for CDF.
  constexpr auto cdf(T /*x*/) const -> T {
    static_assert(false, "Beta CDF requires incomplete beta function - not implemented. "
                         "Use external libraries (Boost::Math, GSL) for this functionality.");
    return T{0};
  }

  // Statistical properties

  // E[X] = α / (α + β)
  constexpr auto mean() const -> T {
    return alpha_ / (alpha_ + beta_);
  }

  // Var[X] = (αβ) / ((α+β)²(α+β+1))
  constexpr auto variance() const -> T {
    const T sum = alpha_ + beta_;
    return (alpha_ * beta_) / (sum * sum * (sum + T{1}));
  }

  // Parameter accessors

  // Shape parameter α
  constexpr auto alpha() const -> T { return alpha_; }

  // Shape parameter β
  constexpr auto beta() const -> T { return beta_; }

 private:
  T alpha_;
  T beta_;

  // Compute log of beta function: log B(α,β) = log Γ(α) + log Γ(β) - log Γ(α+β)
  //
  // Uses lgamma (log gamma) instead of computing gamma directly to avoid overflow.
  // The beta function B(α,β) = Γ(α)Γ(β)/Γ(α+β) is the normalization constant.
  constexpr auto logBeta(T a, T b) const -> T {
    using std::lgamma;
    return lgamma(a) + lgamma(b) - lgamma(a + b);
  }
};

}  // namespace tempura::bayes
