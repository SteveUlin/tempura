#pragma once

#include <cmath>
#include <numbers>
#include <random>
#include <utility>

namespace tempura::bayes {

// The Normal Distribution
// p(x|μ, σ) = 1 / (σ * sqrt(2π)) * exp(-((x - μ)² / (2σ²)))
template <typename T = double>
class Normal {
 public:
  constexpr Normal(T mu, T sigma)
      : mu_{mu}, sigma_{sigma}, cached_value_{mu_} {}

  template <std::uniform_random_bit_generator Generator>
  constexpr auto sample(Generator& g) -> T {
    if (has_value_) {
      has_value_ = false;
      return std::exchange(cached_value_, mu_);
    }

    // The standard is still faster than my implementations :(
    // 70e6 ops/sec vs 44e6 ops/sec

    // Generate a random value with the box muller transform
    // https://en.wikipedia.org/wiki/Box%E2%80%93Muller_transform
    constexpr auto scale =
        1.0 / static_cast<T>(Generator::max() - Generator::min());
    auto bits = g();
    while (bits == 0) [[unlikely]] {
      bits = g();
    }
    T u1 = static_cast<T>(bits) * scale;
    T u2 = static_cast<T>(g()) * scale;

    using std::cos;
    using std::log;
    using std::sin;
    using std::sqrt;
    const T r = sigma_ * sqrt(-2. * log(u1));
    const T theta = 2. * std::numbers::pi * u2;

    cached_value_ += sigma_ * r * cos(theta);
    has_value_ = true;

    return mu_ + (r * sin(theta));
  }

  // A Fast Normal Random Number Generator
  // https://dl.acm.org/doi/10.1145/138351.138364
  template <std::uniform_random_bit_generator Generator>
  constexpr auto ratioOfUniforms(Generator& g) -> T {
    constexpr static auto scale_u =
        1.0 / static_cast<T>(Generator::max() - Generator::min());
    constexpr static auto scale_v =
        1.7156 / static_cast<T>(Generator::max() - Generator::min());

    T u;
    T v;
    T q;
    using std::abs;
    do {
      v = (static_cast<T>(g()) * scale_v) - 0.5;
      u = static_cast<T>(g()) * scale_u;
      const T x = u - 0.449871;
      const T y = abs(v) + 0.386595;
      q = x * x + y * (0.19600 * y - 0.25472 * x);
    } while ((q > 0.27597) && (q > 0.27846 || v * v > -4. * log(u) * u * u));

    return mu_ + sigma_ * v / u;
  }

  constexpr auto prob(T x) const {
    using std::exp;
    using std::sqrt;
    return exp(-((x - mu_) * (x - mu_) / (2. * sigma_ * sigma_))) /
           (sqrt(2. * std::numbers::pi) * sigma_);
  }

  constexpr auto logProb(T x) const {
    using std::log;
    return -((x - mu_) * (x - mu_) / (2. * sigma_ * sigma_)) -
           (0.5 * log(2. * std::numbers::pi)) - log(sigma_);
  }

  constexpr auto cdf(T x) const {
    using std::erf;
    return 0.5 * (1 + erf((x - mu_) / (sigma_ * std::numbers::sqrt2)));
  }

  constexpr auto mean() const { return mu_; }

  constexpr auto stddev() const { return sigma_; }

  constexpr auto variance() const { return sigma_ * sigma_; }

 private:
  T mu_;
  T sigma_;

  T cached_value_;
  bool has_value_ = false;
};

}  // namespace tempura::bayes
