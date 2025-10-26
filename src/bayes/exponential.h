#pragma once

#include <cassert>
#include <cmath>
#include <random>
#include <type_traits>

#include "bayes/numeric_traits.h"

namespace tempura::bayes {

// Exponential distribution
//
// Intuition:
//   Models waiting time until the next event in a memoryless process (constant
//   hazard rate). Common in reliability analysis, queueing theory, and Poisson
//   processes. The only continuous distribution with the memoryless property:
//   P(T > s + t | T > s) = P(T > t). Related to Geometric (discrete analog)
//   and Poisson (event count in fixed time).
//
// PDF: p(x|λ) = λ exp(-λx) for x ≥ 0
template <typename T = double>
class Exponential {
  static_assert(!std::is_integral_v<T>,
                "Exponential is a continuous distribution - use floating-point "
                "types (float, double, long double), not integer types");

 public:
  constexpr Exponential(T λ = T{1}) : λ_{λ} {
    assert(λ > T{0} && "Rate parameter λ must be positive");
  }

  // Inverse transform sampling: U ~ Uniform(0,1) => X = -ln(U)/λ ~ Exp(λ)
  //
  // Derivation: For Y = -ln(X)/λ where X ~ Uniform(0,1):
  //   F_Y(y) = P(Y ≤ y) = P(-ln(X)/λ ≤ y) = P(X ≥ exp(-λy))
  //        = 1 - exp(-λy)  [CDF of Exponential(λ)]
  template <std::uniform_random_bit_generator Generator>
  constexpr auto sample(Generator& gen) -> T {
    // Avoid integer division - cast both numerator and range to T
    constexpr auto range = static_cast<T>(Generator::max() - Generator::min());

    auto bits = gen();
    while (bits == Generator::min()) [[unlikely]] {
      // Reject min value to avoid log(0)
      bits = gen();
    }
    const T u = static_cast<T>(bits - Generator::min()) / range;

    using std::log;
    return -log(u) / λ_;
  }

  constexpr auto prob(T x) const -> T {
    if (x < T{0}) {
      return T{0};
    }
    using std::exp;
    return λ_ * exp(-λ_ * x);
  }

  // Log-space avoids underflow for large x
  constexpr auto logProb(T x) const -> T {
    using std::log;
    using tempura::bayes::numeric_infinity;
    if (x < T{0}) {
      return -numeric_infinity(T{});
    }
    return log(λ_) - λ_ * x;
  }

  constexpr auto cdf(T x) const -> T {
    if (x < T{0}) {
      return T{0};
    }
    using std::exp;
    return T{1} - exp(-λ_ * x);
  }

  constexpr auto invCdf(T p) const -> T {
    assert(p >= T{0} && p <= T{1} && "Probability p must be in [0, 1]");
    using std::log;
    return -log(T{1} - p) / λ_;
  }

  // Mean = 1/λ (expected waiting time)
  constexpr auto mean() const -> T { return T{1} / λ_; }

  // Variance = 1/λ² (spread equals mean²)
  constexpr auto variance() const -> T { return T{1} / (λ_ * λ_); }

  // Rate parameter accessor
  constexpr auto lambda() const -> T { return λ_; }

 private:
  T λ_;
};

}  // namespace tempura::bayes
