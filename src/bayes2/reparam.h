#pragma once

#include <cmath>
#include <concepts>
#include <numbers>

#include "symbolic3/constants.h"
#include "symbolic3/core.h"
#include "symbolic3/operators.h"

// Reparameterization transforms for differentiable sampling.
//
// The reparameterization trick enables gradients to flow through samples by
// expressing a sample as a deterministic transform of parameters and noise:
//
//   z ~ Normal(μ, σ)  →  ε ~ Normal(0, 1), z = μ + σ * ε
//
// Three input modes:
//   1. Symbolic args → symbolic expression (for compile-time autodiff)
//   2. Numeric args → numeric value (floating point, Dual, etc.)
//
// Example:
//   // Symbolic - returns Expression type for autodiff
//   constexpr Symbol mu, sigma, epsilon;
//   auto z_expr = reparamNormal(mu, sigma, epsilon);
//   auto dz_dmu = diff(z_expr, mu);     // = 1
//   auto dz_dsigma = diff(z_expr, sigma); // = epsilon
//
//   // Concrete - returns double
//   double z = reparamNormal(2.0, 1.5, 0.3);  // = 2.0 + 1.5 * 0.3 = 2.45
//
//   // Dual numbers - returns dual for forward-mode autodiff
//   Dual<double> z = reparamNormal(Dual{2.0, 1.0}, Dual{1.5, 0.0}, Dual{0.3, 0.0});

namespace tempura::bayes2 {

// Numeric concept - types that support arithmetic (floating point, Dual, etc.)
template <typename T>
concept Numeric = requires(T a, T b) {
  { a + b } -> std::convertible_to<T>;
  { a - b } -> std::convertible_to<T>;
  { a * b } -> std::convertible_to<T>;
  { a / b } -> std::convertible_to<T>;
} && !symbolic3::Symbolic<T>;

// =============================================================================
// Normal Distribution: z = μ + σ * ε, where ε ~ N(0, 1)
// =============================================================================
//
// Works for both Numeric (double, Dual) and Symbolic types since they share
// arithmetic operators. Template deduction handles return type automatically.

template <typename Mu, typename Sigma, typename Eps>
constexpr auto reparamNormal(Mu mu, Sigma sigma, Eps epsilon) {
  return mu + sigma * epsilon;
}

// =============================================================================
// Half-Normal Distribution: z = σ * ε, where ε ~ HalfNormal(0, 1)
// =============================================================================
//
// The base sample ε should already be from a half-normal (|N(0,1)|).
// At runtime, sample from N(0,1) and take abs. For symbolic differentiation,
// we assume ε > 0 so the transform is simply z = σ * ε.

template <typename Sigma, typename Eps>
constexpr auto reparamHalfNormal(Sigma sigma, Eps epsilon) {
  return sigma * epsilon;
}

// =============================================================================
// Exponential Distribution: x = -log(u) / λ, where u ~ Uniform(0, 1)
// =============================================================================
//
// Inverse CDF method. Gradient w.r.t. λ: ∂x/∂λ = log(u) / λ²

template <Numeric T>
constexpr auto reparamExponential(T lambda, T uniform_sample) -> T {
  using std::log;
  return -log(uniform_sample) / lambda;
}

template <symbolic3::Symbolic Lambda, symbolic3::Symbolic U>
constexpr auto reparamExponential(Lambda lambda, U u) {
  using namespace symbolic3;
  return -log(u) / lambda;
}

// =============================================================================
// Uniform Distribution: x = a + (b - a) * u, where u ~ Uniform(0, 1)
// =============================================================================

template <typename A, typename B, typename U>
constexpr auto reparamUniform(A a, B b, U u) {
  return a + (b - a) * u;
}

// =============================================================================
// Cauchy Distribution: x = x₀ + γ * tan(π * (u - 0.5)), where u ~ Uniform(0, 1)
// =============================================================================
//
// Inverse CDF method using the Cauchy quantile function.

template <Numeric T>
constexpr auto reparamCauchy(T location, T scale, T uniform_sample) -> T {
  using std::tan;
  return location +
         scale * tan(T{std::numbers::pi} * (uniform_sample - T{0.5}));
}

template <symbolic3::Symbolic Loc, symbolic3::Symbolic Scale, symbolic3::Symbolic U>
constexpr auto reparamCauchy(Loc location, Scale scale, U u) {
  using namespace symbolic3;
  return location + scale * tan(pi * (u - Fraction<1, 2>{}));
}

// =============================================================================
// Gamma Distribution (shape α ≥ 1): Rejection-free via Marsaglia-Tsang
// =============================================================================
//
// For Gamma(α, β) with α ≥ 1, we use a transformation that requires
// both a normal and uniform sample. The transform is:
//
//   d = α - 1/3
//   c = 1 / sqrt(9d)
//   v = (1 + c * z)³  where z ~ N(0,1)
//   x = d * v / β
//
// This is differentiable w.r.t. α (via d, c) and β.
// Note: For α < 1, use the boost: Gamma(α) = Gamma(α+1) * U^(1/α)

template <Numeric T>
constexpr auto reparamGamma(T alpha, T beta, T normal_sample, T uniform_sample)
    -> T {
  using std::sqrt;
  // Marsaglia-Tsang method for α ≥ 1
  T d = alpha - T{1.0 / 3.0};
  T c = T{1} / sqrt(T{9} * d);
  T z = normal_sample;

  // v = (1 + c*z)³, must be positive (rejection would occur if z < -1/c)
  // For reparameterization, we assume the sample is valid
  T v = T{1} + c * z;
  v = v * v * v;

  // Check acceptance condition (for gradient correctness, we assume accepted)
  // In practice, sampling code would reject and resample if:
  //   v ≤ 0 or log(u) ≥ 0.5*z² + d - d*v + d*log(v)
  (void)uniform_sample;  // Used for rejection in actual sampling

  return d * v / beta;
}

// Symbolic version - same transform, symbolic expressions
template <symbolic3::Symbolic Alpha, symbolic3::Symbolic Beta,
          symbolic3::Symbolic Z, symbolic3::Symbolic U>
constexpr auto reparamGamma(Alpha alpha, Beta beta, Z z, U u) {
  using namespace symbolic3;
  (void)u;  // Rejection check not expressible symbolically

  auto d = alpha - Fraction<1, 3>{};
  auto c = Fraction<1, 3>{} / sqrt(d);  // 1/sqrt(9d) = 1/(3*sqrt(d))
  auto v_base = Constant<1>{} + c * z;
  auto v = v_base * v_base * v_base;

  return d * v / beta;
}

// =============================================================================
// Beta Distribution: via ratio of Gammas
// =============================================================================
//
// Beta(α, β) = Gamma(α, 1) / (Gamma(α, 1) + Gamma(β, 1))
//
// This composes the Gamma reparameterization. Each Gamma needs its own
// normal and uniform samples. Alpha/beta parameters are unused here since
// the gamma samples are pre-reparameterized.

template <typename Alpha, typename Beta, typename G1, typename G2>
constexpr auto reparamBeta(Alpha /* alpha */, Beta /* beta */, G1 g1, G2 g2) {
  return g1 / (g1 + g2);
}

}  // namespace tempura::bayes2
