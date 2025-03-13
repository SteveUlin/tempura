#pragma once

#include <random>

namespace tempura::bayes {

template <typename T = double>
class Logistic {
public:
  constexpr Logistic(T mu, T sigma) : mu_{mu}, sigma_{sigma} {}

  template <std::uniform_random_bit_generator Generator>
  auto sample(Generator& g) const -> T {
    constexpr T scale = 1.0 / static_cast<T>(Generator::max());
    T x = static_cast<T>(g()) * scale;
    while (x == 0.0 || x == 1.0) {
      x = static_cast<T>(g()) * scale;
    }
    constexpr auto sqrt3_π = std::numbers::sqrt3 / std::numbers::pi;

    using std::log;
    return mu_ + (sigma_ * sqrt3_π * log(x / (1 - x)));
  }

  auto prob(T x) const -> T {
    return 1 / (1 + exp(-(x - mu_) / sigma_));
  }

private:
  T mu_;
  T sigma_;
};

}  // namespace tempura::bayes
