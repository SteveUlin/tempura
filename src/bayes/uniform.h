#pragma once

#include <cassert>
#include <cmath>
#include <type_traits>

#include "bayes/numeric_traits.h"

namespace tempura::bayes {

// Uniform distribution: p(x|a,b) = 1/(b-a) on [a,b]
//
// Every value in the interval is equally likely. Maximum entropy for bounded support.
template <typename T = double>
class Uniform {
  static_assert(!std::is_integral_v<T>, "Uniform requires a floating-point type");

 public:
  constexpr Uniform(T a, T b) : a_{a}, b_{b} {
    assert(a < b && "requires a < b");
  }

  // Inverse transform sampling: U = (g-min)/(max-min), then a + (b-a)·U
  template <typename Generator>
  constexpr auto sample(Generator& g) -> T {
    constexpr auto range = static_cast<T>(Generator::max() - Generator::min());
    auto normalized = static_cast<T>(g() - Generator::min()) / range;
    return a_ + (b_ - a_) * normalized;
  }

  constexpr auto prob(T x) const -> T {
    if (x < a_ or x > b_) return T{0};
    return T{1} / (b_ - a_);
  }

  constexpr auto logProb(T x) const -> T {
    using std::log;
    if (x < a_ or x > b_) return -numeric_infinity(T{});
    return -log(b_ - a_);
  }

  constexpr auto cdf(T x) const -> T {
    if (x < a_) return T{0};
    if (x > b_) return T{1};
    return (x - a_) / (b_ - a_);
  }

  constexpr auto mean() const -> T { return (a_ + b_) / T{2}; }

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
