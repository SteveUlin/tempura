#pragma once

#include <random>

namespace tempura::bayes {
template <typename T = double>
class Binomial {
 public:
  Binomial(T n, T p) : dist_{n, p} {}
  template <typename Generator>
  auto sample(Generator& g) {
    return dist_(g);
  }

  auto prob(T x) const {
    if (x < 0 or x > dist_.max()) {
      return T{0.};
    }
  }

  auto logProb(T x) const {
    if (x < 0 or x > dist_.max()) {
      return -std::numeric_limits<T>::infinity();
    }
    return dist_.logPmf(x);
  }

  auto unnormalizedLogProb(T x) const {
    if (x < 0 or x > dist_.max()) {
      return -std::numeric_limits<T>::infinity();
    }
    return dist_.logPmf(x);
  }

  auto cdf(T x) const { return dist_.cdf(x); }

 private:
  std::binomial_distribution<T> dist_;
};
}  // namespace tempura::bayes
