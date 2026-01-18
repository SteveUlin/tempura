#pragma once

#include <cmath>
#include <concepts>
#include <numbers>

#include "symbolic3/constants.h"
#include "symbolic3/core.h"
#include "symbolic3/operators.h"

// Generic log-probability functions that work with both symbolic and numeric types.
//
// When called with symbolic arguments, these return symbolic expressions.
// When called with concrete arguments, these return numeric values.
//
// Example:
//   // Symbolic - returns an Expression type
//   constexpr Symbol x, μ, σ;
//   auto expr = logNormal(x, μ, σ);  // Expression<AddOp, ...>
//
//   // Concrete - returns double
//   double lp = logNormal(1.5, 0.0, 1.0);  // -1.4189...

namespace tempura::prob {

// =============================================================================
// Constants
// =============================================================================

inline constexpr double kLog2Pi = 1.8378770664093453;      // log(2π)
inline constexpr double kLogSqrt2Pi = 0.9189385332046727;  // log(√(2π))
inline constexpr double kLog2 = 0.6931471805599453;        // log(2)

// =============================================================================
// Normal Distribution
// =============================================================================
//
// log p(x | μ, σ) = -½log(2π) - log(σ) - (x-μ)²/(2σ²)

// Concrete implementation
template <std::floating_point T>
constexpr auto logNormal(T x, T μ, T σ) -> T {
  T z = (x - μ) / σ;
  return T{-0.5} * z * z - std::log(σ) - T{kLogSqrt2Pi};
}

// Symbolic implementation
template <symbolic3::Symbolic X, symbolic3::Symbolic Mu, symbolic3::Symbolic Sigma>
constexpr auto logNormal(X x, Mu μ, Sigma σ) {
  using namespace symbolic3;
  auto z = (x - μ) / σ;
  return Fraction<-1, 2>{} * z * z - log(σ) - logSqrt2Pi;
}

// =============================================================================
// Half-Normal Distribution (x ≥ 0)
// =============================================================================
//
// log p(x | σ) = log(2) - ½log(2π) - log(σ) - x²/(2σ²)
//
// This is a Normal(0, σ) truncated to positive values, with density doubled.

template <std::floating_point T>
constexpr auto logHalfNormal(T x, T σ) -> T {
  T z = x / σ;
  return T{kLog2} - T{kLogSqrt2Pi} - std::log(σ) - T{0.5} * z * z;
}

template <symbolic3::Symbolic X, symbolic3::Symbolic Sigma>
constexpr auto logHalfNormal(X x, Sigma σ) {
  using namespace symbolic3;
  auto z = x / σ;
  return log2 - logSqrt2Pi - log(σ) - Fraction<1, 2>{} * z * z;
}

// =============================================================================
// Exponential Distribution
// =============================================================================
//
// log p(x | λ) = log(λ) - λx

template <std::floating_point T>
constexpr auto logExponential(T x, T λ) -> T {
  return std::log(λ) - λ * x;
}

template <symbolic3::Symbolic X, symbolic3::Symbolic Lambda>
constexpr auto logExponential(X x, Lambda λ) {
  using namespace symbolic3;
  return log(λ) - λ * x;
}

// =============================================================================
// Beta Distribution
// =============================================================================
//
// log p(x | α, β) = (α-1)log(x) + (β-1)log(1-x) - logB(α,β)
//
// where logB(α,β) = logΓ(α) + logΓ(β) - logΓ(α+β)

template <std::floating_point T>
constexpr auto logBeta(T x, T α, T β) -> T {
  T log_beta_fn = std::lgamma(α) + std::lgamma(β) - std::lgamma(α + β);
  return (α - T{1}) * std::log(x) + (β - T{1}) * std::log(T{1} - x) - log_beta_fn;
}

template <symbolic3::Symbolic X, symbolic3::Symbolic Alpha, symbolic3::Symbolic Beta>
constexpr auto logBeta(X x, Alpha α, Beta β) {
  using namespace symbolic3;
  return (α - 1_c) * log(x) + (β - 1_c) * log(1_c - x) - logBetaFn(α, β);
}

// =============================================================================
// Gamma Distribution
// =============================================================================
//
// log p(x | α, β) = α·log(β) + (α-1)log(x) - βx - logΓ(α)
//
// Parameterized as shape α and rate β (not scale).

template <std::floating_point T>
constexpr auto logGamma(T x, T α, T β) -> T {
  return α * std::log(β) + (α - T{1}) * std::log(x) - β * x - std::lgamma(α);
}

template <symbolic3::Symbolic X, symbolic3::Symbolic Alpha, symbolic3::Symbolic Beta>
constexpr auto logGamma(X x, Alpha α, Beta β) {
  using namespace symbolic3;
  return α * log(β) + (α - 1_c) * log(x) - β * x - logGammaFn(α);
}

// =============================================================================
// Bernoulli Distribution
// =============================================================================
//
// log p(x | p) = x·log(p) + (1-x)·log(1-p)
//
// x ∈ {0, 1}

template <std::floating_point T>
constexpr auto logBernoulli(T x, T p) -> T {
  return x * std::log(p) + (T{1} - x) * std::log(T{1} - p);
}

template <symbolic3::Symbolic X, symbolic3::Symbolic P>
constexpr auto logBernoulli(X x, P p) {
  using namespace symbolic3;
  return x * log(p) + (1_c - x) * log(1_c - p);
}

// =============================================================================
// Uniform Distribution
// =============================================================================
//
// log p(x | a, b) = -log(b - a)  for a ≤ x ≤ b

template <std::floating_point T>
constexpr auto logUniform(T x, T a, T b) -> T {
  (void)x;  // Uniform density doesn't depend on x (within bounds)
  return -std::log(b - a);
}

template <symbolic3::Symbolic X, symbolic3::Symbolic A, symbolic3::Symbolic B>
constexpr auto logUniform(X x, A a, B b) {
  using namespace symbolic3;
  (void)x;
  return -log(b - a);
}

// =============================================================================
// Cauchy Distribution
// =============================================================================
//
// log p(x | x₀, γ) = -log(π) - log(γ) - log(1 + ((x-x₀)/γ)²)

template <std::floating_point T>
constexpr auto logCauchy(T x, T x0, T γ) -> T {
  T z = (x - x0) / γ;
  return -std::log(T{std::numbers::pi}) - std::log(γ) - std::log(T{1} + z * z);
}

template <symbolic3::Symbolic X, symbolic3::Symbolic X0, symbolic3::Symbolic Gamma>
constexpr auto logCauchy(X x, X0 x0, Gamma γ) {
  using namespace symbolic3;
  auto z = (x - x0) / γ;
  return -logPi - log(γ) - log(1_c + z * z);
}

}  // namespace tempura::prob
