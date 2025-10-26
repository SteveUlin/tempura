#pragma once

#include <cassert>
#include <cmath>
#include <cstdint>
#include <random>
#include <type_traits>

#include "bayes/numeric_traits.h"

namespace tempura::bayes {

// Binomial distribution Binomial(n, p)
// P(X = k) = C(n,k) p^k (1-p)^(n-k)
//
// Intuition:
//   Models the number of successes in n independent Bernoulli(p) trials.
//   Examples: number of heads in n coin flips, number of conversions in n
//   customer trials, number of defective items in a batch. The sum of n
//   independent Bernoulli(p) random variables is Binomial(n, p).
//
// Compared to Bernoulli(p):
//   - Bernoulli models a single trial (n=1): Bernoulli(p) = Binomial(1, p)
//   - Binomial models the count across multiple trials
//
// Template parameters:
//   T - Probability type (must be floating-point or custom numeric type)
//   IntT - Integer type for counts (default: int64_t)
//
template <typename T = double, typename IntT = int64_t>
class Binomial {
  static_assert(
      !std::is_integral_v<T>,
      "Binomial probability parameter must be a floating-point type. "
      "Integer types cannot represent probabilities in [0,1]. "
      "Use a floating-point type (float, double, long double) or a custom "
      "numeric type.");

  static_assert(
      std::is_integral_v<IntT>,
      "Binomial count type must be an integer type (int, int64_t, etc.).");

 public:
  constexpr Binomial(IntT n, T p) : n_{n}, p_{p} {
    assert(n >= IntT{0} && "Binomial distribution requires n >= 0");
    assert(p >= T{0} && p <= T{1} &&
           "Binomial distribution requires 0 <= p <= 1");
  }

  // Sampling via n independent Bernoulli trials
  //
  // For each of n trials, generate U ~ Uniform(0,1) and count successes (U < p).
  // This is the most direct implementation of the binomial process.
  //
  // Alternative: For large n and moderate p, BTPE (Binomial, Triangle, Parallelogram, Exponential)
  // algorithm would be more efficient, but the simple method is correct and constexpr-compatible.
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

  // Probability mass function (PMF)
  //
  // Formula: P(X = k) = C(n,k) p^k (1-p)^(n-k)
  // where C(n,k) = n! / (k!(n-k)!)
  constexpr auto prob(IntT k) const -> T {
    if (k < IntT{0} or k > n_) {
      return T{0};
    }

    using std::exp;

    // Use log-space computation to avoid overflow, then exponentiate
    return exp(logProb(k));
  }

  // Log probability mass (log PMF)
  //
  // Formula: log P(X = k) = log C(n,k) + k·log(p) + (n-k)·log(1-p)
  //
  // Computing in log-space avoids overflow/underflow for large n or extreme p.
  constexpr auto logProb(IntT k) const -> T {
    if (k < IntT{0} or k > n_) {
      return -numeric_infinity(T{});
    }

    using std::lgamma;
    using std::log;

    // log C(n,k) = log(n!) - log(k!) - log((n-k)!)
    //            = lgamma(n+1) - lgamma(k+1) - lgamma(n-k+1)
    T log_binom_coef = lgamma(static_cast<T>(n_ + IntT{1})) -
                       lgamma(static_cast<T>(k + IntT{1})) -
                       lgamma(static_cast<T>(n_ - k + IntT{1}));

    // Handle edge cases for p
    T log_p_term = T{0};
    T log_1mp_term = T{0};

    if (k > IntT{0}) {
      if (p_ == T{0}) {
        return -numeric_infinity(T{});
      }
      log_p_term = static_cast<T>(k) * log(p_);
    }

    if (k < n_) {
      if (p_ == T{1}) {
        return -numeric_infinity(T{});
      }
      log_1mp_term = static_cast<T>(n_ - k) * log(T{1} - p_);
    }

    return log_binom_coef + log_p_term + log_1mp_term;
  }

  // Cumulative distribution function (CDF)
  //
  // Formula: P(X ≤ k) = Σ(i=0 to k) C(n,i) p^i (1-p)^(n-i)
  //
  // Uses regularized incomplete beta function for numerical stability.
  constexpr auto cdf(IntT k) const -> T {
    if (k < IntT{0}) {
      return T{0};
    }
    if (k >= n_) {
      return T{1};
    }

    // CDF(k) = I_{1-p}(n-k, k+1) where I is regularized incomplete beta
    // For now, use direct summation (can optimize with beta function later)
    using std::exp;

    T sum = T{0};
    for (IntT i = IntT{0}; i <= k; ++i) {
      sum += exp(logProb(i));
    }
    return sum;
  }

  // Statistical properties

  // E[X] = np
  constexpr auto mean() const -> T { return static_cast<T>(n_) * p_; }

  // Var[X] = np(1-p)
  constexpr auto variance() const -> T {
    return static_cast<T>(n_) * p_ * (T{1} - p_);
  }

  // Parameter accessors

  // Number of trials
  constexpr auto n() const -> IntT { return n_; }

  // Success probability
  constexpr auto p() const -> T { return p_; }

 private:
  IntT n_;
  T p_;
};

}  // namespace tempura::bayes
