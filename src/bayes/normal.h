#pragma once

#include <cassert>
#include <cmath>
#include <numbers>
#include <random>
#include <type_traits>
#include <utility>

#include "bayes/numeric_traits.h"

namespace tempura::bayes {

// Normal distribution: p(x|μ,σ) = (1/(σ√(2π))) exp(-(x-μ)²/(2σ²))
//
// The bell curve. Models sums of many independent effects (Central Limit Theorem).
template <typename T = double>
class Normal {
  static_assert(!std::is_integral_v<T>, "Normal requires a floating-point type");

 public:
  constexpr Normal(T μ, T σ) : mu_{μ}, sigma_{σ}, cached_value_{μ} {
    assert(σ > T{0} && "requires σ > 0");
  }

  // Box-Muller generates two samples at once; we cache the second one
  template <std::uniform_random_bit_generator Generator>
  constexpr auto sample(Generator& g) -> T {
    if (has_value_) {
      has_value_ = false;
      return std::exchange(cached_value_, mu_);
    }

    auto [z0, z1] = boxMuller(g);
    cached_value_ = mu_ + sigma_ * z1;
    has_value_ = true;
    return mu_ + sigma_ * z0;
  }

  constexpr auto prob(T x) const -> T {
    using std::exp;
    using std::sqrt;
    const T z = (x - mu_) / sigma_;
    const T two_pi = T{2} * T{std::numbers::pi};
    return exp(-z * z / T{2}) / (sigma_ * sqrt(two_pi));
  }

  // Log-space avoids underflow for small probabilities in the tails
  constexpr auto logProb(T x) const -> T {
    using std::log;
    const T z = (x - mu_) / sigma_;
    const T two_pi = T{2} * T{std::numbers::pi};
    return -z * z / T{2} - log(sigma_) - T{0.5} * log(two_pi);
  }

  constexpr auto cdf(T x) const -> T {
    using std::erf;
    const T z = (x - mu_) / (sigma_ * T{std::numbers::sqrt2});
    return T{0.5} * (T{1} + erf(z));
  }

  constexpr auto mean() const -> T { return mu_; }
  constexpr auto variance() const -> T { return sigma_ * sigma_; }
  constexpr auto stddev() const -> T { return sigma_; }
  constexpr auto mu() const -> T { return mu_; }
  constexpr auto sigma() const -> T { return sigma_; }

 private:
  T mu_;
  T sigma_;
  T cached_value_;
  bool has_value_ = false;

  // Box-Muller: Z₀ = √(-2 ln U₁) cos(2π U₂), Z₁ = √(-2 ln U₁) sin(2π U₂)
  template <std::uniform_random_bit_generator Generator>
  constexpr auto boxMuller(Generator& g) -> std::pair<T, T> {
    constexpr auto range = static_cast<T>(Generator::max() - Generator::min());

    auto bits = g();
    while (bits == T{0}) [[unlikely]] {
      bits = g();  // Avoid log(0)
    }
    T u1 = static_cast<T>(bits) / range;
    T u2 = static_cast<T>(g()) / range;

    using std::cos;
    using std::log;
    using std::sin;
    using std::sqrt;

    const T r = sqrt(-T{2} * log(u1));
    const T theta = T{2} * T{std::numbers::pi} * u2;
    return {r * sin(theta), r * cos(theta)};
  }
};

}  // namespace tempura::bayes
