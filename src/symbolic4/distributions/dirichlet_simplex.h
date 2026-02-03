#pragma once

#include <array>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <span>

#include "symbolic4/mcmc/transforms.h"

// ============================================================================
// dirichlet_simplex.h - Dirichlet distribution with stick-breaking transform
// ============================================================================
//
// Combines SimplexTransform<K> with Dirichlet distribution to provide:
//   - Transform from K-1 unconstrained parameters to K-simplex
//   - Log-probability of Dirichlet(alpha) plus stick-breaking Jacobian
//   - Gradient through the transform including Jacobian correction
//
// Usage:
//   auto dir = dirichletSimplex<3>({2.0, 2.0, 2.0});
//   auto delta = dir.transform(z);  // K-1 unconstrained -> K-simplex
//   double lp = dir.logProbWithJacobian(z);  // Dirichlet log-prob + Jacobian
//   auto grad_z = dir.chainRuleGrad(z, grad_delta);  // Full gradient
//
// ============================================================================

namespace tempura::symbolic4 {

// Dirichlet(alpha) on K-simplex via stick-breaking transform
template <std::size_t K>
  requires (K >= 2)
struct DirichletSimplex {
  SimplexTransform<K> transform_{};
  std::array<double, K> alpha_;

  explicit constexpr DirichletSimplex(std::array<double, K> alpha) : alpha_{alpha} {
    // All alpha values must be positive
    for (std::size_t i = 0; i < K; ++i) {
      assert(alpha_[i] > 0.0 && "Dirichlet concentration must be positive");
    }
  }

  // Transform: K-1 unconstrained -> K-simplex
  auto transform(std::span<const double, K - 1> z) const -> std::array<double, K> {
    return transform_.transform(z);
  }

  auto transform(const std::array<double, K - 1>& z) const -> std::array<double, K> {
    return transform_.transform(z);
  }

  // Inverse: K-simplex -> K-1 unconstrained
  auto inverse(std::span<const double, K> delta) const -> std::array<double, K - 1> {
    return transform_.inverse(delta);
  }

  auto inverse(const std::array<double, K>& delta) const -> std::array<double, K - 1> {
    return transform_.inverse(delta);
  }

  // Log p(delta | alpha) where delta = transform(z)
  // Dirichlet: log p(delta|alpha) = sum_i (alpha[i]-1)*log(delta[i]) + const
  // We include normalizing constant for completeness
  auto logDirichlet(std::span<const double, K> delta) const -> double {
    double result = 0.0;
    for (std::size_t i = 0; i < K; ++i) {
      result += (alpha_[i] - 1.0) * std::log(delta[i]);
    }
    // Add normalizing constant: log(Gamma(sum(alpha))) - sum(log(Gamma(alpha[i])))
    double alpha_sum = 0.0;
    for (std::size_t i = 0; i < K; ++i) {
      alpha_sum += alpha_[i];
      result -= std::lgamma(alpha_[i]);
    }
    result += std::lgamma(alpha_sum);
    return result;
  }

  auto logDirichlet(const std::array<double, K>& delta) const -> double {
    return logDirichlet(std::span<const double, K>{delta});
  }

  // log p(delta | alpha) + log |Jacobian|
  // This is the full log-probability in the unconstrained (z) space
  auto logProbWithJacobian(std::span<const double, K - 1> z) const -> double {
    auto delta = transform(z);
    double log_dirichlet = logDirichlet(delta);
    double log_jacobian = transform_.logJacobian(z);
    return log_dirichlet + log_jacobian;
  }

  auto logProbWithJacobian(const std::array<double, K - 1>& z) const -> double {
    return logProbWithJacobian(std::span<const double, K - 1>{z});
  }

  // Gradient of Dirichlet log-prob w.r.t. delta
  // d/d(delta[i]) [ (alpha[i]-1)*log(delta[i]) ] = (alpha[i]-1) / delta[i]
  auto dirichletGrad(std::span<const double, K> delta) const -> std::array<double, K> {
    std::array<double, K> grad{};
    for (std::size_t i = 0; i < K; ++i) {
      grad[i] = (alpha_[i] - 1.0) / delta[i];
    }
    return grad;
  }

  auto dirichletGrad(const std::array<double, K>& delta) const -> std::array<double, K> {
    return dirichletGrad(std::span<const double, K>{delta});
  }

  // Chain rule: grad_delta -> grad_z (includes Dirichlet gradient and Jacobian gradient)
  // This computes d/dz [ log p(delta|alpha) + log |J| + f(delta) ]
  // where grad_delta = d f / d delta
  auto chainRuleGrad(std::span<const double, K - 1> z,
                     std::span<const double, K> grad_delta) const -> std::array<double, K - 1> {
    auto delta = transform(z);
    auto dir_grad = dirichletGrad(delta);

    // Combine external gradient with Dirichlet gradient
    std::array<double, K> total_grad{};
    for (std::size_t i = 0; i < K; ++i) {
      total_grad[i] = grad_delta[i] + dir_grad[i];
    }

    // Use SimplexTransform's chainRuleGrad which handles the transform Jacobian
    return transform_.chainRuleGrad(z, std::span<const double, K>{total_grad});
  }

  auto chainRuleGrad(const std::array<double, K - 1>& z,
                     const std::array<double, K>& grad_delta) const -> std::array<double, K - 1> {
    return chainRuleGrad(std::span<const double, K - 1>{z},
                         std::span<const double, K>{grad_delta});
  }

  // Gradient of full log-prob (Dirichlet + Jacobian) w.r.t. z
  // Equivalent to chainRuleGrad with grad_delta = 0
  auto logProbGrad(std::span<const double, K - 1> z) const -> std::array<double, K - 1> {
    std::array<double, K> zero_grad{};
    return chainRuleGrad(z, std::span<const double, K>{zero_grad});
  }

  auto logProbGrad(const std::array<double, K - 1>& z) const -> std::array<double, K - 1> {
    return logProbGrad(std::span<const double, K - 1>{z});
  }
};

// Factory function
template <std::size_t K>
  requires (K >= 2)
constexpr auto dirichletSimplex(std::array<double, K> alpha) -> DirichletSimplex<K> {
  return DirichletSimplex<K>{alpha};
}

}  // namespace tempura::symbolic4
