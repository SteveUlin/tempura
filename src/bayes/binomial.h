#pragma once

#include <cassert>
#include <cmath>
#include <cstdint>
#include <random>
#include <type_traits>

#include "bayes/numeric_traits.h"

namespace tempura::bayes {

// Binomial distribution: P(X=k) = C(n,k) p^k (1-p)^(n-k)
//
// Models the number of successes in n independent Bernoulli(p) trials.
template <typename T = double, typename IntT = int64_t>
class Binomial {
  static_assert(!std::is_integral_v<T>, "Binomial requires a floating-point type");
  static_assert(std::is_integral_v<IntT>, "IntT must be an integer type");

 public:
  constexpr Binomial(IntT n, T p) : n_{n}, p_{p} {
    assert(n >= IntT{0} && "requires n ≥ 0");
    assert(p >= T{0} && p <= T{1} && "requires 0 ≤ p ≤ 1");
  }

  // Direct simulation: O(n) but exact. Normal approximation would be O(1)
  // but introduces bias for small n or extreme p.
  template <std::uniform_random_bit_generator Generator>
  constexpr auto sample(Generator& g) -> IntT {
    constexpr T scale =
        T{1} / static_cast<T>(Generator::max() - Generator::min());

    IntT count = IntT{0};
    for (IntT i = IntT{0}; i < n_; ++i) {
      T u = static_cast<T>(g() - Generator::min()) * scale;
      if (u < p_) {
        ++count;
      }
    }
    return count;
  }

  constexpr auto prob(IntT k) const -> T {
    if (k < IntT{0} or k > n_) return T{0};
    using std::exp;
    return exp(logProb(k));
  }

  // Log-space avoids overflow for large n or extreme p
  constexpr auto logProb(IntT k) const -> T {
    if (k < IntT{0} or k > n_) return -numeric_infinity(T{});

    using std::lgamma;
    using std::log;

    T log_binom_coef = lgamma(static_cast<T>(n_ + IntT{1})) -
                       lgamma(static_cast<T>(k + IntT{1})) -
                       lgamma(static_cast<T>(n_ - k + IntT{1}));

    T log_p_term = T{0};
    T log_1mp_term = T{0};

    if (k > IntT{0}) {
      if (p_ == T{0}) return -numeric_infinity(T{});
      log_p_term = static_cast<T>(k) * log(p_);
    }

    if (k < n_) {
      if (p_ == T{1}) return -numeric_infinity(T{});
      log_1mp_term = static_cast<T>(n_ - k) * log(T{1} - p_);
    }

    return log_binom_coef + log_p_term + log_1mp_term;
  }

  // Direct summation in log-space. For large n, could use incomplete beta:
  // F(k; n, p) = I_{1-p}(n-k, k+1), but direct sum is numerically stable.
  constexpr auto cdf(IntT k) const -> T {
    if (k < IntT{0}) return T{0};
    if (k >= n_) return T{1};

    using std::exp;
    T sum = T{0};
    for (IntT i = IntT{0}; i <= k; ++i) {
      sum += exp(logProb(i));
    }
    return sum;
  }

  constexpr auto mean() const -> T { return static_cast<T>(n_) * p_; }
  constexpr auto variance() const -> T {
    return static_cast<T>(n_) * p_ * (T{1} - p_);
  }

  constexpr auto n() const -> IntT { return n_; }
  constexpr auto p() const -> T { return p_; }

 private:
  IntT n_;
  T p_;
};

}  // namespace tempura::bayes
