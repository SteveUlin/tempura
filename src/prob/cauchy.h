#pragma once

#include <cassert>
#include <cmath>
#include <concepts>
#include <numbers>
#include <random>

namespace tempura::prob {

// Cauchy distribution: p(x|μ,γ) = 1/(πγ(1 + ((x-μ)/γ)²))
//
// Heavy-tailed with undefined mean/variance. Models ratios of standard normals.
// Also known as the Lorentz distribution.
template <std::copyable Location, std::copyable Scale>
class Cauchy {
 public:
  constexpr Cauchy(Location location, Scale scale)
      : location_{location}, scale_{scale} {}

  template <typename X>
  constexpr auto prob(X x) const {
    auto z = (x - location_) / scale_;
    return 1.0 / (std::numbers::pi * scale_ * (1.0 + z * z));
  }

  template <typename X>
  constexpr auto logProb(X x) const {
    using std::log;
    auto z = (x - location_) / scale_;
    return -log(std::numbers::pi) - log(scale_) - log(1.0 + z * z);
  }

  // Unnormalized: omits -log(π) - log(γ) constants
  template <typename X>
  constexpr auto unnormalizedLogProb(X x) const {
    using std::log;
    auto z = (x - location_) / scale_;
    return -log(1.0 + z * z);
  }

  // Ratio-of-uniforms: sample (x,y) uniformly in unit half-disk, return y/x
  template <std::uniform_random_bit_generator RNG>
  constexpr auto sample(RNG& rng) const {
    constexpr auto range = static_cast<double>(RNG::max() - RNG::min());
    constexpr auto scale = 1.0 / range;
    constexpr auto scale2 = 2.0 / range;

    double x;
    double y;
    do {
      x = scale * static_cast<double>(rng() - RNG::min());
      y = scale2 * static_cast<double>(rng() - RNG::min()) - 1.0;
    } while (x * x + y * y > 1.0 || x == 0.0);

    return location_ + scale_ * (y / x);
  }

  // Mean and variance don't exist for Cauchy
  constexpr auto median() const { return location_; }
  constexpr auto location() const { return location_; }
  constexpr auto scale() const { return scale_; }

  template <typename X>
  constexpr auto cdf(X x) const {
    using std::atan;
    return 0.5 + (1.0 / std::numbers::pi) * atan((x - location_) / scale_);
  }

  template <typename P>
  constexpr auto invCdf(P p) const {
    using std::tan;
    return location_ + scale_ * tan(std::numbers::pi * (p - 0.5));
  }

 private:
  Location location_;
  Scale scale_;
};

// Deduction guide
template <typename Location, typename Scale>
Cauchy(Location, Scale) -> Cauchy<Location, Scale>;

}  // namespace tempura::prob
