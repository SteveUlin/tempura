#pragma once

#include <cassert>
#include <random>

#include "bayes/normal.h"

namespace tempura::bayes {

// The Gamma Distribution
// p(x|α, β) = β^α / Γ(α) * x^(α - 1) * exp(-β * x)

template <typename T = double>
class Gamma {
 public:
  constexpr Gamma(T α, T β)
      : α_(α),
        β_(β),
        α_sample_(α < 1 ? α + 1. : α),
        a1_(α_sample_ - 1. / 3.),
        a2_(1. / std::sqrt(9. * a1_)) {
    assert(α > 0);
    assert(β > 0);
  }

  // If α is a small integer, you may be better off generating a sum
  // of exponential random variables.
  //
  // A Simple Method for Generating Gamma Variables
  // https://dl.acm.org/doi/10.1145/358407.358414
  template <std::uniform_random_bit_generator Generator>
  constexpr auto sample(Generator& g) const -> T {
    constexpr T scale = 1. / (Generator::max() - Generator::min());
    // Gamma{α, β} ≃ Gamma{α, 1} / β
    T u;
    T v;
    T x;
    do {
      do {
        x = Normal<T>{0, 1}.sample(g);
        v = 1. + a2_ * x;
      } while (v <= 0);
      v = v * v * v;
      u = static_cast<T>(g()) * scale;
    } while (u > 1. - 0.331 * x * x * x * x &&
             std::log(u) > 0.5 * x * x + a1_ * (1. - v + std::log(v)));
    if (α_ < 1) {
      do u = static_cast<T>(g()) * scale; while (u == 0.);
      return std::pow(u, 1. / α_) * a1_ * v / β_;
    }
    return a1_ * v / β_;
  }

 private:
  T α_;
  T β_;

  // Cached values for the sample function
  //
  // If α < 1 the distribution tends to infinity as x approaches 0. This breaks
  // our sampling algorithm. To fix this we use the following relation:
  //   - y ~ Gamma(α + 1, 1)
  //   - u ~ Uniform(0, 1)
  //
  //   => y u^{1 / α} ~ Gamma(α, 1)
  T α_sample_;
  T a1_;
  T a2_;
};

}  // namespace tempura::bayes
