#pragma once

#include <cassert>
#include <cmath>
#include <numbers>
#include <random>
#include <type_traits>

#include "bayes/numeric_traits.h"

namespace tempura::bayes {

// Cauchy distribution (Lorentz distribution)
//
// Intuition:
//   Heavy-tailed distribution with undefined mean and variance. Models ratios
//   of independent standard normals (X/Y ~ Cauchy when X,Y ~ N(0,1)). Appears
//   in physics (Lorentz/Breit-Wigner resonance) and as the sampling distribution
//   of the median. Extreme outliers are common - the central limit theorem does
//   NOT apply.
//
// PDF: p(x|μ, σ) = 1 / (πσ(1 + ((x - μ) / σ)²))
template <typename T = double>
class Cauchy {
  static_assert(!std::is_integral_v<T>,
                "Cauchy is a continuous distribution - use floating-point types "
                "(float, double, long double), not integer types");

 public:
  constexpr Cauchy(T μ, T σ) : μ_{μ}, σ_{σ} {
    assert(σ > T{0} && "Scale parameter σ must be positive");
  }

  // Ratio-of-uniforms sampling: generates (x,y) uniformly in unit half-disk,
  // then returns y/x. This is equivalent to tan(πU - π/2) but avoids expensive
  // trigonometric calls. Geometrically: uniformly distributed angle in semicircle
  // corresponds to Cauchy-distributed tangent.
  template <std::uniform_random_bit_generator Generator>
  constexpr auto sample(Generator& gen) -> T {
    constexpr auto range = static_cast<T>(Generator::max() - Generator::min());
    constexpr auto scale = T{1} / range;
    constexpr auto scale2 = T{2} / range;

    T x{};
    T y{};
    do {
      x = scale * static_cast<T>(gen());        // x ∈ [0, 1]
      y = scale2 * static_cast<T>(gen()) - T{1};  // y ∈ [-1, 1]
    } while (x * x + y * y > T{1});  // Rejection: keep only unit half-disk

    // Return μ + σ·(y/x). Note: x ∈ (0,1] from rejection, so no division by zero
    return μ_ + σ_ * (y / x);
  }

  constexpr auto prob(T x) const -> T {
    const T z = (x - μ_) / σ_;
    return T{1} / (T{std::numbers::pi} * σ_ * (T{1} + z * z));
  }

  // Log-space avoids underflow in extreme tails
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
    assert(p >= T{0} && p <= T{1} && "Probability p must be in [0, 1]");
    return μ_ + σ_ * tan(T{std::numbers::pi} * (p - T{0.5}));
  }

  // Median (the only well-defined central measure)
  constexpr auto median() const -> T { return μ_; }

  // Mean and variance are undefined (do not exist mathematically)
  // Returning NaN to signal this clearly
  constexpr auto mean() const -> T {
    using tempura::bayes::numeric_quiet_nan;
    return numeric_quiet_nan(T{});
  }
  constexpr auto variance() const -> T {
    using tempura::bayes::numeric_quiet_nan;
    return numeric_quiet_nan(T{});
  }

  // Parameter accessors
  constexpr auto mu() const -> T { return μ_; }
  constexpr auto sigma() const -> T { return σ_; }

 private:
  T μ_;
  T σ_;
};

}  // namespace tempura::bayes
