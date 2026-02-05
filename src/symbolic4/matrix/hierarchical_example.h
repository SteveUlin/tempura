#pragma once

#include "symbolic4/core.h"
#include "symbolic4/distributions/random_var.h"
#include "symbolic4/matrix/lkj.h"
#include "symbolic4/matrix/mvn.h"
#include "symbolic4/matrix/ops.h"
#include "symbolic4/matrix/types.h"
#include "symbolic4/model.h"
#include "symbolic4/operators.h"

// ============================================================================
// matrix/hierarchical_example.h - Stan-style varying slopes hierarchical model
// ============================================================================
//
// This file demonstrates how to build hierarchical models with correlated
// group-level coefficients using the matrix support in symbolic4. The model
// follows the Stan manual's recommended parameterization for efficiency.
//
// MODEL SPECIFICATION:
// --------------------
// Hierarchical model with J groups and K correlated coefficients per group.
//
// Data:
//   y[i]      ~ Likelihood(X[i] × β[group[i]])   for i = 1..N observations
//
// Group-level parameters:
//   β[j]      ~ MVN(γ, Σ)                        for j = 1..J groups
//
// Population-level hyperparameters:
//   γ         ~ MVN(0, σ_γ²I)                    (population mean coefficients)
//   Σ = diag(τ) × Ω × diag(τ)                   (scale-correlation decomposition)
//   τ[k]      ~ HalfNormal(σ_τ)                 (coefficient scales)
//   L_Ω       ~ LKJ(η)                          (Cholesky of correlation matrix)
//
// NON-CENTERED PARAMETERIZATION:
// ------------------------------
// The centered parameterization β[j] ~ MVN(γ, Σ) can mix poorly when:
//   - Groups have few observations (weak likelihood)
//   - Prior and likelihood operate on different scales
//
// The non-centered parameterization avoids this by reparameterizing:
//   z[j]      ~ MVN(0, I)                        (standard normal auxiliary)
//   β[j]      = γ + diag(τ) × L_Ω × z[j]        (deterministic transformation)
//
// This is equivalent to β[j] ~ MVN(γ, Σ) but samples z[j] in standardized space,
// then transforms to the β[j] space. The Jacobian is absorbed into the z prior.
//
// WHY THIS WORKS:
//   - z[j] has the same scale regardless of τ or Ω
//   - Gradient information flows through the transformation, not the prior
//   - HMC can take larger steps in the standardized space
//
// TYPICAL APPLICATIONS:
//   - Varying slopes regression (schools, hospitals, regions)
//   - Item response theory (students, test items)
//   - Longitudinal models (subjects with correlated random effects)
//   - Crossed random effects (subjects × items)
//
// ============================================================================

namespace tempura::symbolic4 {

// ============================================================================
// Dimension Tags
// ============================================================================
//
// Dimension tags identify the size of matrix dimensions at model construction
// time (similar to plate dimensions). They are empty types used purely for
// type-level identification.

// Groups: The j = 1..J groups (e.g., schools, hospitals, countries)
struct Groups;

// Predictors: The k = 1..K coefficients per group (e.g., intercept, slope)
struct Predictors;

// ============================================================================
// Model Components
// ============================================================================
//
// We build the model in stages, showing how each component contributes.

// ----------------------------------------------------------------------------
// 1. Population mean coefficients: γ ~ MVN(0, σ_γ²I)
// ----------------------------------------------------------------------------
//
// The population mean γ ∈ ℝᴷ represents the "average group" coefficients.
// We place an independent normal prior on each component.
//
// In symbolic4, we represent this as a DimVectorSymbol tagged with Predictors.

inline auto populationMean() {
  return dimVector<Predictors>();  // γ: K-dimensional vector
}

// ----------------------------------------------------------------------------
// 2. Coefficient scales: τ[k] ~ HalfNormal(σ_τ)
// ----------------------------------------------------------------------------
//
// The scale vector τ ∈ ℝ₊ᴷ controls how much coefficients vary across groups.
// Each τ[k] is the between-group standard deviation for coefficient k.
//
// HalfNormal(σ_τ) is a common weakly informative prior that:
//   - Keeps τ positive
//   - Has mode at 0 (shrinkage toward homogeneity)
//   - Has reasonable tail behavior for large deviations

inline auto coeffScales() {
  return dimVector<Predictors>();  // τ: K-dimensional positive vector
}

// ----------------------------------------------------------------------------
// 3. Correlation Cholesky factor: L_Ω ~ LKJ(η)
// ----------------------------------------------------------------------------
//
// The correlation structure Ω captures how coefficients co-vary across groups.
// We parameterize via the Cholesky factor L_Ω where Ω = L_Ω × L_Ωᵀ.
//
// The LKJ(η) prior on L_Ω:
//   η = 1: Uniform over correlation matrices
//   η = 2: Weakly informative, favors moderate correlations
//   η > 2: Strong shrinkage toward identity (independence)

inline auto correlationCholesky() {
  return cholCorr<Predictors>();  // L_Ω: K×K lower triangular, unit row norms
}

// ----------------------------------------------------------------------------
// 4. Standard normal auxiliary: z[j] ~ MVN(0, I)
// ----------------------------------------------------------------------------
//
// For each group j, we sample K standard normals. These will be transformed
// to get the actual group coefficients β[j].
//
// In a full implementation, this would be a plate over Groups:
//   plate<Groups>(dimVector<Predictors>())
//
// Each z[j] ∈ ℝᴷ is independent MVN(0, I).

inline auto standardNormal() {
  return dimVector<Predictors>();  // z[j]: K-dimensional standard normal
}

// ----------------------------------------------------------------------------
// 5. Group coefficients: β[j] = γ + diag(τ) × L_Ω × z[j]
// ----------------------------------------------------------------------------
//
// The deterministic transformation that maps (γ, τ, L_Ω, z[j]) → β[j].
//
// Breaking it down:
//   1. L_Ω × z[j]              - Introduces correlation structure
//   2. diag(τ) × (L_Ω × z[j])  - Scales by coefficient-specific variance
//   3. γ + ...                  - Shifts to population mean
//
// This is equivalent to: β[j] ~ MVN(γ, diag(τ) × L_Ω × L_Ωᵀ × diag(τ))
//                              = MVN(γ, diag(τ) × Ω × diag(τ))
//                              = MVN(γ, Σ)

// ============================================================================
// HierarchicalVaryingSlopes - Complete model specification
// ============================================================================
//
// This struct demonstrates the full API for defining a hierarchical model
// with correlated random effects. It shows how components compose together.

struct HierarchicalVaryingSlopes {
  // -------------------------------------------------------------------------
  // Hyperprior parameters (fixed)
  // -------------------------------------------------------------------------

  static constexpr double sigma_gamma = 5.0;  // Prior SD for population mean
  static constexpr double sigma_tau = 2.5;    // Prior SD for group-level scales
  static constexpr double eta = 2.0;          // LKJ shape parameter

  // -------------------------------------------------------------------------
  // Symbolic parameters (random variables to infer)
  // -------------------------------------------------------------------------

  // Population-level mean: γ ~ MVN(0, σ_γ²I)
  // In practice, each component: γ[k] ~ Normal(0, σ_γ)
  static auto gamma() { return dimVector<Predictors>(); }

  // Coefficient scales: τ[k] ~ HalfNormal(σ_τ)
  static auto tau() { return dimVector<Predictors>(); }

  // Correlation Cholesky: L_Ω ~ LKJ(η)
  static auto lOmega() { return cholCorr<Predictors>(); }

  // Standard normal auxiliary for group j: z[j] ~ MVN(0, I)
  // In full model: plate<Groups>(dimVector<Predictors>())
  static auto zAux() { return dimVector<Predictors>(); }

  // -------------------------------------------------------------------------
  // Derived quantities
  // -------------------------------------------------------------------------

  // Covariance Cholesky factor: L_Σ = diag(τ) × L_Ω
  // This combines scale and correlation into the full covariance structure.
  static auto covCholeskyFor(auto tau_vec, auto l_omega) {
    return diagPreMult(tau_vec, l_omega);
  }

  // Group coefficients: β[j] = γ + L_Σ × z[j]
  // Note: L_Σ × z[j] introduces both scale and correlation
  static auto groupCoeffsFor(auto gamma_vec, auto l_sigma, auto z_vec) {
    // In full implementation: gamma_vec + matVecMult(l_sigma, z_vec)
    // For now, showing the structure symbolically
    return gamma_vec;  // Placeholder - full impl needs matrix-vector mult
  }

  // -------------------------------------------------------------------------
  // Log probability contributions
  // -------------------------------------------------------------------------

  // Prior on population mean: log p(γ | σ_γ)
  // = Σₖ logNormal(γ[k] | 0, σ_γ)
  // This would use MVNCholesky with identity covariance in full implementation
  static auto logPriorGamma(auto gamma_vec, auto k) {
    // MVN(0, σ_γ²I) log-prob
    auto sigma_gamma_lit = 5.0_c;  // sigma_gamma
    auto l_cov_gamma = cholCov<Predictors>();  // σ_γ × I (diagonal)
    return logMVNormalCholesky(gamma_vec, dimVector<Predictors>(),
                               l_cov_gamma, k);
  }

  // Prior on scales: log p(τ | σ_τ)
  // = Σₖ logHalfNormal(τ[k] | σ_τ)
  // Note: Requires constraining τ > 0 via transform
  // static auto logPriorTau(auto tau_vec) { ... }

  // Prior on correlation Cholesky: log p(L_Ω | η)
  static auto logPriorCorrelation(auto l_omega) {
    return lkjCholesky(2.0_c).logProbFor(l_omega);
  }

  // Prior on auxiliary standard normals: log p(z | I)
  // = Σₖ logNormal(z[k] | 0, 1) = -K/2 log(2π) - ½||z||²
  static auto logPriorZ(auto z_vec, auto k) {
    auto zero_mean = dimVector<Predictors>();
    auto identity_chol = cholCov<Predictors>();
    return logMVNormalCholesky(z_vec, zero_mean, identity_chol, k);
  }
};

// ============================================================================
// Usage Example (Conceptual)
// ============================================================================
//
// This shows how the model would be used in practice. The actual implementation
// would require:
//   1. Matrix-vector multiplication operator
//   2. Plate support for indexed DimVectorSymbol
//   3. Transforms for constrained parameters (τ > 0)
//
// ```cpp
// // Dimensions
// constexpr std::size_t J = 50;   // Number of groups (e.g., schools)
// constexpr std::size_t K = 3;    // Number of predictors (intercept + 2 slopes)
// constexpr std::size_t N = 500;  // Number of observations
//
// // Data (would come from real dataset)
// std::vector<double> y(N);              // Outcomes
// std::vector<std::array<double, K>> X(N);  // Design matrix
// std::vector<std::size_t> group(N);     // Group assignments
//
// // Define model
// auto gamma = dimVector<Predictors>();           // Population mean
// auto tau = dimVector<Predictors>();             // Scales
// auto l_omega = cholCorr<Predictors>();          // Correlation Cholesky
// auto z = plate<Groups>(dimVector<Predictors>()); // Auxiliary vars
//
// // Priors
// auto gamma_prior = mvnCholesky(zeros<Predictors>(), sigma_gamma * eye<Predictors>());
// auto tau_prior = plate<Predictors>(halfNormal(2.5_c));
// auto l_omega_prior = lkjCholesky(2.0_c);
// auto z_prior = plate<Groups>(mvnCholesky(zeros<Predictors>(), eye<Predictors>()));
//
// // Derived: group coefficients
// auto l_sigma = diagPreMult(tau, l_omega);
// auto beta = [&](std::size_t j) {
//   return gamma + matVecMult(l_sigma, z[j]);
// };
//
// // Likelihood
// auto sigma_y = halfNormal(1.0_c);  // Observation noise
// auto y_rv = plate<Observations>([&](std::size_t i) {
//   auto j = group[i];
//   auto mu_i = dot(X[i], beta(j));
//   return normal(mu_i, sigma_y);
// });
//
// // Build posterior
// auto m = model(gamma, tau, l_omega, z, sigma_y, y_rv);
// auto posterior = m.posterior()
//     .withDimension<Predictors>(K)
//     .withDimension<Groups>(J)
//     .observe(y_rv = y_data)
//     .build();
//
// // Sample via HMC
// // ...
// ```
//
// ============================================================================
// Key Implementation Notes
// ============================================================================
//
// 1. PARAMETER TRANSFORMS:
//    - γ: unconstrained (identity transform)
//    - τ: positive (log transform)
//    - L_Ω: correlation Cholesky (spherical coordinates or stick-breaking)
//    - z: unconstrained (identity transform)
//
// 2. JACOBIAN CORRECTIONS:
//    - log(τ) transform: add Σₖ log(τ[k]) to log-posterior
//    - L_Ω transform: handled internally by LKJ Cholesky parameterization
//
// 3. EFFICIENCY CONSIDERATIONS:
//    - Store L_Σ = diag(τ) × L_Ω rather than recomputing
//    - For large K, use sparse/banded correlation structures
//    - For large J, vectorize z[j] into J×K matrix
//
// 4. POSTERIOR PREDICTIVE:
//    - Sample z_new ~ MVN(0, I)
//    - Compute β_new = γ + L_Σ × z_new
//    - Predict y_new from β_new
//
// ============================================================================

}  // namespace tempura::symbolic4
