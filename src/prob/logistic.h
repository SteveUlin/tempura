#pragma once

#include <cassert>
#include <cmath>
#include <concepts>
#include <numbers>
#include <random>

namespace tempura::prob {

// Logistic distribution: p(x|μ,s) = exp(-z) / (s(1+exp(-z))²) where z = (x-μ)/s
//
// Like normal but with heavier tails. CDF is the sigmoid function.
// Variance = (π²s²)/3
template <std::copyable Location, std::copyable Scale>
class Logistic {
 public:
  constexpr Logistic(Location location, Scale scale)
      : location_{location}, scale_{scale} {}

  template <typename X>
  constexpr auto prob(X x) const {
    using std::exp;
    auto z = (x - location_) / scale_;

    if (z >= 0.0) {
      auto exp_neg_z = exp(-z);
      auto denom = 1.0 + exp_neg_z;
      return exp_neg_z / (scale_ * denom * denom);
    } else {
      auto exp_z = exp(z);
      auto denom = exp_z + 1.0;
      return exp_z / (scale_ * denom * denom);
    }
  }

  template <typename X>
  constexpr auto logProb(X x) const {
    using std::exp;
    using std::log;

    auto z = (x - location_) / scale_;
    auto abs_z = (z >= 0.0) ? z : -z;
    auto log_sum_exp = log(1.0 + exp(-abs_z));
    return -abs_z - log(scale_) - 2.0 * log_sum_exp;
  }

  // Unnormalized: omits -log(s) constant
  template <typename X>
  constexpr auto unnormalizedLogProb(X x) const {
    using std::exp;
    using std::log;

    auto z = (x - location_) / scale_;
    auto abs_z = (z >= 0.0) ? z : -z;
    auto log_sum_exp = log(1.0 + exp(-abs_z));
    return -abs_z - 2.0 * log_sum_exp;
  }

  // Inverse transform: Q(u) = μ + s·log(u/(1-u))
  template <std::uniform_random_bit_generator RNG>
  constexpr auto sample(RNG& rng) const {
    constexpr double range_scale =
        1.0 / static_cast<double>(RNG::max() - RNG::min());
    double u = static_cast<double>(rng() - RNG::min()) * range_scale;
    while (u == 0.0 || u == 1.0) [[unlikely]] {
      u = static_cast<double>(rng() - RNG::min()) * range_scale;
    }

    using std::log;
    return location_ + scale_ * log(u / (1.0 - u));
  }

  constexpr auto mean() const { return location_; }

  constexpr auto variance() const {
    return (std::numbers::pi * std::numbers::pi * scale_ * scale_) / 3.0;
  }

  constexpr auto location() const { return location_; }
  constexpr auto scale() const { return scale_; }

  template <typename X>
  constexpr auto cdf(X x) const {
    using std::exp;
    auto z = (x - location_) / scale_;
    return 1.0 / (1.0 + exp(-z));
  }

  template <typename P>
  constexpr auto invCdf(P p) const {
    using std::log;
    return location_ + scale_ * log(p / (1.0 - p));
  }

 private:
  Location location_;
  Scale scale_;
};

// Deduction guide
template <typename Location, typename Scale>
Logistic(Location, Scale) -> Logistic<Location, Scale>;

}  // namespace tempura::prob
