#pragma once

#include <cassert>
#include <cmath>
#include <numbers>
#include <random>
#include <type_traits>

#include "bayes/normal.h"
#include "bayes/numeric_traits.h"
#include "special/gamma.h"

namespace tempura::bayes {

// Gamma distribution: Γ(α, β)
//
// Intuition:
//   Models waiting time for α events in a Poisson process with rate β.
//   Also arises as the conjugate prior for precision (inverse variance) in
//   Bayesian inference. Special cases: Exponential (α=1), Chi-squared (α=k/2, β=1/2).
//
// PDF: p(x|α, β) = (β^α / Γ(α)) x^(α-1) exp(-βx)
// Support: x ∈ (0, ∞)
// Parameters: α > 0 (shape), β > 0 (rate)

template <typename T = double>
class Gamma {
  static_assert(!std::is_integral_v<T>,
                "Gamma is a continuous distribution - use floating-point type");

 public:
  constexpr Gamma(T α, T β)
      : α_(α),
        β_(β),
        α_sample_(α < T{1} ? α + T{1} : α),
        a1_(α_sample_ - T{1} / T{3}),
        a2_([&] {
          using std::sqrt;
          return T{1} / sqrt(T{9} * a1_);
        }()) {
    assert(α > T{0});
    assert(β > T{0});
  }

  // Probability density function (PDF)
  //
  // p(x|α, β) = (β^α / Γ(α)) x^(α-1) exp(-βx)
  constexpr auto prob(T x) const -> T {
    using std::exp;
    using std::lgamma;
    using std::log;
    using std::pow;

    if (x <= T{0}) {
      return T{0};
    }

    // Compute in log-space to avoid overflow/underflow
    const T log_p =
        α_ * log(β_) - lgamma(α_) + (α_ - T{1}) * log(x) - β_ * x;
    return exp(log_p);
  }

  // Log probability density function
  //
  // Avoids underflow for very small probabilities
  constexpr auto logProb(T x) const -> T {
    using std::lgamma;
    using std::log;

    if (x <= T{0}) {
      return -numeric_infinity(T{});
    }

    return α_ * log(β_) - lgamma(α_) + (α_ - T{1}) * log(x) - β_ * x;
  }

  // Cumulative distribution function (CDF)
  //
  // Uses regularized incomplete gamma function P(a, x)
  constexpr auto cdf(T x) const -> T {
    if (x <= T{0}) {
      return T{0};
    }

    // CDF = P(α, βx) where P is the regularized lower incomplete gamma
    return special::incompleteGamma(α_, β_ * x);
  }

  constexpr auto mean() const -> T { return α_ / β_; }

  constexpr auto variance() const -> T { return α_ / (β_ * β_); }

  constexpr auto alpha() const -> T { return α_; }

  constexpr auto beta() const -> T { return β_; }

  // Marsaglia and Tsang's method for sampling Gamma(α, 1)
  //
  // Reference: A Simple Method for Generating Gamma Variables
  // https://dl.acm.org/doi/10.1145/358407.358414
  //
  // For α < 1, uses the transformation: if Y ~ Gamma(α+1, 1) and U ~ Uniform(0,1),
  // then Y·U^(1/α) ~ Gamma(α, 1). This handles the singular behavior at x=0.
  template <std::uniform_random_bit_generator Generator>
  constexpr auto sample(Generator& g) const -> T {
    using std::log;
    using std::pow;

    constexpr T scale = T{1} / static_cast<T>(Generator::max() - Generator::min());

    // Generate Gamma(α_sample, 1) using Marsaglia-Tsang
    T u;
    T v;
    T x;
    do {
      // Rejection sampling: generate candidate from transformed normal
      do {
        x = Normal<T>{T{0}, T{1}}.sample(g);
        v = T{1} + a2_ * x;
      } while (v <= T{0});

      v = v * v * v;
      u = static_cast<T>(g()) * scale;

      // Accept/reject with squeeze and log test
      // Quick rejection: u > 1 - 0.331·x⁴
      // Careful test: log(u) > 0.5·x² + d·(1 - v + log(v))
    } while (u > T{1} - T{0.331} * x * x * x * x &&
             log(u) > T{0.5} * x * x + a1_ * (T{1} - v + log(v)));

    T result = a1_ * v;

    // Apply correction for α < 1
    if (α_ < T{1}) {
      // Generate U ~ Uniform(0, 1), avoiding exact 0
      do {
        u = static_cast<T>(g()) * scale;
      } while (u == T{0});

      // Transform: Gamma(α, 1) = Gamma(α+1, 1) · U^(1/α)
      result *= pow(u, T{1} / α_);
    }

    // Scale by rate parameter: Gamma(α, β) = Gamma(α, 1) / β
    return result / β_;
  }

 private:
  T α_;
  T β_;

  // Cached values for Marsaglia-Tsang sampling algorithm
  //
  // If α < 1, the PDF tends to infinity as x → 0⁺, which breaks rejection sampling.
  // Solution: sample from Gamma(α+1, 1) and transform using U^(1/α)
  T α_sample_;  // max(α, 1) - ensures we sample from well-behaved distribution
  T a1_;        // α_sample - 1/3 (algorithm parameter d)
  T a2_;        // 1/√(9·a1) (algorithm parameter c)
};

}  // namespace tempura::bayes
