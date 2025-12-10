#pragma once

#include <cassert>
#include <cmath>
#include <type_traits>

#include "bayes/gamma.h"
#include "bayes/numeric_traits.h"
#include "special/gamma.h"

namespace tempura::bayes {

// Beta distribution: p(x|α,β) ∝ x^(α-1) · (1-x)^(β-1) on [0,1]
//
// Models probabilities and proportions. Beta(1,1) = Uniform(0,1).
// α,β > 1 gives bell-shaped; α,β < 1 gives U-shaped.
template <typename T = double>
class Beta {
  static_assert(!std::is_integral_v<T>, "Beta requires a floating-point type");

 public:
  constexpr Beta(T α, T β) : α_{α}, β_{β} {
    assert(α > T{0} && "requires α > 0");
    assert(β > T{0} && "requires β > 0");
  }

  // If X ~ Gamma(α,1) and Y ~ Gamma(β,1), then X/(X+Y) ~ Beta(α,β)
  template <std::uniform_random_bit_generator Generator>
  constexpr auto sample(Generator& g) const -> T {
    const T x = Gamma<T>{α_, T{1}}.sample(g);
    const T y = Gamma<T>{β_, T{1}}.sample(g);
    return x / (x + y);
  }

  constexpr auto prob(T x) const -> T {
    if (x < T{0} || x > T{1}) return T{0};

    // Boundary behavior depends on shape parameters
    if (x == T{0}) {
      if (α_ < T{1}) return numeric_infinity(T{});  // U-shaped: density → +∞
      if (α_ == T{1}) return β_;
      return T{0};
    }
    if (x == T{1}) {
      if (β_ < T{1}) return numeric_infinity(T{});
      if (β_ == T{1}) return α_;
      return T{0};
    }

    using std::exp;
    return exp(unnormalizedLogProb(x) - logBeta());
  }

  // Log-space avoids underflow for small probabilities
  constexpr auto logProb(T x) const -> T {
    if (x < T{0} || x > T{1}) return -numeric_infinity(T{});

    if (x == T{0}) {
      if (α_ < T{1}) return numeric_infinity(T{});
      if (α_ == T{1}) {
        using std::log;
        return log(β_);
      }
      return -numeric_infinity(T{});
    }
    if (x == T{1}) {
      if (β_ < T{1}) return numeric_infinity(T{});
      if (β_ == T{1}) {
        using std::log;
        return log(α_);
      }
      return -numeric_infinity(T{});
    }

    return unnormalizedLogProb(x) - logBeta();
  }

  // Without normalization constant - useful for MCMC
  constexpr auto unnormalizedLogProb(T x) const -> T {
    if (x < T{0} || x > T{1}) return -numeric_infinity(T{});

    if (x == T{0}) {
      if (α_ < T{1}) return numeric_infinity(T{});
      if (α_ == T{1}) return T{0};
      return -numeric_infinity(T{});
    }
    if (x == T{1}) {
      if (β_ < T{1}) return numeric_infinity(T{});
      if (β_ == T{1}) return T{0};
      return -numeric_infinity(T{});
    }

    using std::log;
    return (α_ - T{1}) * log(x) + (β_ - T{1}) * log(T{1} - x);
  }

  // CDF via regularized incomplete beta function
  constexpr auto cdf(T x) const -> T {
    if (x <= T{0}) return T{0};
    if (x >= T{1}) return T{1};
    return special::incompleteBeta(α_, β_, x);
  }

  constexpr auto mean() const -> T { return α_ / (α_ + β_); }

  constexpr auto variance() const -> T {
    const T sum = α_ + β_;
    return (α_ * β_) / (sum * sum * (sum + T{1}));
  }

  // Mode = (α-1)/(α+β-2), undefined (NaN) when α ≤ 1 or β ≤ 1
  constexpr auto mode() const -> T {
    if (α_ <= T{1} || β_ <= T{1}) return T{0} / T{0};
    return (α_ - T{1}) / (α_ + β_ - T{2});
  }

  constexpr auto alpha() const -> T { return α_; }
  constexpr auto beta() const -> T { return β_; }

 private:
  T α_;
  T β_;

  constexpr auto logBeta() const -> T {
    using std::lgamma;
    return lgamma(α_) + lgamma(β_) - lgamma(α_ + β_);
  }
};

}  // namespace tempura::bayes
