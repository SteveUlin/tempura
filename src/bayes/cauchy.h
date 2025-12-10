#pragma once

#include <cassert>
#include <cmath>
#include <numbers>
#include <random>
#include <type_traits>

#include "bayes/numeric_traits.h"

namespace tempura::bayes {

// Cauchy distribution: p(x|μ,σ) = 1/(πσ(1 + ((x-μ)/σ)²))
//
// Heavy-tailed with undefined mean/variance. Models ratios of standard normals.
template <typename T = double>
class Cauchy {
  static_assert(!std::is_integral_v<T>, "Cauchy requires a floating-point type");

 public:
  constexpr Cauchy(T μ, T σ) : μ_{μ}, σ_{σ} {
    assert(σ > T{0} && "requires σ > 0");
  }

  // Ratio-of-uniforms: sample (x,y) uniformly in unit half-disk, return y/x.
  // Avoids expensive tan() calls.
  template <std::uniform_random_bit_generator Generator>
  constexpr auto sample(Generator& gen) -> T {
    constexpr auto range = static_cast<T>(Generator::max() - Generator::min());
    constexpr auto scale = T{1} / range;
    constexpr auto scale2 = T{2} / range;

    T x{};
    T y{};
    do {
      x = scale * static_cast<T>(gen());
      y = scale2 * static_cast<T>(gen()) - T{1};
    } while (x * x + y * y > T{1});

    return μ_ + σ_ * (y / x);
  }

  constexpr auto prob(T x) const -> T {
    const T z = (x - μ_) / σ_;
    return T{1} / (T{std::numbers::pi} * σ_ * (T{1} + z * z));
  }

  constexpr auto logProb(T x) const -> T {
    using std::log;
    const T z = (x - μ_) / σ_;
    return -log(T{std::numbers::pi} * σ_ * (T{1} + z * z));
  }

  constexpr auto cdf(T x) const -> T {
    using std::atan;
    return T{0.5} + (T{1} / T{std::numbers::pi}) * atan((x - μ_) / σ_);
  }

  constexpr auto invCdf(T p) const -> T {
    using std::tan;
    assert(p >= T{0} && p <= T{1} && "requires 0 ≤ p ≤ 1");
    return μ_ + σ_ * tan(T{std::numbers::pi} * (p - T{0.5}));
  }

  constexpr auto median() const -> T { return μ_; }

  // Mean and variance don't exist for Cauchy distribution
  constexpr auto mean() const -> T { return numeric_quiet_nan(T{}); }
  constexpr auto variance() const -> T { return numeric_quiet_nan(T{}); }

  constexpr auto mu() const -> T { return μ_; }
  constexpr auto sigma() const -> T { return σ_; }

 private:
  T μ_;
  T σ_;
};

}  // namespace tempura::bayes
