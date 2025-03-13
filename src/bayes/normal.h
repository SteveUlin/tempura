#pragma once

#include <cmath>
#include <numbers>
#include <utility>

namespace tempura::bayes {

template <typename T = double>
class Normal {
 public:
  constexpr Normal(T mu, T sigma)
      : mu_{mu}, sigma_{sigma}, cached_value_{mu_} {}

  template <typename Generator>
  constexpr auto sample(Generator& g) -> T {
    if (has_value_) {
      has_value_ = false;
      return std::exchange(cached_value_, mu_);
    }
    // Generate a random value with the box muller transform
    // https://en.wikipedia.org/wiki/Box%E2%80%93Muller_transform
    constexpr auto delta = static_cast<T>(Generator::max() - Generator::min());
    auto bits = g();
    while (bits == 0) [[unlikely]] {
      bits = g();
    }
    T u1 = static_cast<T>(bits) / delta;
    T u2 = static_cast<T>(g()) / delta;

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
    0.5 * (1 + erf((x - mu_) / (sigma_ * std::numbers::sqrt2)));
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
