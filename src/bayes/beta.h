#pragma once

#include <random>
#include <stdexcept>

namespace tempura::bayes {

template <typename T = double>
class Beta {
 public:
  constexpr Beta(T alpha, T sigma) : X_{alpha, 1}, Y_{sigma, 1} {}

  template <std::uniform_random_bit_generator Generator>
  constexpr auto sample(Generator& g) const -> T {
    T x = X_(g);
    return x / (x + Y_(g));
  }

  constexpr auto prob(T x) const {
    if (x < 0 || x > 1) {
      return T{0.};
    }
    return pow(x, X_.alpha() - 1) * pow(1 - x, Y_.alpha() - 1) /
           std::beta(X_.alpha(), Y_.alpha());
  }

  constexpr auto logProb(T x) const {
    if (x < 0 || x > 1) {
      return -std::numeric_limits<T>::infinity();
    }
    return ((X_.alpha() - 1) * log(x)) + ((Y_.alpha() - 1) * log(1 - x)) -
           log(std::beta(X_.alpha(), Y_.alpha()));
  }

  constexpr auto unnormalizedLogProb(T x) const {
    if (x < 0 || x > 1) {
      return -std::numeric_limits<T>::infinity();
    }
    return ((X_.alpha() - 1) * log(x)) + ((Y_.alpha() - 1) * log(1 - x));
  }

  constexpr auto cdf(T /*unused*/) const { throw std::runtime_error("Not implemented"); }

 private:
  std::gamma_distribution<T> X_;
  std::gamma_distribution<T> Y_;
};

}  // namespace tempura::bayes
