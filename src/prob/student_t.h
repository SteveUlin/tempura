#pragma once

#include <cassert>
#include <cmath>
#include <concepts>
#include <limits>
#include <numbers>
#include <random>

#include "prob/gamma.h"
#include "prob/normal.h"

namespace tempura::prob {

// Student's t-distribution: p(x|ν) ∝ (1 + x²/ν)^(-(ν+1)/2)
//
// Bell-shaped with heavier tails than normal. As ν → ∞, approaches N(0,1).
// ν = 1 is Cauchy distribution.
template <std::copyable Nu>
class StudentT {
 public:
  constexpr explicit StudentT(Nu nu) : nu_{nu} {}

  template <typename X>
  constexpr auto prob(X x) const {
    using std::exp;
    return exp(logProb(x));
  }

  template <typename X>
  constexpr auto logProb(X x) const {
    using std::lgamma;
    using std::log;

    auto nu = static_cast<double>(nu_);
    double log_normalizer = lgamma((nu + 1.0) / 2.0) - lgamma(nu / 2.0) -
                            0.5 * log(nu * std::numbers::pi);
    return log_normalizer - ((nu + 1.0) / 2.0) * log(1.0 + x * x / nu);
  }

  // Unnormalized: omits normalizing constant
  template <typename X>
  constexpr auto unnormalizedLogProb(X x) const {
    using std::log;
    auto nu = static_cast<double>(nu_);
    return -((nu + 1.0) / 2.0) * log(1.0 + x * x / nu);
  }

  // If Z ~ N(0,1) and V ~ χ²(ν), then T = Z / √(V/ν) ~ t(ν)
  template <std::uniform_random_bit_generator RNG>
  constexpr auto sample(RNG& rng) const {
    using std::sqrt;
    auto nu = static_cast<double>(nu_);

    Normal<double, double> standard_normal{0.0, 1.0};
    Gamma<double, double> chi_sq_gamma{nu / 2.0, 0.5};

    double z = standard_normal.sample(rng);
    double chi_sq = chi_sq_gamma.sample(rng);
    return z / sqrt(chi_sq / nu);
  }

  constexpr auto mean() const {
    auto nu = static_cast<double>(nu_);
    if (nu <= 1.0) {
      return std::numeric_limits<double>::quiet_NaN();
    }
    return 0.0;
  }

  constexpr auto variance() const {
    auto nu = static_cast<double>(nu_);
    if (nu <= 1.0) {
      return std::numeric_limits<double>::quiet_NaN();
    }
    if (nu <= 2.0) {
      return std::numeric_limits<double>::infinity();
    }
    return nu / (nu - 2.0);
  }

  constexpr auto nu() const { return nu_; }

 private:
  Nu nu_;
};

// Deduction guide
template <typename Nu>
StudentT(Nu) -> StudentT<Nu>;

}  // namespace tempura::prob
