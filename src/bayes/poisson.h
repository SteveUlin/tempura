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

// Poisson distribution: Poisson(λ)
//
// Intuition:
//   Models the number of events occurring in a fixed interval when events happen
//   at a constant average rate and independently of each other. Common in:
//   - Call center arrivals per hour
//   - Website visits per minute
//   - Radioactive decay counts
//   - Typos per page
//
// PMF: P(X = k) = λ^k e^(-λ) / k!
// Support: k ∈ {0, 1, 2, ...}
// Parameter: λ > 0 (rate/mean)
//
// Relation to other distributions:
//   - Binomial(n, p) → Poisson(np) as n → ∞, p → 0, np → λ
//   - Sum of independent Poisson(λᵢ) is Poisson(Σλᵢ)
//   - Inter-arrival times are Exponential(λ)
//
template <typename T = double, typename IntT = int64_t>
class Poisson {
  static_assert(
      !std::is_integral_v<T>,
      "Poisson rate parameter must be a floating-point type. "
      "Integer types cannot represent rates accurately. "
      "Use a floating-point type (float, double, long double) or a custom "
      "numeric type.");

  static_assert(std::is_integral_v<IntT>,
                "Poisson count type must be an integer type (int, int64_t, etc.).");

 public:
  constexpr Poisson(T λ) : λ_{λ}, exp_neg_λ_{[&] {
                             using std::exp;
                             return exp(-λ);
                           }()} {
    assert(λ > T{0} && "Poisson distribution requires λ > 0");
  }

  // Sampling using Knuth's algorithm for small λ, PTRD for large λ
  //
  // For small λ (< 30): Use inverse transform via product of uniforms
  //   Generate U₁, U₂, ... until ∏Uᵢ < e^(-λ); return count - 1
  //   This simulates inter-arrival times of a Poisson process.
  //
  // For large λ (≥ 30): Use transformed rejection with squeeze (PTRD)
  //   Normal approximation with acceptance-rejection refinement.
  //   More efficient as direct method becomes O(λ) per sample.
  template <std::uniform_random_bit_generator Generator>
  constexpr auto sample(Generator& g) -> IntT {
    if (λ_ < T{30}) {
      return sampleKnuth(g);
    }
    return samplePTRD(g);
  }

  // Probability mass function (PMF)
  //
  // Formula: P(X = k) = λ^k e^(-λ) / k!
  constexpr auto prob(IntT k) const -> T {
    if (k < IntT{0}) {
      return T{0};
    }

    using std::exp;
    return exp(logProb(k));
  }

  // Log probability mass (log PMF)
  //
  // Formula: log P(X = k) = k·log(λ) - λ - log(k!)
  //
  // Computing in log-space avoids overflow for large λ or k.
  constexpr auto logProb(IntT k) const -> T {
    if (k < IntT{0}) {
      return -numeric_infinity(T{});
    }

    using std::lgamma;
    using std::log;

    // log(k!) = lgamma(k + 1)
    return static_cast<T>(k) * log(λ_) - λ_ -
           lgamma(static_cast<T>(k + IntT{1}));
  }

  // Cumulative distribution function (CDF)
  //
  // Formula: P(X ≤ k) = Σ(i=0 to k) λ^i e^(-λ) / i!
  //
  // Uses regularized incomplete gamma: P(X ≤ k) = Q(k+1, λ) = 1 - P(k+1, λ)
  // where P(a, x) is the regularized lower incomplete gamma function.
  constexpr auto cdf(IntT k) const -> T {
    if (k < IntT{0}) {
      return T{0};
    }

    // CDF = Q(k+1, λ) = 1 - P(k+1, λ) = Γ(k+1, λ) / Γ(k+1)
    // Using regularized upper incomplete gamma
    return T{1} - special::incompleteGamma(static_cast<T>(k + IntT{1}), λ_);
  }

  // Statistical properties

  // E[X] = λ
  constexpr auto mean() const -> T { return λ_; }

  // Var[X] = λ (mean equals variance - a key Poisson property)
  constexpr auto variance() const -> T { return λ_; }

  // Parameter accessors

  // Rate parameter (also equals mean and variance)
  constexpr auto lambda() const -> T { return λ_; }

 private:
  T λ_;
  T exp_neg_λ_;  // Cached e^(-λ) for Knuth sampling

  // Knuth's algorithm: count steps until product of uniforms < e^(-λ)
  //
  // Intuition: Simulates a Poisson process by generating exponential
  // inter-arrival times. Count events until total time exceeds 1.
  template <std::uniform_random_bit_generator Generator>
  constexpr auto sampleKnuth(Generator& g) -> IntT {
    constexpr T scale = T{1} / static_cast<T>(Generator::max() - Generator::min());

    IntT k = IntT{0};
    T p = T{1};

    do {
      ++k;
      T u = static_cast<T>(g() - Generator::min()) * scale;
      // Avoid u = 0 which would make p = 0 permanently
      if (u == T{0}) [[unlikely]] {
        u = scale;  // Use smallest positive value
      }
      p *= u;
    } while (p > exp_neg_λ_);

    return k - IntT{1};
  }

  // PTRD: Transformed rejection with decomposition
  //
  // For large λ, Poisson is approximately Normal(λ, √λ).
  // Use normal proposal with acceptance-rejection for exactness.
  //
  // Reference: Hörmann, "The transformed rejection method for generating
  // Poisson random variables", Insurance: Mathematics and Economics, 1993.
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
