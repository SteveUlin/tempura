#pragma once

#include <cassert>
#include <cmath>
#include <random>
#include <type_traits>

#include "bayes/numeric_traits.h"

namespace tempura::bayes {

// Bernoulli distribution Bernoulli(p)
// P(X = 1) = p, P(X = 0) = 1 - p
//
// Intuition:
//   Models a single coin flip with probability p of heads (true/1). The
//   fundamental building block for binary random events: success/failure,
//   yes/no, true/false. Examples include a single trial in A/B testing,
//   whether a customer converts, whether a component fails, or any binary
//   outcome with known success probability.
//
// Compared to Binomial(n, p):
//   - Bernoulli is a special case: Bernoulli(p) = Binomial(1, p)
//   - Binomial models n independent Bernoulli trials
//   - Sum of n i.i.d. Bernoulli(p) variables is Binomial(n, p)
//
template <typename T = double>
class Bernoulli {
  static_assert(
      !std::is_integral_v<T>,
      "Bernoulli probability parameter must be a floating-point type. "
      "Integer types cannot represent probabilities in [0,1]. "
      "Use a floating-point type (float, double, long double) or a custom "
      "numeric type.");

 public:
  constexpr Bernoulli(T p) : p_{p} {
    assert(p >= T{0} && p <= T{1} &&
           "Bernoulli distribution requires 0 <= p <= 1");
  }

  // Inverse transform sampling
  //
  // Generates a uniform U(0,1) sample and returns true if U < p.
  // This correctly implements the Bernoulli distribution.
  template <std::uniform_random_bit_generator Generator>
  constexpr auto sample(Generator& g) -> bool {
    constexpr T scale =
        T{1} / static_cast<T>(Generator::max() - Generator::min());
    T u = static_cast<T>(g() - Generator::min()) * scale;
    return u < p_;
  }

  // Probability mass function (PMF)
  //
  // Formula: P(X = x) = p^x (1-p)^(1-x)
  constexpr auto prob(bool x) const -> T { return x ? p_ : T{1} - p_; }

  // Log probability mass (log PMF)
  //
  // Formula: log P(X = x) = x·log(p) + (1-x)·log(1-p)
  //
  // Computing in log-space avoids underflow for very small probabilities.
  constexpr auto logProb(bool x) const -> T {
    using std::log;
    if (x) {
      // Handle p = 0 edge case
      if (p_ == T{0}) {
        return -numeric_infinity(T{});
      }
      return log(p_);
    } else {
      // Handle p = 1 edge case
      if (p_ == T{1}) {
        return -numeric_infinity(T{});
      }
      return log(T{1} - p_);
    }
  }

  // Cumulative distribution function (CDF)
  //
  // Formula: P(X ≤ x) = {1-p if x=0, 1 if x=1}
  constexpr auto cdf(bool x) const -> T { return x ? T{1} : T{1} - p_; }

  // Statistical properties

  // E[X] = p
  constexpr auto mean() const -> T { return p_; }

  // Var[X] = p(1-p)
  constexpr auto variance() const -> T { return p_ * (T{1} - p_); }

  // Parameter accessor

  // Success probability
  constexpr auto p() const -> T { return p_; }

 private:
  T p_;
};

}  // namespace tempura::bayes
