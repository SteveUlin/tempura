#pragma once

#include <cassert>
#include <cmath>
#include <cstddef>
#include <span>
#include <vector>

#include "symbolic4/core.h"
#include "symbolic4/matrix/eval.h"
#include "symbolic4/matrix/mvn.h"
#include "symbolic4/matrix/types.h"
#include "symbolic4/mcmc/transforms.h"

// Note: We use CholeskyTransform from mcmc/transforms.h (non-templated),
// not the templated one from matrix/hmc.h

// ============================================================================
// matrix_mcmc.h - MCMC adapter for matrix-valued parameters (MVN sampling)
// ============================================================================
//
// Provides MatrixPosterior for MCMC sampling of multivariate normal models
// with dynamic state size. Combines:
//   - DimVectorTransform for mean vector (K params, identity transform)
//   - CholeskyTransform for covariance Cholesky (K(K+1)/2 params, log-diag)
//
// State layout: [mu_0, ..., mu_{K-1}, z_L_00, z_L_10, z_L_11, ...]
// Total size: K + K(K+1)/2
//
// The Cholesky transform maps unconstrained z to L via:
//   - Diagonal: L[i,i] = exp(z[diag_idx])
//   - Off-diagonal: L[i,j] = z[k] directly
//
// Log-Jacobian = sum of z values at diagonal positions (from exp transform)
//
// Usage:
//   struct Dims;
//   auto mu = dimVector<Dims>();
//   auto L = cholCov<Dims>();
//   auto y = dimVector<Dims>();
//   auto dist = mvnCholesky(mu, L);
//
//   auto posterior = makeMatrixPosterior(dist, mu, L, /*dim=*/ 3)
//                      .observe(y_binding);
//
//   std::vector<double> state(posterior.stateDim());
//   double lp = posterior.logProb(state);
//   auto grad = posterior.gradient(state);
//
// ============================================================================

namespace tempura::symbolic4 {

// ============================================================================
// DimVectorTransform - Maps K-dimensional vector to K unconstrained params
// ============================================================================
//
// Simple identity transform for unconstrained vectors:
//   x_i = z_i
//
// Log-Jacobian: 0 (identity transform)
//
// Defined locally to avoid including matrix/hmc.h which conflicts with
// mcmc/transforms.h's CholeskyTransform.

struct DimVectorTransform {
  std::size_t dim;

  // Forward: unconstrained z -> DynamicVector x (identity)
  auto transform(std::span<const double> z) const -> DynamicVector {
    DynamicVector x(dim);
    for (std::size_t i = 0; i < dim; ++i) {
      x[i] = z[i];
    }
    return x;
  }

  // Inverse: DynamicVector x -> unconstrained z (identity)
  auto inverse(const DynamicVector& x) const -> std::vector<double> {
    std::vector<double> z(dim);
    for (std::size_t i = 0; i < dim; ++i) {
      z[i] = x[i];
    }
    return z;
  }

  // Log-Jacobian: 0 for identity transform
  auto logJacobian([[maybe_unused]] std::span<const double> z) const -> double {
    return 0.0;
  }

  // Number of unconstrained parameters needed for dimension k
  static constexpr auto stateSize(std::size_t k) -> std::size_t { return k; }
};

// ============================================================================
// MatrixPosterior - Wraps MVN for MCMC with dynamic state size
// ============================================================================

template <typename LogProbExpr, typename MuSym, typename CholSym, typename ObsBindings>
class MatrixPosterior {
 public:
  MatrixPosterior(LogProbExpr log_prob, MuSym mu_sym, CholSym chol_sym,
                  std::size_t dim, ObsBindings obs)
      : log_prob_expr_{log_prob},
        mu_sym_{mu_sym},
        chol_sym_{chol_sym},
        dim_{dim},
        mu_transform_{dim},
        chol_transform_{dim},
        observations_{obs} {}

  // Total state dimension: K (for mu) + K(K+1)/2 (for Cholesky)
  auto stateDim() const -> std::size_t {
    return dim_ + CholeskyTransform::stateSize(dim_);
  }

  // Evaluate log-probability at unconstrained state z
  // z = [z_mu_0, ..., z_mu_{K-1}, z_L_00, z_L_10, z_L_11, ...]
  auto logProb(std::span<const double> z) const -> double {
    assert(z.size() == stateDim());

    // Split state into mu and Cholesky parts
    auto z_mu = z.subspan(0, dim_);
    auto z_chol = z.subspan(dim_);

    // Transform: mu is identity, L uses exp for diagonal
    DynamicVector mu = mu_transform_.transform(z_mu);
    DynamicMatrix L = chol_transform_.transform(z_chol);

    // Compute log-Jacobian: only Cholesky contributes (mu is identity)
    double log_jacobian = chol_transform_.logJacobian(z_chol);

    // Create bindings and evaluate log-probability
    auto mu_binding = DimVectorBinding<MuSym>{mu};
    auto chol_binding = CholeskyBinding<CholSym>{L};

    // Merge with observations
    auto bindings = mergeBindings(mu_binding, chol_binding, observations_);
    double lp = evaluateMatrix(log_prob_expr_, bindings);

    return lp + log_jacobian;
  }

  // Evaluate gradient using finite differences
  auto gradient(std::span<const double> z) const -> std::vector<double> {
    constexpr double eps = 1e-7;
    std::vector<double> grad(z.size());
    std::vector<double> z_plus(z.begin(), z.end());
    std::vector<double> z_minus(z.begin(), z.end());

    for (std::size_t i = 0; i < z.size(); ++i) {
      z_plus[i] = z[i] + eps;
      z_minus[i] = z[i] - eps;

      double lp_plus = logProb(z_plus);
      double lp_minus = logProb(z_minus);

      grad[i] = (lp_plus - lp_minus) / (2.0 * eps);

      z_plus[i] = z[i];
      z_minus[i] = z[i];
    }

    return grad;
  }

  // Transform from unconstrained z to constrained values
  // Returns: [mu_0, ..., mu_{K-1}, L_00, L_10, L_11, L_20, ...]
  auto transform(std::span<const double> z) const -> std::vector<double> {
    assert(z.size() == stateDim());

    auto z_mu = z.subspan(0, dim_);
    auto z_chol = z.subspan(dim_);

    DynamicVector mu = mu_transform_.transform(z_mu);
    DynamicMatrix L = chol_transform_.transform(z_chol);

    // Pack results: mu followed by packed Cholesky
    std::vector<double> result;
    result.reserve(stateDim());

    for (std::size_t i = 0; i < dim_; ++i) {
      result.push_back(mu[i]);
    }

    // Pack Cholesky in column-major lower triangle order
    for (std::size_t col = 0; col < dim_; ++col) {
      for (std::size_t row = col; row < dim_; ++row) {
        result.push_back(L[row, col]);
      }
    }

    return result;
  }

  // Inverse transform: constrained values to unconstrained z
  auto inverse(std::span<const double> x) const -> std::vector<double> {
    assert(x.size() == stateDim());

    std::vector<double> z(stateDim());

    // Mu: identity inverse
    for (std::size_t i = 0; i < dim_; ++i) {
      z[i] = x[i];
    }

    // Cholesky: extract from x and apply inverse transform
    DynamicMatrix L(dim_, dim_);
    std::size_t x_idx = dim_;
    for (std::size_t col = 0; col < dim_; ++col) {
      for (std::size_t row = col; row < dim_; ++row) {
        L[row, col] = x[x_idx++];
      }
    }

    auto z_chol = chol_transform_.inverse(L);
    for (std::size_t i = 0; i < z_chol.size(); ++i) {
      z[dim_ + i] = z_chol[i];
    }

    return z;
  }

  // Get the dimension
  auto dim() const -> std::size_t { return dim_; }

  // Extract mu from state
  auto extractMu(std::span<const double> z) const -> DynamicVector {
    auto z_mu = z.subspan(0, dim_);
    return mu_transform_.transform(z_mu);
  }

  // Extract Cholesky from state
  auto extractCholesky(std::span<const double> z) const -> DynamicMatrix {
    auto z_chol = z.subspan(dim_);
    return chol_transform_.transform(z_chol);
  }

 private:
  LogProbExpr log_prob_expr_;
  [[no_unique_address]] MuSym mu_sym_;
  [[no_unique_address]] CholSym chol_sym_;
  std::size_t dim_;
  DimVectorTransform mu_transform_;
  CholeskyTransform chol_transform_;
  ObsBindings observations_;

  // Merge bindings: param bindings + observation bindings
  template <typename MuBinding, typename CholBinding, typename Obs>
  static auto mergeBindings(MuBinding mu, CholBinding chol, [[maybe_unused]] Obs obs) {
    if constexpr (std::is_same_v<Obs, BinderPack<>>) {
      return MatrixBinderPack{mu, chol};
    } else {
      return mergeWithObs(mu, chol, obs);
    }
  }

  // Helper to merge param bindings with observations
  template <typename MuBinding, typename CholBinding, typename... ObsBinders>
  static auto mergeWithObs(MuBinding mu, CholBinding chol, BinderPack<ObsBinders...> obs) {
    return MatrixBinderPack{mu, chol, static_cast<ObsBinders&>(obs)...};
  }
};

// ============================================================================
// MatrixPosteriorBuilder - Fluent builder for MatrixPosterior
// ============================================================================

template <typename LogProbExpr, typename MuSym, typename CholSym>
class MatrixPosteriorBuilder {
 public:
  MatrixPosteriorBuilder(LogProbExpr log_prob, MuSym mu_sym, CholSym chol_sym, std::size_t dim)
      : log_prob_expr_{log_prob}, mu_sym_{mu_sym}, chol_sym_{chol_sym}, dim_{dim} {}

  // Add observations
  template <typename... Binders>
  auto observe(Binders... binders) const {
    auto obs = BinderPack{binders...};
    return MatrixPosterior<LogProbExpr, MuSym, CholSym, decltype(obs)>{
        log_prob_expr_, mu_sym_, chol_sym_, dim_, obs};
  }

  // Build without observations
  auto build() const {
    return MatrixPosterior<LogProbExpr, MuSym, CholSym, BinderPack<>>{
        log_prob_expr_, mu_sym_, chol_sym_, dim_, BinderPack<>{}};
  }

 private:
  LogProbExpr log_prob_expr_;
  [[no_unique_address]] MuSym mu_sym_;
  [[no_unique_address]] CholSym chol_sym_;
  std::size_t dim_;
};

// ============================================================================
// Factory functions
// ============================================================================

// Create MatrixPosterior from MVN distribution and observation symbol
// Uses the distribution's unnormalized log-prob (normalizing constant doesn't
// affect MCMC)
//
// Parameters:
//   dist      - MVN distribution with mu and L parameters
//   mu_sym    - Symbol for mean vector parameter
//   chol_sym  - Symbol for Cholesky factor parameter
//   y_sym     - Symbol for observation vector (same type as used in bindings)
//   dim       - Dimension K of the MVN
template <typename Mu, typename LCov, typename YSym>
auto makeMatrixPosterior(MVNormalCholeskyDist<Mu, LCov> dist, Mu mu_sym, LCov chol_sym,
                         YSym y_sym, std::size_t dim) {
  // Get unnormalized log-prob expression using the provided y symbol
  auto log_prob = dist.unnormalizedLogProbFor(y_sym);

  return MatrixPosteriorBuilder<decltype(log_prob), Mu, LCov>{log_prob, mu_sym, chol_sym, dim};
}

// Overload that takes log-prob expression directly
template <Symbolic LogProbExpr, typename MuSym, typename CholSym>
auto makeMatrixPosterior(LogProbExpr log_prob, MuSym mu_sym, CholSym chol_sym, std::size_t dim) {
  return MatrixPosteriorBuilder<LogProbExpr, MuSym, CholSym>{log_prob, mu_sym, chol_sym, dim};
}

// ============================================================================
// MVNPrior - Helper for specifying MVN priors on mu and L
// ============================================================================
//
// For hierarchical models, mu and L often have their own priors:
//   mu ~ Normal(mu0, sigma0)   (independent components)
//   L ~ LKJ(eta)               (correlation structure)
//
// This helper computes the combined log-prior for use in full Bayesian inference.

struct MVNPriorConfig {
  // Prior on mu: Normal(mu_prior_mean, mu_prior_sd) for each component
  double mu_prior_mean = 0.0;
  double mu_prior_sd = 10.0;

  // LKJ prior on correlation structure (eta=1 is uniform)
  double lkj_eta = 1.0;

  // Half-Normal prior on diagonal of L (scale parameters)
  double scale_prior_sd = 2.5;
};

// Compute log-prior for mu (independent normal on each component)
inline auto muLogPrior(const DynamicVector& mu, double prior_mean, double prior_sd) -> double {
  double log_p = 0.0;
  double log_norm = -0.5 * std::log(2.0 * M_PI) - std::log(prior_sd);
  double inv_var = 1.0 / (prior_sd * prior_sd);

  for (std::size_t i = 0; i < mu.size(); ++i) {
    double z = mu[i] - prior_mean;
    log_p += log_norm - 0.5 * z * z * inv_var;
  }
  return log_p;
}

// Compute LKJ log-prior for Cholesky factor
// LKJ(eta) on correlation matrix R = LL^T where L is unit-norm rows
// For covariance Cholesky, we separate scale (diagonal) from correlation
inline auto lkjLogPrior(const DynamicMatrix& L, double eta, std::size_t k) -> double {
  // LKJ prior: p(L) proportional to product of L[i,i]^{K-i-1} for correlation
  // For eta != 1, there's an additional factor
  // Simplified version assuming eta=1 (uniform over correlations)
  double log_p = 0.0;

  if (std::abs(eta - 1.0) < 1e-10) {
    // eta=1: uniform over correlation matrices
    // Jacobian from correlation to Cholesky gives:
    // log p(L) = sum_{i=1}^{K-1} (K-i-1) * log(L[i,i])
    for (std::size_t i = 1; i < k; ++i) {
      log_p += static_cast<double>(k - i - 1) * std::log(L[i, i]);
    }
  } else {
    // General eta: additional factor of L[i,i]^{2*(eta-1)}
    for (std::size_t i = 1; i < k; ++i) {
      double exponent = static_cast<double>(k - i - 1) + 2.0 * (eta - 1.0);
      log_p += exponent * std::log(L[i, i]);
    }
  }

  return log_p;
}

// Compute Half-Normal log-prior on Cholesky diagonal (scale parameters)
inline auto choleskyDiagonalLogPrior(const DynamicMatrix& L, double scale_sd, std::size_t k)
    -> double {
  double log_p = 0.0;
  // Half-Normal: p(x) = 2/sqrt(2*pi*s^2) * exp(-x^2/(2*s^2)) for x > 0
  double log_norm = std::log(2.0) - 0.5 * std::log(2.0 * M_PI) - std::log(scale_sd);
  double inv_var = 1.0 / (scale_sd * scale_sd);

  for (std::size_t i = 0; i < k; ++i) {
    double diag = L[i, i];
    log_p += log_norm - 0.5 * diag * diag * inv_var;
  }
  return log_p;
}

// ============================================================================
// MatrixPosteriorWithPriors - Full Bayesian posterior with priors on params
// ============================================================================

template <typename LogProbExpr, typename MuSym, typename CholSym, typename ObsBindings>
class MatrixPosteriorWithPriors {
 public:
  MatrixPosteriorWithPriors(LogProbExpr log_prob, MuSym mu_sym, CholSym chol_sym,
                            std::size_t dim, ObsBindings obs, MVNPriorConfig config)
      : base_{log_prob, mu_sym, chol_sym, dim, obs}, config_{config}, dim_{dim} {}

  auto stateDim() const -> std::size_t { return base_.stateDim(); }

  auto logProb(std::span<const double> z) const -> double {
    // Base likelihood + Jacobian
    double lp = base_.logProb(z);

    // Extract transformed parameters for prior evaluation
    DynamicVector mu = base_.extractMu(z);
    DynamicMatrix L = base_.extractCholesky(z);

    // Add priors
    lp += muLogPrior(mu, config_.mu_prior_mean, config_.mu_prior_sd);
    lp += lkjLogPrior(L, config_.lkj_eta, dim_);
    lp += choleskyDiagonalLogPrior(L, config_.scale_prior_sd, dim_);

    return lp;
  }

  auto gradient(std::span<const double> z) const -> std::vector<double> {
    constexpr double eps = 1e-7;
    std::vector<double> grad(z.size());
    std::vector<double> z_plus(z.begin(), z.end());
    std::vector<double> z_minus(z.begin(), z.end());

    for (std::size_t i = 0; i < z.size(); ++i) {
      z_plus[i] = z[i] + eps;
      z_minus[i] = z[i] - eps;

      double lp_plus = logProb(z_plus);
      double lp_minus = logProb(z_minus);

      grad[i] = (lp_plus - lp_minus) / (2.0 * eps);

      z_plus[i] = z[i];
      z_minus[i] = z[i];
    }

    return grad;
  }

  auto transform(std::span<const double> z) const -> std::vector<double> {
    return base_.transform(z);
  }

  auto inverse(std::span<const double> x) const -> std::vector<double> {
    return base_.inverse(x);
  }

  auto dim() const -> std::size_t { return dim_; }

 private:
  MatrixPosterior<LogProbExpr, MuSym, CholSym, ObsBindings> base_;
  MVNPriorConfig config_;
  std::size_t dim_;
};

// ============================================================================
// Builder with priors
// ============================================================================

template <typename LogProbExpr, typename MuSym, typename CholSym>
class MatrixPosteriorWithPriorsBuilder {
 public:
  MatrixPosteriorWithPriorsBuilder(LogProbExpr log_prob, MuSym mu_sym, CholSym chol_sym,
                                    std::size_t dim, MVNPriorConfig config)
      : log_prob_expr_{log_prob},
        mu_sym_{mu_sym},
        chol_sym_{chol_sym},
        dim_{dim},
        config_{config} {}

  template <typename... Binders>
  auto observe(Binders... binders) const {
    auto obs = BinderPack{binders...};
    return MatrixPosteriorWithPriors<LogProbExpr, MuSym, CholSym, decltype(obs)>{
        log_prob_expr_, mu_sym_, chol_sym_, dim_, obs, config_};
  }

  auto build() const {
    return MatrixPosteriorWithPriors<LogProbExpr, MuSym, CholSym, BinderPack<>>{
        log_prob_expr_, mu_sym_, chol_sym_, dim_, BinderPack<>{}, config_};
  }

 private:
  LogProbExpr log_prob_expr_;
  [[no_unique_address]] MuSym mu_sym_;
  [[no_unique_address]] CholSym chol_sym_;
  std::size_t dim_;
  MVNPriorConfig config_;
};

// Factory with priors
template <typename Mu, typename LCov, typename YSym>
auto makeMatrixPosteriorWithPriors(MVNormalCholeskyDist<Mu, LCov> dist, Mu mu_sym, LCov chol_sym,
                                    YSym y_sym, std::size_t dim, MVNPriorConfig config = {}) {
  auto log_prob = dist.unnormalizedLogProbFor(y_sym);

  return MatrixPosteriorWithPriorsBuilder<decltype(log_prob), Mu, LCov>{
      log_prob, mu_sym, chol_sym, dim, config};
}

}  // namespace tempura::symbolic4
