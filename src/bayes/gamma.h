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

// Gamma distribution: p(x|α, β) = (β^α / Γ(α)) x^(α-1) exp(-βx) on (0, ∞)
//
// Models waiting time for α events in a Poisson process.
// Special cases: Exponential (α=1), Chi-squared (α=k/2, β=1/2).

template <typename T = double>
class Gamma {
  static_assert(!std::is_integral_v<T>, "Gamma requires a floating-point type");

 public:
  constexpr Gamma(T α, T β)
      : α_{α},
        β_{β},
        α_sample_{α < T{1} ? α + T{1} : α},
        a1_{α_sample_ - T{1} / T{3}},
        a2_{[&] {
          using std::sqrt;
          return T{1} / sqrt(T{9} * a1_);
        }()} {
    assert(α > T{0} && "requires α > 0");
    assert(β > T{0} && "requires β > 0");
  }

  constexpr auto prob(T x) const -> T {
    if (x <= T{0}) return T{0};
    using std::exp;
    return exp(logProb(x));
  }

  constexpr auto logProb(T x) const -> T {
    if (x <= T{0}) return -numeric_infinity(T{});
    using std::lgamma;
    using std::log;
    return α_ * log(β_) - lgamma(α_) + (α_ - T{1}) * log(x) - β_ * x;
  }

  // CDF via regularized incomplete gamma function
  constexpr auto cdf(T x) const -> T {
    if (x <= T{0}) return T{0};
    return special::incompleteGamma(α_, β_ * x);
  }

  constexpr auto mean() const -> T { return α_ / β_; }
  constexpr auto variance() const -> T { return α_ / (β_ * β_); }
  constexpr auto alpha() const -> T { return α_; }
  constexpr auto beta() const -> T { return β_; }

  // Marsaglia-Tsang rejection sampling
  // For α < 1, uses transformation: Gamma(α,1) = Gamma(α+1,1) · U^(1/α)
  template <std::uniform_random_bit_generator Generator>
  constexpr auto sample(Generator& g) const -> T {
    using std::log;
    using std::pow;

    constexpr T scale = T{1} / static_cast<T>(Generator::max() - Generator::min());

    T u;
    T v;
    T x;
    do {
      do {
        x = Normal<T>{T{0}, T{1}}.sample(g);
        v = T{1} + a2_ * x;
      } while (v <= T{0});

      v = v * v * v;
      u = static_cast<T>(g()) * scale;
    } while (u > T{1} - T{0.331} * x * x * x * x &&
             log(u) > T{0.5} * x * x + a1_ * (T{1} - v + log(v)));

    T result = a1_ * v;

    if (α_ < T{1}) {
      do {
        u = static_cast<T>(g()) * scale;
      } while (u == T{0});
      result *= pow(u, T{1} / α_);
    }

    return result / β_;
  }

 private:
  T α_;
  T β_;

  // Cached Marsaglia-Tsang parameters
  T α_sample_;  // max(α, 1) for well-behaved sampling when α < 1
  T a1_;        // α_sample - 1/3
  T a2_;        // 1/√(9·a1)
};

}  // namespace tempura::bayes
