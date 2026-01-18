#pragma once

#include <array>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <functional>
#include <numbers>
#include <random>
#include <tuple>
#include <vector>

#include "bayes/normal.h"

namespace tempura::bayes {

// =============================================================================
// Automatic Differentiation Variational Inference (ADVI)
// =============================================================================
//
// ADVI approximates the posterior p(θ|data) with a simpler variational
// distribution q(θ) and optimizes the parameters of q to minimize
// KL(q || p), equivalently maximizing the ELBO.
//
// Key insight: Instead of sampling from the true posterior (expensive),
// we find the best Gaussian approximation by gradient-based optimization.
//
// This implementation uses:
//   - Mean-field Gaussian: q(θ) = ∏ᵢ N(θᵢ | μᵢ, exp(ωᵢ)²)
//   - Reparameterization trick: θ = μ + exp(ω) ⊙ ε, ε ~ N(0,I)
//   - Adam optimizer for stable convergence
//   - Monte Carlo estimation of ELBO gradients

// =============================================================================
// ADVIOptions - Configuration for ADVI optimization
// =============================================================================

struct ADVIOptions {
  double learning_rate = 0.1;
  double beta1 = 0.9;       // Adam first moment decay
  double beta2 = 0.999;     // Adam second moment decay
  std::size_t mc_samples = 1;  // Monte Carlo samples for gradient estimation
  double elbo_tol = 1e-4;   // Convergence tolerance (relative ELBO change)
  double epsilon = 1e-8;    // Adam numerical stability constant
};

// =============================================================================
// MeanFieldGaussian - Variational family for ADVI
// =============================================================================
//
// Parameterizes N independent Gaussian distributions using:
//   - mu: means
//   - omega: log standard deviations (ensures σ > 0 without constraints)
//
// Sampling uses the reparameterization trick:
//   θ = μ + exp(ω) ⊙ ε, where ε ~ N(0, I)
//
// This makes gradients flow through samples, enabling gradient-based optimization.

template <typename T, std::size_t N>
struct MeanFieldGaussian {
  std::array<T, N> mu{};     // Means
  std::array<T, N> omega{};  // Log standard deviations

  constexpr MeanFieldGaussian() = default;

  constexpr MeanFieldGaussian(std::array<T, N> mu_init,
                               std::array<T, N> omega_init)
      : mu{mu_init}, omega{omega_init} {}

  // Sample using reparameterization: θ = μ + σε, σ = exp(ω)
  template <std::uniform_random_bit_generator Gen>
  auto sample(Gen& g) const -> std::array<T, N> {
    std::array<T, N> result;
    Normal<T> std_normal{T{0}, T{1}};
    for (std::size_t i = 0; i < N; ++i) {
      T epsilon = std_normal.sample(g);
      result[i] = mu[i] + std::exp(omega[i]) * epsilon;
    }
    return result;
  }

  // Sample with pre-drawn noise (for gradient computation)
  auto sampleWithNoise(const std::array<T, N>& epsilon) const
      -> std::array<T, N> {
    std::array<T, N> result;
    for (std::size_t i = 0; i < N; ++i) {
      result[i] = mu[i] + std::exp(omega[i]) * epsilon[i];
    }
    return result;
  }

  // Entropy of mean-field Gaussian: H[q] = Σᵢ (ωᵢ + 0.5 * log(2πe))
  // This is maximized by ELBO, encouraging spread in the approximation.
  constexpr auto entropy() const -> T {
    constexpr T half_log_2pi_e =
        T{0.5} * (T{1} + std::log(T{2} * T{std::numbers::pi}));
    T h{0};
    for (std::size_t i = 0; i < N; ++i) {
      h += omega[i] + half_log_2pi_e;
    }
    return h;
  }

  // Standard deviations (derived from omega)
  constexpr auto sigma() const -> std::array<T, N> {
    std::array<T, N> result;
    for (std::size_t i = 0; i < N; ++i) {
      result[i] = std::exp(omega[i]);
    }
    return result;
  }
};

// =============================================================================
// Adam Optimizer State
// =============================================================================
//
// Adam combines momentum (tracking mean of gradients) with RMSprop (tracking
// variance of gradients) for adaptive per-parameter learning rates.
//
// Update equations:
//   m = β₁m + (1-β₁)g           (momentum)
//   v = β₂v + (1-β₂)g²          (adaptive learning rate)
//   m̂ = m / (1 - β₁ᵗ)           (bias correction)
//   v̂ = v / (1 - β₂ᵗ)           (bias correction)
//   θ = θ + lr · m̂ / (√v̂ + ε)  (update)

template <typename T, std::size_t N>
struct AdamState {
  std::array<T, N> m_mu{};     // First moment for mu
  std::array<T, N> v_mu{};     // Second moment for mu
  std::array<T, N> m_omega{};  // First moment for omega
  std::array<T, N> v_omega{};  // Second moment for omega
  std::size_t t{0};            // Timestep for bias correction

  constexpr AdamState() {
    for (std::size_t i = 0; i < N; ++i) {
      m_mu[i] = T{0};
      v_mu[i] = T{0};
      m_omega[i] = T{0};
      v_omega[i] = T{0};
    }
  }

  // Update variational parameters given gradients
  void update(MeanFieldGaussian<T, N>& params,
              const std::array<T, N>& grad_mu,
              const std::array<T, N>& grad_omega,
              const ADVIOptions& opts) {
    ++t;

    T beta1_t = std::pow(opts.beta1, static_cast<T>(t));
    T beta2_t = std::pow(opts.beta2, static_cast<T>(t));

    for (std::size_t i = 0; i < N; ++i) {
      // Update mu moments and parameters
      m_mu[i] = opts.beta1 * m_mu[i] + (T{1} - opts.beta1) * grad_mu[i];
      v_mu[i] =
          opts.beta2 * v_mu[i] + (T{1} - opts.beta2) * grad_mu[i] * grad_mu[i];

      T m_hat_mu = m_mu[i] / (T{1} - beta1_t);
      T v_hat_mu = v_mu[i] / (T{1} - beta2_t);

      // Gradient ascent (maximizing ELBO)
      params.mu[i] += opts.learning_rate * m_hat_mu /
                      (std::sqrt(v_hat_mu) + static_cast<T>(opts.epsilon));

      // Update omega moments and parameters
      m_omega[i] =
          opts.beta1 * m_omega[i] + (T{1} - opts.beta1) * grad_omega[i];
      v_omega[i] = opts.beta2 * v_omega[i] +
                   (T{1} - opts.beta2) * grad_omega[i] * grad_omega[i];

      T m_hat_omega = m_omega[i] / (T{1} - beta1_t);
      T v_hat_omega = v_omega[i] / (T{1} - beta2_t);

      params.omega[i] += opts.learning_rate * m_hat_omega /
                         (std::sqrt(v_hat_omega) + static_cast<T>(opts.epsilon));
    }
  }
};

// =============================================================================
// ADVI - Main class for variational inference
// =============================================================================
//
// Template parameters:
//   T: Numeric type (typically double)
//   N: Dimension of parameter space
//   LogJoint: Callable (θ) -> log p(θ, data)
//   GradLogJoint: Callable (θ) -> ∇log p(θ, data)
//
// The ELBO objective:
//   ELBO = E_q[log p(θ, data)] + H[q]
//        = E_q[log p(θ, data)] - E_q[log q(θ)]
//
// We maximize ELBO using stochastic gradient ascent with the reparameterization
// trick for unbiased gradient estimates.

template <typename T, std::size_t N, typename LogJoint, typename GradLogJoint>
class ADVI {
 public:
  using State = std::array<T, N>;
  using Variational = MeanFieldGaussian<T, N>;

  ADVI(LogJoint log_joint, GradLogJoint grad_log_joint,
       ADVIOptions opts = {})
      : log_joint_{std::move(log_joint)},
        grad_log_joint_{std::move(grad_log_joint)},
        opts_{opts},
        params_{},
        adam_{} {
    assert(opts.learning_rate > 0 && "learning rate must be positive");
    assert(opts.mc_samples >= 1 && "must use at least 1 MC sample");
  }

  // Initialize variational parameters (optional, can customize starting point)
  void initialize(Variational init_params) { params_ = init_params; }

  // Run optimization for max_iters iterations
  // Returns final variational approximation
  template <std::uniform_random_bit_generator Gen>
  auto fit(std::size_t max_iters, Gen& g) -> Variational {
    elbo_history_.clear();
    elbo_history_.reserve(max_iters);

    for (std::size_t iter = 0; iter < max_iters; ++iter) {
      auto [elbo, grad_mu, grad_omega] = computeGradientsAndELBO(g);
      elbo_history_.push_back(elbo);

      // Check convergence
      if (iter > 0) {
        T rel_change =
            std::abs(elbo - elbo_history_[iter - 1]) /
            (std::abs(elbo_history_[iter - 1]) + static_cast<T>(opts_.epsilon));
        if (rel_change < static_cast<T>(opts_.elbo_tol)) {
          break;
        }
      }

      // Adam update (gradient ascent on ELBO)
      adam_.update(params_, grad_mu, grad_omega, opts_);
    }

    return params_;
  }

  // Sample from the fitted variational approximation
  template <std::uniform_random_bit_generator Gen>
  auto sample(Gen& g, std::size_t n) const -> std::vector<State> {
    std::vector<State> samples;
    samples.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
      samples.push_back(params_.sample(g));
    }
    return samples;
  }

  // Get ELBO history for convergence diagnostics
  auto elboHistory() const -> const std::vector<T>& { return elbo_history_; }

  // Access current variational parameters
  auto params() const -> const Variational& { return params_; }

  // Compute ELBO at current parameters (useful for diagnostics)
  template <std::uniform_random_bit_generator Gen>
  auto computeELBO(Gen& g) const -> T {
    T elbo{0};
    Normal<T> std_normal{T{0}, T{1}};

    for (std::size_t s = 0; s < opts_.mc_samples; ++s) {
      // Draw standard normal noise
      State epsilon;
      for (std::size_t i = 0; i < N; ++i) {
        epsilon[i] = std_normal.sample(g);
      }

      // Reparameterization: θ = μ + σε
      State theta = params_.sampleWithNoise(epsilon);

      // Accumulate log joint
      elbo += log_joint_(theta);
    }

    // Average over MC samples and add entropy
    elbo /= static_cast<T>(opts_.mc_samples);
    elbo += params_.entropy();

    return elbo;
  }

 private:
  LogJoint log_joint_;
  GradLogJoint grad_log_joint_;
  ADVIOptions opts_;
  Variational params_;
  AdamState<T, N> adam_;
  std::vector<T> elbo_history_;

  // Compute gradients of ELBO w.r.t. variational parameters
  // Uses reparameterization trick: ∇_φ E_q[f(θ)] = E_ε[∇_φ f(μ + σε)]
  //
  // For mean-field Gaussian:
  //   ∂ELBO/∂μᵢ = E_ε[∂log p/∂θᵢ]
  //   ∂ELBO/∂ωᵢ = E_ε[∂log p/∂θᵢ · σᵢεᵢ] + 1  (the +1 is from ∂H/∂ωᵢ)
  template <std::uniform_random_bit_generator Gen>
  auto computeGradientsAndELBO(Gen& g)
      -> std::tuple<T, State, State> {
    State grad_mu{};
    State grad_omega{};
    T elbo{0};

    for (std::size_t i = 0; i < N; ++i) {
      grad_mu[i] = T{0};
      grad_omega[i] = T{0};
    }

    Normal<T> std_normal{T{0}, T{1}};
    State sigma = params_.sigma();

    for (std::size_t s = 0; s < opts_.mc_samples; ++s) {
      // Draw standard normal noise
      State epsilon;
      for (std::size_t i = 0; i < N; ++i) {
        epsilon[i] = std_normal.sample(g);
      }

      // Reparameterization: θ = μ + σε
      State theta = params_.sampleWithNoise(epsilon);

      // Compute log joint and its gradient
      T lj = log_joint_(theta);
      State grad_lj = grad_log_joint_(theta);

      elbo += lj;

      // Accumulate gradients using chain rule through reparameterization
      // ∂θ/∂μ = 1, ∂θ/∂ω = σε (since σ = exp(ω))
      for (std::size_t i = 0; i < N; ++i) {
        grad_mu[i] += grad_lj[i];
        grad_omega[i] += grad_lj[i] * sigma[i] * epsilon[i];
      }
    }

    // Average over MC samples
    T inv_samples = T{1} / static_cast<T>(opts_.mc_samples);
    for (std::size_t i = 0; i < N; ++i) {
      grad_mu[i] *= inv_samples;
      grad_omega[i] *= inv_samples;
      // Add entropy gradient: ∂H/∂ωᵢ = 1
      grad_omega[i] += T{1};
    }

    elbo *= inv_samples;
    elbo += params_.entropy();

    return {elbo, grad_mu, grad_omega};
  }
};

// =============================================================================
// Factory Functions
// =============================================================================

template <typename T, std::size_t N, typename LogJoint, typename GradLogJoint>
auto makeADVI(LogJoint log_joint, GradLogJoint grad_log_joint,
              ADVIOptions opts = {}) {
  return ADVI<T, N, LogJoint, GradLogJoint>{std::move(log_joint),
                                             std::move(grad_log_joint), opts};
}

// =============================================================================
// Convenience: ADVI with automatic gradient (finite differences)
// =============================================================================
//
// For cases where analytical gradients aren't available. Uses central
// differences for O(h²) accuracy. Note: much slower than analytical gradients.

template <typename T, std::size_t N>
struct FiniteDifferenceGradient {
  std::function<T(const std::array<T, N>&)> f;
  T h = 1e-5;  // Step size

  auto operator()(const std::array<T, N>& theta) const -> std::array<T, N> {
    std::array<T, N> grad;
    std::array<T, N> theta_plus = theta;
    std::array<T, N> theta_minus = theta;

    for (std::size_t i = 0; i < N; ++i) {
      theta_plus[i] = theta[i] + h;
      theta_minus[i] = theta[i] - h;

      grad[i] = (f(theta_plus) - f(theta_minus)) / (T{2} * h);

      theta_plus[i] = theta[i];
      theta_minus[i] = theta[i];
    }
    return grad;
  }
};

template <typename T, std::size_t N, typename LogJoint>
auto makeADVIWithFiniteDiff(LogJoint log_joint, ADVIOptions opts = {},
                             T h = 1e-5) {
  FiniteDifferenceGradient<T, N> grad{log_joint, h};
  return ADVI<T, N, LogJoint, FiniteDifferenceGradient<T, N>>{
      std::move(log_joint), std::move(grad), opts};
}

}  // namespace tempura::bayes
