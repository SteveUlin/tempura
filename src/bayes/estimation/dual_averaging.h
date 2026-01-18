#pragma once

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <numbers>
#include <random>
#include <tuple>
#include <utility>
#include <vector>

#include "bayes/normal.h"
#include "bayes/uniform.h"

namespace tempura::bayes {

// =============================================================================
// Dual Averaging Step Size Adaptation (Nesterov 2009, Stan adaptation)
// =============================================================================
//
// During HMC warmup, automatically tunes the step size to achieve a target
// acceptance rate (typically 0.8 for HMC). The algorithm maintains a running
// average that converges to the optimal step size.
//
// Key insight: acceptance probability is monotonically decreasing in step size,
// so we can use stochastic optimization to find the step size achieving target.

// =============================================================================
// Options
// =============================================================================

struct DualAveragingOptions {
  double target_accept = 0.8;   // Target acceptance probability
  double gamma = 0.05;          // Adaptation regularization scale
  double t0 = 10.0;             // Initial iteration offset (stabilizes early)
  double kappa = 0.75;          // Step size decay exponent (0.5 < kappa <= 1)
  double mu = std::numbers::ln10;  // Initial step size guess (log scale)
};

// =============================================================================
// Dual Averaging State
// =============================================================================

template <typename T>
struct DualAveragingState {
  T log_epsilon;      // Current log step size
  T log_epsilon_bar;  // Averaged log step size (final result after warmup)
  T h_bar;            // Accumulated acceptance statistic
  std::size_t m;      // Iteration count

  // Initialize with initial epsilon
  static auto init(T epsilon) -> DualAveragingState {
    assert(epsilon > T{0} && "initial epsilon must be positive");
    return DualAveragingState{
        .log_epsilon = std::log(epsilon),
        .log_epsilon_bar = T{0},
        .h_bar = T{0},
        .m = 0};
  }

  // Update after one HMC iteration
  // accept_stat: acceptance statistic from the most recent HMC step (clamped to [0,1])
  void update(T accept_stat, const DualAveragingOptions& opts) {
    ++m;
    T m_t = static_cast<T>(m);

    // Weight for this iteration's contribution
    T eta = T{1} / (m_t + static_cast<T>(opts.t0));

    // Update running average of (target - actual) acceptance
    // h_bar tracks how far we are from target acceptance
    h_bar = (T{1} - eta) * h_bar +
            eta * (static_cast<T>(opts.target_accept) - accept_stat);

    // Update log step size: move in direction that reduces h_bar
    // Large h_bar (accept too low) -> decrease step size
    // Small/negative h_bar (accept too high) -> increase step size
    log_epsilon = static_cast<T>(opts.mu) -
                  std::sqrt(m_t) / static_cast<T>(opts.gamma) * h_bar;

    // Clamp to prevent runaway values
    log_epsilon = std::clamp(log_epsilon, T{-20}, T{20});

    // Update running average of log step size (for final estimate)
    // Uses polynomial averaging with decay exponent kappa
    T m_kappa = std::pow(m_t, -static_cast<T>(opts.kappa));
    log_epsilon_bar =
        m_kappa * log_epsilon + (T{1} - m_kappa) * log_epsilon_bar;
  }

  // Get current step size (for use during warmup)
  auto epsilon() const -> T { return std::exp(log_epsilon); }

  // Get final averaged step size (use after warmup completes)
  auto finalEpsilon() const -> T { return std::exp(log_epsilon_bar); }
};

// =============================================================================
// Adaptive HMC with Dual Averaging
// =============================================================================

template <typename T, std::size_t N, typename LogTarget, typename GradLogTarget>
class AdaptiveHMC {
 public:
  using State = std::array<T, N>;
  using Momentum = std::array<T, N>;
  using Mass = std::array<T, N>;

  AdaptiveHMC(LogTarget log_target, GradLogTarget grad_log_target,
              T initial_epsilon, std::size_t n_steps,
              DualAveragingOptions opts = {})
      : log_target_{std::move(log_target)},
        grad_log_target_{std::move(grad_log_target)},
        epsilon_{initial_epsilon},
        n_steps_{n_steps},
        opts_{opts},
        mass_{unitMass()},
        inv_mass_{unitMass()},
        da_state_{DualAveragingState<T>::init(initial_epsilon)} {
    assert(initial_epsilon > T{0} && "step size must be positive");
    assert(n_steps >= 1 && "must take at least one leapfrog step");
    // Set mu based on initial epsilon (Stan convention: mu = log(10 * epsilon))
    opts_.mu = std::log(T{10} * initial_epsilon);
  }

  AdaptiveHMC(LogTarget log_target, GradLogTarget grad_log_target,
              T initial_epsilon, std::size_t n_steps, Mass mass,
              DualAveragingOptions opts = {})
      : log_target_{std::move(log_target)},
        grad_log_target_{std::move(grad_log_target)},
        epsilon_{initial_epsilon},
        n_steps_{n_steps},
        opts_{opts},
        mass_{mass},
        inv_mass_{invertMass(mass)},
        da_state_{DualAveragingState<T>::init(initial_epsilon)} {
    assert(initial_epsilon > T{0} && "step size must be positive");
    assert(n_steps >= 1 && "must take at least one leapfrog step");
    for (std::size_t i = 0; i < N; ++i) {
      assert(mass[i] > T{0} && "mass must be positive");
    }
    opts_.mu = std::log(T{10} * initial_epsilon);
  }

  // Warmup phase: run HMC while adapting step size
  template <std::uniform_random_bit_generator Gen>
  void warmup(State& state, Gen& g, std::size_t n_warmup) {
    T log_prob = log_target_(state);

    warmup_accepted_ = 0;
    warmup_total_ = 0;

    for (std::size_t i = 0; i < n_warmup; ++i) {
      T current_eps = da_state_.epsilon();
      auto [new_state, new_log_prob, accept_stat, accepted] =
          hmcStep(state, log_prob, current_eps, g);

      // Update dual averaging with the acceptance statistic
      da_state_.update(accept_stat, opts_);

      // Update chain state
      state = new_state;
      log_prob = new_log_prob;

      if (accepted) ++warmup_accepted_;
      ++warmup_total_;
    }

    // Set final step size from averaged estimate
    epsilon_ = da_state_.finalEpsilon();
  }

  // Sampling phase: uses fixed step size (no adaptation)
  template <std::uniform_random_bit_generator Gen>
  auto sample(State& state, Gen& g, std::size_t n_samples)
      -> std::vector<State> {
    std::vector<State> samples;
    samples.reserve(n_samples);

    T log_prob = log_target_(state);

    sample_accepted_ = 0;
    sample_total_ = 0;

    for (std::size_t i = 0; i < n_samples; ++i) {
      auto [new_state, new_log_prob, accept_stat, accepted] =
          hmcStep(state, log_prob, epsilon_, g);
      state = new_state;
      log_prob = new_log_prob;
      samples.push_back(state);

      if (accepted) ++sample_accepted_;
      ++sample_total_;
    }

    return samples;
  }

  // Get adapted step size
  auto epsilon() const -> T { return epsilon_; }

  // Get acceptance rate during warmup
  auto warmupAcceptanceRate() const -> double {
    if (warmup_total_ == 0) return 0.0;
    return static_cast<double>(warmup_accepted_) /
           static_cast<double>(warmup_total_);
  }

  // Get acceptance rate during sampling
  auto acceptanceRate() const -> double {
    if (sample_total_ == 0) return 0.0;
    return static_cast<double>(sample_accepted_) /
           static_cast<double>(sample_total_);
  }

  // Access dual averaging state
  auto daState() const -> const DualAveragingState<T>& { return da_state_; }

  // Access options
  auto options() const -> const DualAveragingOptions& { return opts_; }

 private:
  LogTarget log_target_;
  GradLogTarget grad_log_target_;
  T epsilon_;
  std::size_t n_steps_;
  DualAveragingOptions opts_;
  Mass mass_;
  Mass inv_mass_;
  DualAveragingState<T> da_state_;
  std::size_t warmup_accepted_ = 0;
  std::size_t warmup_total_ = 0;
  std::size_t sample_accepted_ = 0;
  std::size_t sample_total_ = 0;

  static constexpr auto unitMass() -> Mass {
    Mass m{};
    for (std::size_t i = 0; i < N; ++i) {
      m[i] = T{1};
    }
    return m;
  }

  static constexpr auto invertMass(const Mass& m) -> Mass {
    Mass inv{};
    for (std::size_t i = 0; i < N; ++i) {
      inv[i] = T{1} / m[i];
    }
    return inv;
  }

  // K(p) = 0.5 * sum(p[i]^2 / m[i])
  constexpr auto kineticEnergy(const Momentum& p) const -> T {
    T kinetic{0};
    for (std::size_t i = 0; i < N; ++i) {
      kinetic += p[i] * p[i] * inv_mass_[i];
    }
    return kinetic / T{2};
  }

  // Sample momentum: p[i] ~ N(0, m[i])
  template <std::uniform_random_bit_generator Gen>
  auto sampleMomentum(Gen& g) -> Momentum {
    Momentum p;
    for (std::size_t i = 0; i < N; ++i) {
      Normal<T> dist{T{0}, std::sqrt(mass_[i])};
      p[i] = dist.sample(g);
    }
    return p;
  }

  // Leapfrog integration
  auto leapfrog(State q, Momentum p, T eps) const -> std::pair<State, Momentum> {
    // Compute gradient of potential = -gradient of log target
    auto grad_u = [this](const State& x) -> State {
      State grad_log = grad_log_target_(x);
      State result;
      for (std::size_t i = 0; i < N; ++i) {
        result[i] = -grad_log[i];
      }
      return result;
    };

    // Half step for momentum
    State grad = grad_u(q);
    for (std::size_t i = 0; i < N; ++i) {
      p[i] -= (eps / T{2}) * grad[i];
    }

    // Full steps
    for (std::size_t step = 0; step < n_steps_; ++step) {
      // Full step for position
      for (std::size_t i = 0; i < N; ++i) {
        q[i] += eps * p[i] * inv_mass_[i];
      }

      // Full step for momentum except at end
      if (step < n_steps_ - 1) {
        grad = grad_u(q);
        for (std::size_t i = 0; i < N; ++i) {
          p[i] -= eps * grad[i];
        }
      }
    }

    // Half step for momentum at end
    grad = grad_u(q);
    for (std::size_t i = 0; i < N; ++i) {
      p[i] -= (eps / T{2}) * grad[i];
    }

    // Negate momentum for reversibility
    for (std::size_t i = 0; i < N; ++i) {
      p[i] = -p[i];
    }

    return {q, p};
  }

  // Single HMC step returning (new_state, new_log_prob, accept_stat, accepted)
  // accept_stat is the acceptance probability before the uniform draw (for dual averaging)
  template <std::uniform_random_bit_generator Gen>
  auto hmcStep(const State& current, T current_log_prob, T eps, Gen& g)
      -> std::tuple<State, T, T, bool> {
    // Sample momentum
    Momentum p = sampleMomentum(g);

    // Initial Hamiltonian
    T current_kinetic = kineticEnergy(p);
    T current_H = -current_log_prob + current_kinetic;

    // Leapfrog integration
    auto [proposed, proposed_p] = leapfrog(current, p, eps);

    // Proposed log probability
    T proposed_log_prob = log_target_(proposed);

    // Check for numerical issues
    if (!std::isfinite(proposed_log_prob)) {
      return {current, current_log_prob, T{0}, false};
    }

    // Proposed Hamiltonian
    T proposed_kinetic = kineticEnergy(proposed_p);
    T proposed_H = -proposed_log_prob + proposed_kinetic;

    // Accept probability = min(1, exp(-dH)) = min(1, exp(current_H - proposed_H))
    T log_accept = current_H - proposed_H;
    T accept_stat = std::min(T{1}, std::exp(log_accept));
    if (!std::isfinite(accept_stat)) {
      accept_stat = T{0};
    }

    // Metropolis accept/reject
    bool accepted = false;
    if (log_accept >= T{0}) {
      accepted = true;
    } else {
      T u = Uniform<T>{T{0}, T{1}}.sample(g);
      accepted = std::log(u) < log_accept;
    }

    if (accepted) {
      return {proposed, proposed_log_prob, accept_stat, true};
    }
    return {current, current_log_prob, accept_stat, false};
  }
};

// =============================================================================
// Factory Functions
// =============================================================================

template <typename T, std::size_t N, typename LogTarget, typename GradLogTarget>
auto makeAdaptiveHMC(LogTarget log_target, GradLogTarget grad_log_target,
                     T initial_epsilon, std::size_t n_steps,
                     DualAveragingOptions opts = {}) {
  return AdaptiveHMC<T, N, LogTarget, GradLogTarget>{
      std::move(log_target), std::move(grad_log_target), initial_epsilon,
      n_steps, opts};
}

template <typename T, std::size_t N, typename LogTarget, typename GradLogTarget>
auto makeAdaptiveHMC(LogTarget log_target, GradLogTarget grad_log_target,
                     T initial_epsilon, std::size_t n_steps,
                     std::array<T, N> mass, DualAveragingOptions opts = {}) {
  return AdaptiveHMC<T, N, LogTarget, GradLogTarget>{
      std::move(log_target), std::move(grad_log_target), initial_epsilon,
      n_steps, mass, opts};
}

}  // namespace tempura::bayes
