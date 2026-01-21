#pragma once

#include <cassert>
#include <cmath>
#include <concepts>
#include <numbers>
#include <random>

namespace tempura::prob {

// Half-Normal distribution: p(x|σ) = (2/(σ√(2π))) exp(-x²/(2σ²)) for x ≥ 0
//
// A Normal(0, σ) truncated to positive values, with density doubled.
// Useful as a prior for scale parameters.
template <std::copyable Sigma>
class HalfNormal {
 public:
  constexpr explicit HalfNormal(Sigma sigma) : sigma_{sigma} {}

  template <typename X>
  constexpr auto prob(X x) const {
    using std::exp;
    using std::sqrt;
    constexpr double two_pi = 2.0 * std::numbers::pi;
    auto z = x / sigma_;
    return 2.0 * exp(-z * z / 2.0) / (sigma_ * sqrt(two_pi));
  }

  template <typename X>
  constexpr auto logProb(X x) const {
    using std::log;
    constexpr double log_sqrt_2pi = 0.9189385332046727;
    auto z = x / sigma_;
    return std::numbers::ln2 - log_sqrt_2pi - log(sigma_) - 0.5 * z * z;
  }

  // Unnormalized: omits constants
  template <typename X>
  constexpr auto unnormalizedLogProb(X x) const {
    auto z = x / sigma_;
    return -0.5 * z * z;
  }

  // Sample via |Z| where Z ~ Normal(0, σ)
  template <std::uniform_random_bit_generator RNG>
  constexpr auto sample(RNG& rng) const {
    constexpr auto range = static_cast<double>(RNG::max() - RNG::min());

    auto bits = rng();
    while (bits == RNG::min()) [[unlikely]] {
      bits = rng();
    }
    double u1 = static_cast<double>(bits - RNG::min()) / range;
    double u2 = static_cast<double>(rng() - RNG::min()) / range;

    using std::abs;
    using std::log;
    using std::sin;
    using std::sqrt;

    double r = sqrt(-2.0 * log(u1));
    double theta = 2.0 * std::numbers::pi * u2;
    double z = r * sin(theta);

    return sigma_ * abs(z);
  }

  constexpr auto mean() const {
    using std::sqrt;
    constexpr double sqrt_2_over_pi = 0.7978845608028654;
    return sigma_ * sqrt_2_over_pi;
  }

  constexpr auto variance() const {
    constexpr double one_minus_2_over_pi = 0.36338022763241866;
    return sigma_ * sigma_ * one_minus_2_over_pi;
  }

  constexpr auto sigma() const { return sigma_; }

 private:
  Sigma sigma_;
};

// Deduction guide
template <typename Sigma>
HalfNormal(Sigma) -> HalfNormal<Sigma>;

}  // namespace tempura::prob
