#pragma once

#include <cassert>
#include <cmath>
#include <numbers>
#include <random>
#include <type_traits>

#include "bayes/gamma.h"
#include "bayes/normal.h"
#include "bayes/numeric_traits.h"
#include "special/gamma.h"

namespace tempura::bayes {

// Student's t-distribution: p(x|ν) ∝ (1 + x²/ν)^(-(ν+1)/2)
//
// Bell-shaped with heavier tails than normal. As ν → ∞, approaches N(0,1).
// ν = 1 is Cauchy distribution.
template <typename T = double>
class StudentT {
  static_assert(!std::is_integral_v<T>, "StudentT requires a floating-point type");

 public:
  constexpr StudentT(T ν)
      : ν_{ν},
        log_normalizer_{[&] {
          using std::lgamma;
          using std::log;
          return lgamma((ν + T{1}) / T{2}) - lgamma(ν / T{2}) -
                 T{0.5} * log(ν * T{std::numbers::pi});
        }()} {
    assert(ν > T{0} && "requires ν > 0");
  }

  // If Z ~ N(0,1) and V ~ χ²(ν), then T = Z / √(V/ν) ~ t(ν)
  template <std::uniform_random_bit_generator Generator>
  constexpr auto sample(Generator& g) -> T {
    using std::sqrt;
    T z = Normal<T>{T{0}, T{1}}.sample(g);
    T chi_sq = Gamma<T>{ν_ / T{2}, T{0.5}}.sample(g);
    return z / sqrt(chi_sq / ν_);
  }

  constexpr auto prob(T x) const -> T {
    using std::exp;
    return exp(logProb(x));
  }

  constexpr auto logProb(T x) const -> T {
    using std::log;
    return log_normalizer_ - ((ν_ + T{1}) / T{2}) * log(T{1} + x * x / ν_);
  }

  // CDF via incomplete beta: F(x) = 1 - ½ I_{ν/(ν+x²)}(ν/2, ½) for x ≥ 0
  constexpr auto cdf(T x) const -> T {
    if (x == T{0}) return T{0.5};

    T x_sq = x * x;
    T t = ν_ / (ν_ + x_sq);
    T tail = special::incompleteBeta(ν_ / T{2}, T{0.5}, t);

    return x > T{0} ? T{1} - tail / T{2} : tail / T{2};
  }

  // E[X] = 0 for ν > 1, undefined for ν ≤ 1
  constexpr auto mean() const -> T {
    return ν_ <= T{1} ? numeric_quiet_nan(T{}) : T{0};
  }

  // Var[X] = ν/(ν-2) for ν > 2, ∞ for 1 < ν ≤ 2, undefined for ν ≤ 1
  constexpr auto variance() const -> T {
    if (ν_ <= T{1}) return numeric_quiet_nan(T{});
    if (ν_ <= T{2}) return numeric_infinity(T{});
    return ν_ / (ν_ - T{2});
  }

  constexpr auto nu() const -> T { return ν_; }

 private:
  T ν_;
  T log_normalizer_;  // log Γ((ν+1)/2) - log Γ(ν/2) - ½ log(νπ)
};

}  // namespace tempura::bayes
