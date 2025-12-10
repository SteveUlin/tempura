#pragma once

#include <cassert>
#include <cmath>
#include <limits>
#include <random>
#include <type_traits>

namespace tempura::bayes {

// Bernoulli distribution: P(X=1) = p, P(X=0) = 1-p
//
// Models a single coin flip. The building block for binary random events.
template <typename T = double>
class Bernoulli {
  static_assert(!std::is_integral_v<T>, "Bernoulli requires a floating-point type");

 public:
  constexpr Bernoulli(T p) : p_{p} {
    assert(p >= T{0} && p <= T{1} && "requires 0 ≤ p ≤ 1");
  }

  // Inverse transform sampling without floating-point multiply per sample.
  // Instead of: u = rand/range; return u < p
  // We use:     return rand < p*range (integer comparison)
  template <std::uniform_random_bit_generator Generator>
  constexpr auto sample(Generator& g) -> bool {
    using R = typename Generator::result_type;
    constexpr R range = Generator::max() - Generator::min();
    R threshold = static_cast<R>(p_ * static_cast<T>(range));
    return (g() - Generator::min()) < threshold;
  }

  constexpr auto prob(bool x) const -> T { return x ? p_ : T{1} - p_; }

  // Log-space avoids underflow for tiny probabilities.
  // Edge cases: log(0) = -∞ when p=0 and x=true, or p=1 and x=false.
  constexpr auto logProb(bool x) const -> T {
    using std::log;
    constexpr T neg_inf = -std::numeric_limits<T>::infinity();
    if (x) return p_ == T{0} ? neg_inf : log(p_);
    return p_ == T{1} ? neg_inf : log(T{1} - p_);
  }

  // P(X ≤ x): for x=false, only X=0 satisfies, so P = 1-p
  //           for x=true, both X=0 and X=1 satisfy, so P = 1
  constexpr auto cdf(bool x) const -> T { return x ? T{1} : T{1} - p_; }

  constexpr auto mean() const -> T { return p_; }
  constexpr auto variance() const -> T { return p_ * (T{1} - p_); }
  constexpr auto p() const -> T { return p_; }

 private:
  T p_;
};

}  // namespace tempura::bayes
