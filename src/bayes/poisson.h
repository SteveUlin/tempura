#pragma once

#include <cassert>
#include <cmath>
#include <cstdint>
#include <random>
#include <type_traits>

#include "bayes/normal.h"
#include "bayes/numeric_traits.h"
#include "special/gamma.h"

namespace tempura::bayes {

// Poisson distribution: P(X=k) = λ^k e^(-λ) / k! for k ∈ {0,1,2,...}
//
// Models count of events in a fixed interval when events occur independently
// at constant rate λ. Mean = variance = λ.
template <typename T = double, typename IntT = int64_t>
class Poisson {
  static_assert(!std::is_integral_v<T>, "Poisson requires a floating-point type");
  static_assert(std::is_integral_v<IntT>, "Poisson count type must be integral");

 public:
  constexpr Poisson(T λ) : λ_{λ}, exp_neg_λ_{[&] {
                             using std::exp;
                             return exp(-λ);
                           }()} {
    assert(λ > T{0} && "requires λ > 0");
  }

  // Knuth's algorithm for λ < 30, PTRD (rejection sampling) for large λ
  template <std::uniform_random_bit_generator Generator>
  constexpr auto sample(Generator& g) -> IntT {
    if (λ_ < T{30}) {
      return sampleKnuth(g);
    }
    return samplePTRD(g);
  }

  constexpr auto prob(IntT k) const -> T {
    if (k < IntT{0}) {
      return T{0};
    }

    using std::exp;
    return exp(logProb(k));
  }

  // Log-space avoids overflow for large λ or k
  constexpr auto logProb(IntT k) const -> T {
    if (k < IntT{0}) {
      return -numeric_infinity(T{});
    }

    using std::lgamma;
    using std::log;

    return static_cast<T>(k) * log(λ_) - λ_ - lgamma(static_cast<T>(k + IntT{1}));
  }

  // CDF via regularized upper incomplete gamma: P(X ≤ k) = Q(k+1, λ)
  constexpr auto cdf(IntT k) const -> T {
    if (k < IntT{0}) {
      return T{0};
    }

    return T{1} - special::incompleteGamma(static_cast<T>(k + IntT{1}), λ_);
  }

  constexpr auto mean() const -> T { return λ_; }
  constexpr auto variance() const -> T { return λ_; }
  constexpr auto lambda() const -> T { return λ_; }

 private:
  T λ_;
  T exp_neg_λ_;

  // Knuth: generate uniforms until product < e^(-λ)
  template <std::uniform_random_bit_generator Generator>
  constexpr auto sampleKnuth(Generator& g) -> IntT {
    constexpr T scale = T{1} / static_cast<T>(Generator::max() - Generator::min());

    IntT k = IntT{0};
    T p = T{1};

    do {
      ++k;
      T u = static_cast<T>(g() - Generator::min()) * scale;
      if (u == T{0}) [[unlikely]] {
        u = scale;  // Avoid p → 0 permanently
      }
      p *= u;
    } while (p > exp_neg_λ_);

    return k - IntT{1};
  }

  // PTRD: transformed rejection with normal approximation
  template <std::uniform_random_bit_generator Generator>
  constexpr auto samplePTRD(Generator& g) -> IntT {
    using std::floor;
    using std::log;
    using std::sqrt;

    constexpr T scale = T{1} / static_cast<T>(Generator::max() - Generator::min());

    const T sqrt_λ = sqrt(λ_);
    const T b = T{0.931} + T{2.53} * sqrt_λ;
    const T a = T{-0.059} + T{0.02483} * b;
    const T inv_α = T{1.1239} + T{1.1328} / (b - T{3.4});
    const T v_r = T{0.9277} - T{3.6224} / (b - T{2});

    while (true) {
      T u = static_cast<T>(g() - Generator::min()) * scale - T{0.5};
      T v = static_cast<T>(g() - Generator::min()) * scale;

      T us = T{0.5} - (u < T{0} ? -u : u);
      IntT k = static_cast<IntT>(floor((T{2} * a / us + b) * u + λ_ + T{0.43}));

      if (k < IntT{0}) {
        continue;
      }

      // Quick acceptance
      if (us >= T{0.07} && v <= v_r) {
        return k;
      }

      // Quick rejection
      if (us < T{0.013} && v > us) {
        continue;
      }

      // Full acceptance test
      T log_accept =
          log(v * inv_α / (a / (us * us) + b)) -
          (λ_ - static_cast<T>(k) + static_cast<T>(k) * log(λ_) -
           special::logFactorial(k));

      if (log_accept <= T{0}) {
        return k;
      }
    }
  }
};

}  // namespace tempura::bayes
