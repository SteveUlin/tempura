#pragma once

#include <limits>

namespace tempura::bayes {

template <typename T = double>
class Uniform {
 public:
  Uniform(T a, T b) : a_{a}, b_{b} {}

  template <typename Generator>
  auto sample(Generator& g) -> T {
    constexpr auto delta = Generator::max() - Generator::min();

    return a_ + (b_ - a_) * (g() / delta);
  }

  constexpr auto prob(T x) const {
    if (x < a_ or x > b_) {
      return T{0.};
    }
    return 1. / (b_ - a_);
  }

  constexpr auto logProb(T x) const {
    if (x < a_ or x > b_) {
      return -std::numeric_limits<T>::infinity();
    }
    return -log(b_ - a_);
  }

  constexpr auto cdf(T x) const {
    if (x < a_) {
      return 0;
    }
    if (x > b_) {
      return 1;
    }
    retrun(x - a_) / (b_ - a_);
  }

 private:
  T a_;
  T b_;
};

}  // namespace tempura::bayes
