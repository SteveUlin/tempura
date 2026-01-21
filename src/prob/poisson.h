#pragma once

#include <cassert>
#include <cmath>
#include <concepts>
#include <cstdint>
#include <random>

namespace tempura::prob {

// Poisson distribution: P(X=k) = λ^k e^(-λ) / k! for k ∈ {0,1,2,...}
//
// Models count of events in a fixed interval when events occur independently
// at constant rate λ. Mean = variance = λ.
template <std::copyable Lambda>
class Poisson {
 public:
  constexpr explicit Poisson(Lambda lambda) : lambda_{lambda} {}

  template <typename K>
  constexpr auto prob(K k) const {
    using std::exp;
    return exp(logProb(k));
  }

  template <typename K>
  constexpr auto logProb(K k) const {
    using std::lgamma;
    using std::log;

    auto kd = static_cast<double>(k);
    auto lam = static_cast<double>(lambda_);
    return kd * log(lam) - lam - lgamma(kd + 1.0);
  }

  // Unnormalized: omits -λ constant
  template <typename K>
  constexpr auto unnormalizedLogProb(K k) const {
    using std::lgamma;
    using std::log;

    auto kd = static_cast<double>(k);
    auto lam = static_cast<double>(lambda_);
    return kd * log(lam) - lgamma(kd + 1.0);
  }

  // Knuth's algorithm for small λ, rejection sampling for large λ
  template <std::uniform_random_bit_generator RNG>
  constexpr auto sample(RNG& rng) const -> int64_t {
    auto lam = static_cast<double>(lambda_);
    if (lam < 30.0) {
      return sampleKnuth(rng, lam);
    }
    return samplePTRD(rng, lam);
  }

  constexpr auto mean() const { return lambda_; }
  constexpr auto variance() const { return lambda_; }
  constexpr auto lambda() const { return lambda_; }

 private:
  Lambda lambda_;

  // Knuth: generate uniforms until product < e^(-λ)
  template <std::uniform_random_bit_generator RNG>
  static constexpr auto sampleKnuth(RNG& rng, double lam) -> int64_t {
    using std::exp;
    constexpr double scale = 1.0 / static_cast<double>(RNG::max() - RNG::min());
    double exp_neg_lam = exp(-lam);

    int64_t k = 0;
    double p = 1.0;

    do {
      ++k;
      double u = static_cast<double>(rng() - RNG::min()) * scale;
      if (u == 0.0) [[unlikely]] {
        u = scale;
      }
      p *= u;
    } while (p > exp_neg_lam);

    return k - 1;
  }

  // PTRD: transformed rejection with normal approximation
  template <std::uniform_random_bit_generator RNG>
  static constexpr auto samplePTRD(RNG& rng, double lam) -> int64_t {
    using std::exp;
    using std::floor;
    using std::lgamma;
    using std::log;
    using std::sqrt;

    constexpr double scale = 1.0 / static_cast<double>(RNG::max() - RNG::min());

    double sqrt_lam = sqrt(lam);
    double b = 0.931 + 2.53 * sqrt_lam;
    double a = -0.059 + 0.02483 * b;
    double inv_alpha = 1.1239 + 1.1328 / (b - 3.4);
    double v_r = 0.9277 - 3.6224 / (b - 2.0);

    while (true) {
      double u = static_cast<double>(rng() - RNG::min()) * scale - 0.5;
      double v = static_cast<double>(rng() - RNG::min()) * scale;

      double us = 0.5 - (u < 0.0 ? -u : u);
      auto k = static_cast<int64_t>(floor((2.0 * a / us + b) * u + lam + 0.43));

      if (k < 0) {
        continue;
      }

      // Quick acceptance
      if (us >= 0.07 && v <= v_r) {
        return k;
      }

      // Quick rejection
      if (us < 0.013 && v > us) {
        continue;
      }

      // Full acceptance test
      double kd = static_cast<double>(k);
      double log_accept = log(v * inv_alpha / (a / (us * us) + b)) -
                          (lam - kd + kd * log(lam) - lgamma(kd + 1.0));

      if (log_accept <= 0.0) {
        return k;
      }
    }
  }
};

// Deduction guide
template <typename Lambda>
Poisson(Lambda) -> Poisson<Lambda>;

}  // namespace tempura::prob
