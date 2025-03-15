#pragma once

#include <random>

namespace tempura::bayes {

// The Cauchy distribution
//
// p(x|μ, σ) = 1 / (πσ(1 + ((x - μ) / σ)²))
//
// The Cauchy distribution is a heavy-tailed distribution with no mean or
// variance.
template <typename T = double>
class Cauchy {
 public:
  constexpr Cauchy(T μ, T σ) : μ_{μ}, σ_{σ} {}

  template <std::uniform_random_bit_generator Generator>
  constexpr auto sample(Generator& gen) -> T {
    // We could use the invCdf sampling method here, but we can avoid the call
    // to std::tan by instead sampling from the unit hemisphere.
    constexpr auto scale =
        1.0 / static_cast<T>(Generator::max() - Generator::min());
    constexpr auto scale2 =
        2.0 / static_cast<T>(Generator::max() - Generator::min());

    // x ∈ [0, 1]
    auto x = (scale * gen());
    // y ∈ [-1, 1]
    auto y = (scale2 * gen()) - 1.0;; // [-1, 1]
    while (x * x + y * y > 1.0 || y == 0.0) {
      x = (scale * gen());
      y = (scale2 * gen()) - 1.0;
    }

    return μ_ + (σ_ * (y / x));
  }

  constexpr auto prob(T x) const {
    return 1. / (std::numbers::pi_v<T> * σ_ *
                 (1. + ((x - μ_) / σ_) * ((x - μ_) / σ_)));
  }

  constexpr auto cdf(T x) const {
    using std::atan;
    return 0.5 + ((1. / std::numbers::pi_v<T>)*atan((x - μ_) / σ_));
  }

  constexpr auto invCdf(T p) const {
    using std::tan;
    return μ_ + (σ_ * tan(std::numbers::pi_v<T> * (p - 0.5)));
  }

 private:
  T μ_;
  T σ_;
};

}  // namespace tempura::bayes
