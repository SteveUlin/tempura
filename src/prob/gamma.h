#pragma once

#include <cassert>
#include <cmath>
#include <concepts>
#include <random>

#include "prob/normal.h"

namespace tempura::prob {

// Gamma distribution: p(x|α, β) = (β^α / Γ(α)) x^(α-1) exp(-βx) for x > 0
//
// Parameterized by shape α and rate β (not scale).
// Special cases: Exponential (α=1), Chi-squared (α=k/2, β=1/2).
template <std::copyable Shape, std::copyable Rate>
class Gamma {
 public:
  constexpr Gamma(Shape shape, Rate rate) : shape_{shape}, rate_{rate} {}

  template <typename X>
  constexpr auto prob(X x) const {
    using std::exp;
    return exp(logProb(x));
  }

  template <typename X>
  constexpr auto logProb(X x) const {
    using std::lgamma;
    using std::log;
    return shape_ * log(rate_) - lgamma(shape_) + (shape_ - 1.0) * log(x) -
           rate_ * x;
  }

  // Unnormalized: omits α·log(β) - logΓ(α) constant
  template <typename X>
  constexpr auto unnormalizedLogProb(X x) const {
    using std::log;
    return (shape_ - 1.0) * log(x) - rate_ * x;
  }

  // Marsaglia-Tsang rejection sampling
  // For α < 1, uses transformation: Gamma(α,1) = Gamma(α+1,1) · U^(1/α)
  template <std::uniform_random_bit_generator RNG>
  constexpr auto sample(RNG& rng) const {
    using std::log;
    using std::pow;
    using std::sqrt;

    auto alpha = static_cast<double>(shape_);
    auto alpha_sample = alpha < 1.0 ? alpha + 1.0 : alpha;
    double a1 = alpha_sample - 1.0 / 3.0;
    double a2 = 1.0 / sqrt(9.0 * a1);

    constexpr double scale =
        1.0 / static_cast<double>(RNG::max() - RNG::min());

    Normal<double, double> standard_normal{0.0, 1.0};

    double u;
    double v;
    double x;
    do {
      do {
        x = standard_normal.sample(rng);
        v = 1.0 + a2 * x;
      } while (v <= 0.0);

      v = v * v * v;
      u = static_cast<double>(rng() - RNG::min()) * scale;
    } while (u > 1.0 - 0.331 * x * x * x * x &&
             log(u) > 0.5 * x * x + a1 * (1.0 - v + log(v)));

    double result = a1 * v;

    if (alpha < 1.0) {
      do {
        u = static_cast<double>(rng() - RNG::min()) * scale;
      } while (u == 0.0);
      result *= pow(u, 1.0 / alpha);
    }

    return result / rate_;
  }

  constexpr auto mean() const { return shape_ / rate_; }
  constexpr auto variance() const { return shape_ / (rate_ * rate_); }
  constexpr auto shape() const { return shape_; }
  constexpr auto rate() const { return rate_; }

 private:
  Shape shape_;
  Rate rate_;
};

// Deduction guide
template <typename Shape, typename Rate>
Gamma(Shape, Rate) -> Gamma<Shape, Rate>;

}  // namespace tempura::prob
