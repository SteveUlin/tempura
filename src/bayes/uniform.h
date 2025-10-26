#pragma once

#include <cassert>
#include <cmath>
#include <type_traits>

#include "bayes/numeric_traits.h"

namespace tempura::bayes {

// Continuous uniform distribution U(a, b)
//
// Intuition:
//   Maximum entropy distribution for a bounded interval - every value in [a, b]
//   is equally likely. Models complete uncertainty within known bounds: random
//   arrival times, initial parameter values, unbiased selection from a range.
//
template <typename T = double>
class Uniform {
  static_assert(
      !std::is_integral_v<T>,
      "Uniform is a continuous distribution - integer types are not supported. "
      "Use a floating-point type (float, double, long double) or a custom "
      "numeric type.");

 public:
  constexpr Uniform(T a, T b) : a_{a}, b_{b} {
    assert(a < b && "Uniform distribution requires a < b");
  }

  // Inverse transform sampling: map generator's discrete range to [0,1), then
  // scale to [a,b)
  template <typename Generator>
  constexpr auto sample(Generator& g) -> T {
    // Must avoid integer division - cast both numerator and denominator to T
    constexpr auto range = static_cast<T>(Generator::max() - Generator::min());
    auto normalized = static_cast<T>(g() - Generator::min()) / range;

    return a_ + (b_ - a_) * normalized;
  }

  constexpr auto prob(T x) const -> T {
    if (x < a_ or x > b_) {
      return T{0};
    }
    return T{1} / (b_ - a_);
  }

  // Log-space avoids underflow for very small probabilities
  constexpr auto logProb(T x) const -> T {
    using std::log;
    if (x < a_ or x > b_) {
      return -numeric_infinity(T{});
    }
    return -log(b_ - a_);
  }

  constexpr auto cdf(T x) const -> T {
    if (x < a_) {
      return T{0};
    }
    if (x > b_) {
      return T{1};
    }
    return (x - a_) / (b_ - a_);
  }

  constexpr auto mean() const -> T { return (a_ + b_) / T{2}; }

  // Variance = (b-a)²/12 is the standard result for continuous uniform
  // distributions
  constexpr auto variance() const -> T {
    auto range = b_ - a_;
    return range * range / T{12};
  }

  constexpr auto lower() const -> T { return a_; }
  constexpr auto upper() const -> T { return b_; }

 private:
  T a_;
  T b_;
};

}  // namespace tempura::bayes
