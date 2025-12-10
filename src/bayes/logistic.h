#pragma once

#include <cassert>
#include <cmath>
#include <numbers>
#include <random>
#include <type_traits>

#include "bayes/numeric_traits.h"

namespace tempura::bayes {

// Logistic distribution: p(x|μ,s) = exp(-z) / (s(1+exp(-z))²) where z = (x-μ)/s
//
// Like normal but with heavier tails. CDF is the sigmoid function.
// Variance = (π²s²)/3
template <typename T = double>
class Logistic {
  static_assert(!std::is_integral_v<T>, "Logistic requires a floating-point type");

 public:
  constexpr Logistic(T μ, T s) : μ_{μ}, s_{s} {
    assert(s > T{0} && "requires s > 0");
  }

  // Inverse transform: Q(u) = μ + s·log(u/(1-u))
  // Reject u=0 or u=1 to avoid log(0) and division by zero
  template <std::uniform_random_bit_generator Generator>
  constexpr auto sample(Generator& g) -> T {
    constexpr T scale =
        T{1} / static_cast<T>(Generator::max() - Generator::min());
    T u = static_cast<T>(g()) * scale;
    while (u == T{0} || u == T{1}) [[unlikely]] {
      u = static_cast<T>(g()) * scale;
    }

    using std::log;
    return μ_ + s_ * log(u / (T{1} - u));
  }

  // Numerically stable: use exp(-z) if z≥0, else exp(z)
  constexpr auto prob(T x) const -> T {
    using std::exp;
    const T z = (x - μ_) / s_;

    if (z >= T{0}) {
      const T exp_neg_z = exp(-z);
      const T denom = T{1} + exp_neg_z;
      return exp_neg_z / (s_ * denom * denom);
    } else {
      const T exp_z = exp(z);
      const T denom = exp_z + T{1};
      return exp_z / (s_ * denom * denom);
    }
  }

  // Log-space avoids underflow: log p(x) = -|z| - log(s) - 2·log(1 + exp(-|z|))
  constexpr auto logProb(T x) const -> T {
    using std::exp;
    using std::log;

    const T z = (x - μ_) / s_;
    const T abs_z = (z >= T{0}) ? z : -z;

    const T log_sum_exp = log(T{1} + exp(-abs_z));
    return -abs_z - log(s_) - T{2} * log_sum_exp;
  }

  constexpr auto cdf(T x) const -> T {
    using std::exp;
    const T z = (x - μ_) / s_;
    return T{1} / (T{1} + exp(-z));
  }

  constexpr auto mean() const -> T { return μ_; }

  constexpr auto variance() const -> T {
    const T π = T{std::numbers::pi};
    return (π * π * s_ * s_) / T{3};
  }

  constexpr auto mu() const -> T { return μ_; }
  constexpr auto s() const -> T { return s_; }

 private:
  T μ_;
  T s_;
};

}  // namespace tempura::bayes
