#pragma once

#include "symbolic4/constants.h"
#include "symbolic4/operators.h"

// ============================================================================
// log_prob.h - Symbolic log-probability functions
// ============================================================================
//
// Free functions returning symbolic expressions for log-probabilities.
// Each distribution has 2_c variants:
//   - logXxx()             Full log-probability (for model comparison)
//   - unnormalizedLogXxx() Omits normalizing constants (for MCMC)
//
// Usage:
//   Symbol<struct X> x;
//   Symbol<struct Mu> mu;
//   Symbol<struct Sigma> sigma;
//   auto lp = logNormal(x, mu, sigma);  // Returns symbolic expression
//   auto grad = diff(lp, mu);           // Symbolic gradient
//
// ============================================================================

namespace tempura::symbolic4 {

// ============================================================================
// Normal distribution
// ============================================================================
// p(x|μ,σ) = (1/(σ√(2π))) exp(-(x-μ)²/(2σ²))
// log p = -½z² - log(σ) - ½log(2π)  where z = (x-μ)/σ

template <Symbolic X, Symbolic Mu, Symbolic Sigma>
constexpr auto logNormal(X x, Mu mu, Sigma sigma) {
  auto z = (x - mu) / sigma;
  // -0.5 * z² - log(σ) - 0.5 * log(2π)
  // log(√(2π)) ≈ 0.9189385332046727
  return Fraction<-1, 2>{} * z * z - log(sigma) - Constant<0.9189385332046727>{};
}

template <Symbolic X, Symbolic Mu, Symbolic Sigma>
constexpr auto unnormalizedLogNormal(X x, Mu mu, Sigma sigma) {
  auto z = (x - mu) / sigma;
  return Fraction<-1, 2>{} * z * z;
}

// ============================================================================
// Half-Normal distribution (x ≥ 0)
// ============================================================================
// p(x|σ) = (√(2/π)/σ) exp(-x²/(2σ²))
// log p = ½log(2/π) - log(σ) - x²/(2σ²)

template <Symbolic X, Symbolic Sigma>
constexpr auto logHalfNormal(X x, Sigma sigma) {
  // log(√(2/π)) ≈ 0.2257913526447274
  return Constant<0.2257913526447274>{} - log(sigma) -
         x * x / (2_c * sigma * sigma);
}

template <Symbolic X, Symbolic Sigma>
constexpr auto unnormalizedLogHalfNormal(X x, Sigma sigma) {
  return -x * x / (2_c * sigma * sigma);
}

// ============================================================================
// Beta distribution (x ∈ [0,1])
// ============================================================================
// p(x|α,β) = x^(α-1) (1-x)^(β-1) / B(α,β)
// log p = (α-1)log(x) + (β-1)log(1-x) - log B(α,β)
// log B(α,β) = lgamma(α) + lgamma(β) - lgamma(α+β)

template <Symbolic X, Symbolic Alpha, Symbolic Beta>
constexpr auto logBeta(X x, Alpha alpha, Beta beta) {
  // Full normalized log-probability
  auto kernel = (alpha - 1_c) * log(x) + (beta - 1_c) * log(1_c - x);
  auto log_normalizer = lgamma(alpha) + lgamma(beta) - lgamma(alpha + beta);
  return kernel - log_normalizer;
}

template <Symbolic X, Symbolic Alpha, Symbolic Beta>
constexpr auto unnormalizedLogBeta(X x, Alpha alpha, Beta beta) {
  return (alpha - 1_c) * log(x) + (beta - 1_c) * log(1_c - x);
}

// ============================================================================
// Gamma distribution (x > 0)
// ============================================================================
// p(x|α,β) = β^α x^(α-1) exp(-βx) / Γ(α)
// log p = α log(β) + (α-1) log(x) - βx - lgamma(α)

template <Symbolic X, Symbolic Alpha, Symbolic Beta>
constexpr auto logGamma(X x, Alpha alpha, Beta beta) {
  // Full normalized log-probability
  return alpha * log(beta) + (alpha - 1_c) * log(x) - beta * x - lgamma(alpha);
}

template <Symbolic X, Symbolic Alpha, Symbolic Beta>
constexpr auto unnormalizedLogGamma(X x, Alpha alpha, Beta beta) {
  return (alpha - 1_c) * log(x) - beta * x;
}

// ============================================================================
// Exponential distribution (x ≥ 0)
// ============================================================================
// p(x|λ) = λ exp(-λx)
// log p = log(λ) - λx

template <Symbolic X, Symbolic Lambda>
constexpr auto logExponential(X x, Lambda lambda) {
  return log(lambda) - lambda * x;
}

template <Symbolic X, Symbolic Lambda>
constexpr auto unnormalizedLogExponential(X x, Lambda lambda) {
  return -lambda * x;
}

// ============================================================================
// Uniform distribution (x ∈ [a,b])
// ============================================================================
// p(x|a,b) = 1/(b-a)
// log p = -log(b-a)

template <Symbolic X, Symbolic A, Symbolic B>
constexpr auto logUniform([[maybe_unused]] X x, A a, B b) {
  return -log(b - a);
}

template <Symbolic X, Symbolic A, Symbolic B>
constexpr auto unnormalizedLogUniform([[maybe_unused]] X x,
                                      [[maybe_unused]] A a,
                                      [[maybe_unused]] B b) {
  return 0_c;
}

// ============================================================================
// Cauchy distribution
// ============================================================================
// p(x|x₀,γ) = 1/(πγ(1 + ((x-x₀)/γ)²))
// log p = -log(π) - log(γ) - log(1 + z²)  where z = (x-x₀)/γ

template <Symbolic X, Symbolic X0, Symbolic Gamma>
constexpr auto logCauchy(X x, X0 x0, Gamma gamma) {
  auto z = (x - x0) / gamma;
  // log(π) ≈ 1.1447298858494002
  return -Constant<1.1447298858494002>{} - log(gamma) - log(1_c + z * z);
}

template <Symbolic X, Symbolic X0, Symbolic Gamma>
constexpr auto unnormalizedLogCauchy(X x, X0 x0, Gamma gamma) {
  auto z = (x - x0) / gamma;
  return -log(1_c + z * z);
}

// ============================================================================
// Bernoulli distribution (x ∈ {0, 1})
// ============================================================================
// p(x|p) = p^x (1-p)^(1-x)
// log p = x log(p) + (1-x) log(1-p)

template <Symbolic X, Symbolic P>
constexpr auto logBernoulli(X x, P p) {
  return x * log(p) + (1_c - x) * log(1_c - p);
}

// Bernoulli has no normalizing constant
template <Symbolic X, Symbolic P>
constexpr auto unnormalizedLogBernoulli(X x, P p) {
  return logBernoulli(x, p);
}

// ============================================================================
// Binomial distribution (k ∈ {0, 1, ..., n})
// ============================================================================
// p(k|n,p) = C(n,k) p^k (1-p)^(n-k)
// log p = log C(n,k) + k log(p) + (n-k) log(1-p)
// log C(n,k) = lgamma(n+1) - lgamma(k+1) - lgamma(n-k+1)

template <Symbolic K, Symbolic N, Symbolic P>
constexpr auto logBinomial(K k, N n, P p) {
  // Full normalized log-probability
  auto log_comb = lgamma(n + 1_c) - lgamma(k + 1_c) - lgamma(n - k + 1_c);
  return log_comb + k * log(p) + (n - k) * log(1_c - p);
}

template <Symbolic K, Symbolic N, Symbolic P>
constexpr auto unnormalizedLogBinomial(K k, N n, P p) {
  // For MCMC, we can drop log C(n,k) when k and n are fixed observations
  return k * log(p) + (n - k) * log(1_c - p);
}

// ============================================================================
// Poisson distribution (x ∈ {0, 1, 2, ...})
// ============================================================================
// p(x|λ) = λ^x exp(-λ) / x!
// log p = x log(λ) - λ - lgamma(x+1)

template <Symbolic X, Symbolic Lambda>
constexpr auto logPoisson(X x, Lambda lambda) {
  // Full normalized log-probability
  // log(x!) = lgamma(x+1)
  return x * log(lambda) - lambda - lgamma(x + 1_c);
}

template <Symbolic X, Symbolic Lambda>
constexpr auto unnormalizedLogPoisson(X x, Lambda lambda) {
  return x * log(lambda) - lambda;
}

// ============================================================================
// Student's t distribution
// ============================================================================
// p(x|ν,μ,σ) = Γ((ν+1)/2) / (Γ(ν/2) √(νπ) σ) * (1 + z²/ν)^(-(ν+1)/2)
// log p = lgamma((ν+1)/2) - lgamma(ν/2) - 0.5*log(νπ) - log(σ) - (ν+1)/2 * log(1 + z²/ν)

template <Symbolic X, Symbolic Nu, Symbolic Mu, Symbolic Sigma>
constexpr auto logStudentT(X x, Nu nu, Mu mu, Sigma sigma) {
  auto z = (x - mu) / sigma;
  auto nu_plus_1_half = (nu + 1_c) / 2_c;
  auto nu_half = nu / 2_c;
  // log(π) ≈ 1.1447298858494002
  auto log_normalizer = lgamma(nu_plus_1_half) - lgamma(nu_half) -
                        Fraction<1, 2>{} * log(nu * Constant<3.14159265358979323846>{}) -
                        log(sigma);
  return log_normalizer - nu_plus_1_half * log(1_c + z * z / nu);
}

template <Symbolic X, Symbolic Nu, Symbolic Mu, Symbolic Sigma>
constexpr auto unnormalizedLogStudentT(X x, Nu nu, Mu mu, Sigma sigma) {
  auto z = (x - mu) / sigma;
  return -(nu + 1_c) / 2_c * log(1_c + z * z / nu);
}

// ============================================================================
// Categorical distribution (x ∈ {0, 1, ..., K-1})
// ============================================================================
// p(x|π) = π[x]
// log p(x|π) = log(π[x])
//
// For symbolic use, we represent as sum: x_k * log(π_k) for 1_c-hot x

template <Symbolic X, Symbolic LogP>
constexpr auto logCategorical(X x, LogP log_p) {
  // x is the category index (as probability/1_c-hot encoding)
  // log_p is log(probability) for that category
  // log p(x=k) = log(π_k), so for 1_c-hot: sum over k of x_k * log(π_k)
  return x * log_p;
}

// ============================================================================
// Dirichlet distribution (x ∈ simplex)
// ============================================================================
// p(x|α) ∝ ∏ x_k^(α_k - 1)
// log p(x|α) = Σ (α_k - 1) log(x_k) + const
// Note: Normalizing constant involves lgamma, so only unnormalized provided

template <Symbolic X, Symbolic Alpha>
constexpr auto unnormalizedLogDirichlet(X x, Alpha alpha) {
  // For a single component: (α - 1) * log(x)
  return (alpha - 1_c) * log(x);
}

}  // namespace tempura::symbolic4
