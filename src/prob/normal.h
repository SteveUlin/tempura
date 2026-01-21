#pragma once

#include <cassert>
#include <cmath>
#include <concepts>
#include <numbers>
#include <random>
#include <utility>

namespace tempura::prob {

// Normal distribution: p(x|μ,σ) = (1/(σ√(2π))) exp(-(x-μ)²/(2σ²))
//
// Template parameters Mu and Sigma can be any copyable type - scalars, symbolic
// expressions, or distribution objects for hierarchical models.
template <std::copyable Mu, std::copyable Sigma>
class Normal {
 public:
  constexpr Normal(Mu mu, Sigma sigma) : mu_{mu}, sigma_{sigma} {}

  template <typename X>
  constexpr auto prob(X x) const {
    using std::exp;
    using std::sqrt;
    auto z = (x - mu_) / sigma_;
    constexpr double two_pi = 2.0 * std::numbers::pi;
    return exp(-z * z / 2.0) / (sigma_ * sqrt(two_pi));
  }

  template <typename X>
  constexpr auto logProb(X x) const {
    using std::log;
    constexpr double log_sqrt_2pi = 0.9189385332046727;
    auto z = (x - mu_) / sigma_;
    return -0.5 * z * z - log(sigma_) - log_sqrt_2pi;
  }

  // Unnormalized: omits the -log(σ) - log(√2π) constant
  template <typename X>
  constexpr auto unnormalizedLogProb(X x) const {
    auto z = (x - mu_) / sigma_;
    return -0.5 * z * z;
  }

  // Box-Muller transform for sampling
  template <std::uniform_random_bit_generator RNG>
  constexpr auto sample(RNG& rng) const {
    if (has_cached_) {
      has_cached_ = false;
      return mu_ + sigma_ * std::exchange(cached_, decltype(cached_){});
    }

    constexpr auto range =
        static_cast<double>(RNG::max() - RNG::min());

    auto bits = rng();
    while (bits == RNG::min()) [[unlikely]] {
      bits = rng();
    }
    double u1 = static_cast<double>(bits - RNG::min()) / range;
    double u2 = static_cast<double>(rng() - RNG::min()) / range;

    using std::cos;
    using std::log;
    using std::sin;
    using std::sqrt;

    double r = sqrt(-2.0 * log(u1));
    double theta = 2.0 * std::numbers::pi * u2;

    cached_ = r * cos(theta);
    has_cached_ = true;

    return mu_ + sigma_ * (r * sin(theta));
  }

  constexpr auto mean() const { return mu_; }
  constexpr auto variance() const { return sigma_ * sigma_; }
  constexpr auto stddev() const { return sigma_; }
  constexpr auto mu() const { return mu_; }
  constexpr auto sigma() const { return sigma_; }

 private:
  Mu mu_;
  Sigma sigma_;
  mutable double cached_ = 0.0;
  mutable bool has_cached_ = false;
};

// Deduction guide
template <typename Mu, typename Sigma>
Normal(Mu, Sigma) -> Normal<Mu, Sigma>;

}  // namespace tempura::prob
