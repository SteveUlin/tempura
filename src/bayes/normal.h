#pragma once

#include <cassert>
#include <cmath>
#include <numbers>
#include <random>
#include <type_traits>
#include <utility>

#include "bayes/numeric_traits.h"

namespace tempura::bayes {

// Normal (Gaussian) distribution N(μ, σ)
// p(x|μ, σ) = 1 / (σ√(2π)) exp(-((x - μ)² / (2σ²)))
//
// Intuition:
//   The bell curve - ubiquitous in nature due to the Central Limit Theorem.
//   Models sums of many independent random effects: measurement errors,
//   human heights, test scores, particle velocities. Characterized by its
//   mean (peak location) and standard deviation (spread).
//
template <typename T = double>
class Normal {
  static_assert(
      !std::is_integral_v<T>,
      "Normal is a continuous distribution - integer types are not supported. "
      "Use a floating-point type (float, double, long double) or a custom "
      "numeric type.");

 public:
  constexpr Normal(T mu, T sigma)
      : mu_{mu}, sigma_{sigma}, cached_value_{mu_} {
    assert(sigma > T{0} && "Normal distribution requires sigma > 0");
  }

  template <std::uniform_random_bit_generator Generator>
  constexpr auto sample(Generator& g) -> T {
    // Return cached value if available
    if (has_value_) {
      has_value_ = false;
      return std::exchange(cached_value_, mu_);
    }

    // Generate a pair of standard normal samples
    auto [z0, z1] = boxMuller(g);

    // Cache second sample, scaled to N(μ, σ)
    cached_value_ = mu_ + sigma_ * z1;
    has_value_ = true;

    // Return first sample, scaled to N(μ, σ)
    return mu_ + sigma_ * z0;
  }

  // Probability density function (PDF)
  //
  // Formula: p(x|μ,σ) = (1 / (σ√(2π))) exp(-(x-μ)² / (2σ²))
  //
  // Computes the standardized form using z-score: z = (x - μ) / σ
  // Then: p(x) = (1 / (σ√(2π))) exp(-z²/2)
  constexpr auto prob(T x) const -> T {
    using std::exp;
    using std::sqrt;
    // Standardize to z-score
    const T z = (x - mu_) / sigma_;
    const T two_pi = T{2} * T{std::numbers::pi};
    return exp(-z * z / T{2}) / (sigma_ * sqrt(two_pi));
  }

  // Log probability density (log PDF)
  //
  // Formula: log p(x|μ,σ) = -½((x-μ)/σ)² - log(σ) - ½log(2π)
  //
  // Computing in log-space avoids underflow for very small probabilities
  // (e.g., in the tails where x is many standard deviations from μ).
  // The three terms represent:
  //   1. -½z²: Gaussian kernel (quadratic penalty)
  //   2. -log(σ): Normalization by scale
  //   3. -½log(2π): Constant normalization factor
  constexpr auto logProb(T x) const -> T {
    using std::log;
    const T z = (x - mu_) / sigma_;
    const T two_pi = T{2} * T{std::numbers::pi};
    return -z * z / T{2} - log(sigma_) - T{0.5} * log(two_pi);
  }

  // Cumulative distribution function (CDF)
  //
  // Formula: Φ(x|μ,σ) = ½(1 + erf((x-μ)/(σ√2)))
  //
  // Uses the error function (erf) to compute the integral of the PDF from -∞ to x.
  // The erf transforms the Gaussian integral into a standard form:
  //   erf(z) = (2/√π) ∫₀ᶻ exp(-t²) dt
  constexpr auto cdf(T x) const -> T {
    using std::erf;
    const T z = (x - mu_) / (sigma_ * T{std::numbers::sqrt2});
    return T{0.5} * (T{1} + erf(z));
  }

  // Statistical properties

  // E[X] = μ
  constexpr auto mean() const -> T { return mu_; }

  // Var[X] = σ²
  constexpr auto variance() const -> T { return sigma_ * sigma_; }

  // σ = √Var[X]
  constexpr auto stddev() const -> T { return sigma_; }

  // Parameter accessors

  // Location parameter
  constexpr auto mu() const -> T { return mu_; }

  // Scale parameter
  constexpr auto sigma() const -> T { return sigma_; }

 private:
  T mu_;
  T sigma_;

  T cached_value_;
  bool has_value_ = false;

  // Box-Muller transform: converts uniform samples to Gaussian samples
  //
  // Generates two independent N(0,1) samples from two uniform U(0,1) samples:
  //   Z₀ = √(-2 ln U₁) cos(2π U₂)
  //   Z₁ = √(-2 ln U₁) sin(2π U₂)
  //
  // Why it works: In 2D, independent normal variables X,Y have polar coordinates
  // where R² ~ Exponential(1/2) and Θ ~ Uniform(0, 2π). The transform inverts
  // this: R² = -2 ln U₁ generates the exponential variate, and Θ = 2π U₂
  // generates the uniform angle. Converting back to Cartesian yields normals.
  template <std::uniform_random_bit_generator Generator>
  constexpr auto boxMuller(Generator& g) -> std::pair<T, T> {
    // Generate two uniform samples in (0, 1)
    // Avoid integer division - cast both numerator and denominator to T
    constexpr auto range = static_cast<T>(Generator::max() - Generator::min());

    auto bits = g();
    while (bits == T{0}) [[unlikely]] {
      // Reject 0 to avoid log(0)
      bits = g();
    }
    T u1 = static_cast<T>(bits) / range;
    T u2 = static_cast<T>(g()) / range;

    // Apply Box-Muller transform
    using std::cos;
    using std::log;
    using std::sin;
    using std::sqrt;

    // Radial component
    const T r = sqrt(-T{2} * log(u1));

    // Angular component
    const T theta = T{2} * T{std::numbers::pi} * u2;

    // Convert from polar to Cartesian to get two independent normals
    return {r * sin(theta), r * cos(theta)};
  }
};

}  // namespace tempura::bayes
