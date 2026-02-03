#pragma once

#include <array>
#include <cassert>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <random>
#include <span>
#include <tuple>
#include <utility>
#include <vector>

#include "bayes/normal.h"
#include "bayes/uniform.h"

namespace tempura::bayes {

// =============================================================================
// Hamiltonian Monte Carlo (HMC) Kernel
// =============================================================================
//
// HMC exploits Hamiltonian dynamics to make distant proposals that are likely
// to be accepted. Instead of random-walk proposals, it simulates a physical
// system where the negative log-probability is potential energy.
//
// The Hamiltonian: H(q, p) = U(q) + K(p)
//   - U(q) = -log p(q) is the potential energy (negative log-prob)
//   - K(p) = 0.5 * sum(p[i]^2 / m[i]) is kinetic energy
//
// At each step:
//   1. Sample momentum p ~ N(0, M) where M is the mass matrix
//   2. Simulate Hamiltonian dynamics using leapfrog integration
//   3. Accept/reject using Metropolis criterion on total energy
//
// Benefits over Metropolis-Hastings:
//   - Much higher acceptance rates (often 60-90%)
//   - Lower autocorrelation between samples
//   - More efficient exploration of high-dimensional spaces
//
// Requires:
//   - Gradient of log-probability (or negative gradient of potential)
//   - Tuning of step size (ε) and number of leapfrog steps (L)

// =============================================================================
// Concepts
// =============================================================================

template <typename F, typename State, typename T>
concept GradientFn = requires(F f, const State& s) {
  { f(s) } -> std::convertible_to<State>;
};

// =============================================================================
// Leapfrog Integrator
// =============================================================================

// Leapfrog integration for Hamiltonian dynamics
// Preserves volume (symplectic) and is time-reversible, both crucial for HMC
//
// The algorithm:
//   p(t + ε/2) = p(t) - (ε/2) * ∇U(q(t))      half momentum step
//   q(t + ε)   = q(t) + ε * p(t + ε/2) / m    full position step
//   p(t + ε)   = p(t + ε/2) - (ε/2) * ∇U(q(t + ε))  half momentum step
//
// For L steps, the half-steps at boundaries combine into full steps

template <typename T, std::size_t N, typename GradU>
struct LeapfrogIntegrator {
  using State = std::array<T, N>;
  using Momentum = std::array<T, N>;
  using Mass = std::array<T, N>;

  GradU grad_u;        // Gradient of potential energy (negative gradient of log-prob)
  T epsilon;           // Step size
  std::size_t n_steps; // Number of leapfrog steps
  Mass inv_mass;       // Inverse mass matrix (diagonal)

  // Returns (final_position, final_momentum)
  constexpr auto integrate(State q, Momentum p) const -> std::pair<State, Momentum> {
    // Half step for momentum
    State grad = grad_u(q);
    for (std::size_t i = 0; i < N; ++i) {
      p[i] -= (epsilon / T{2}) * grad[i];
    }

    // Alternate full steps for position and momentum
    for (std::size_t step = 0; step < n_steps; ++step) {
      // Full step for position
      for (std::size_t i = 0; i < N; ++i) {
        q[i] += epsilon * p[i] * inv_mass[i];
      }

      // Full step for momentum, except at end
      if (step < n_steps - 1) {
        grad = grad_u(q);
        for (std::size_t i = 0; i < N; ++i) {
          p[i] -= epsilon * grad[i];
        }
      }
    }

    // Half step for momentum at the end
    grad = grad_u(q);
    for (std::size_t i = 0; i < N; ++i) {
      p[i] -= (epsilon / T{2}) * grad[i];
    }

    // Negate momentum for reversibility (makes proposal symmetric)
    for (std::size_t i = 0; i < N; ++i) {
      p[i] = -p[i];
    }

    return {q, p};
  }
};

// =============================================================================
// HMC Kernel
// =============================================================================

template <typename T, std::size_t N, typename LogTarget, typename GradLogTarget>
class HamiltonianMonteCarlo {
 public:
  using State = std::array<T, N>;
  using StateType = State;
  using ProbType = T;
  using Momentum = std::array<T, N>;
  using Mass = std::array<T, N>;

  // Unit mass constructor
  constexpr HamiltonianMonteCarlo(LogTarget log_target, GradLogTarget grad_log_target,
                                   T epsilon, std::size_t n_steps)
      : log_target_{std::move(log_target)},
        grad_log_target_{std::move(grad_log_target)},
        epsilon_{epsilon},
        n_steps_{n_steps},
        mass_{unitMass()},
        inv_mass_{unitMass()} {
    assert(epsilon > T{0} && "step size must be positive");
    assert(n_steps >= 1 && "must take at least one leapfrog step");
  }

  // Custom mass matrix constructor
  constexpr HamiltonianMonteCarlo(LogTarget log_target, GradLogTarget grad_log_target,
                                   T epsilon, std::size_t n_steps, Mass mass)
      : log_target_{std::move(log_target)},
        grad_log_target_{std::move(grad_log_target)},
        epsilon_{epsilon},
        n_steps_{n_steps},
        mass_{mass},
        inv_mass_{invertMass(mass)} {
    assert(epsilon > T{0} && "step size must be positive");
    assert(n_steps >= 1 && "must take at least one leapfrog step");
    for (std::size_t i = 0; i < N; ++i) {
      assert(mass[i] > T{0} && "mass must be positive");
    }
  }

  constexpr auto logProb(const State& state) const -> T {
    return log_target_(state);
  }

  // Single HMC step: returns (new_state, new_log_prob, accepted)
  template <std::uniform_random_bit_generator Generator>
  auto step(const State& current, T current_log_prob, Generator& g)
      -> std::tuple<State, T, bool> {
    // Sample momentum from N(0, M)
    Momentum p = sampleMomentum(g);

    // Compute initial Hamiltonian: H = U + K = -logProb + kinetic
    T current_kinetic = kineticEnergy(p);
    T current_H = -current_log_prob + current_kinetic;

    // Leapfrog integration
    // grad_u = -grad(log_prob) = gradient of potential energy
    auto grad_u = [this](const State& q) -> State {
      State grad_log = grad_log_target_(q);
      State result;
      for (std::size_t i = 0; i < N; ++i) {
        result[i] = -grad_log[i];  // Negate to get gradient of potential
      }
      return result;
    };

    LeapfrogIntegrator<T, N, decltype(grad_u)> integrator{
        grad_u, epsilon_, n_steps_, inv_mass_};

    auto [proposed, proposed_p] = integrator.integrate(current, p);

    // Compute proposed Hamiltonian
    T proposed_log_prob = log_target_(proposed);

    // Reject if outside support
    if (!std::isfinite(proposed_log_prob)) {
      return {current, current_log_prob, false};
    }

    T proposed_kinetic = kineticEnergy(proposed_p);
    T proposed_H = -proposed_log_prob + proposed_kinetic;

    // Metropolis accept/reject based on energy difference
    // Accept with probability min(1, exp(-H_new + H_old))
    T log_accept_prob = current_H - proposed_H;

    bool accept = false;
    if (log_accept_prob >= T{0}) {
      accept = true;
    } else {
      T u = Uniform<T>{T{0}, T{1}}.sample(g);
      accept = std::log(u) < log_accept_prob;
    }

    if (accept) {
      return {proposed, proposed_log_prob, true};
    }
    return {current, current_log_prob, false};
  }

  // Accessors for tuning
  constexpr auto epsilon() const -> T { return epsilon_; }
  constexpr auto nSteps() const -> std::size_t { return n_steps_; }
  constexpr auto mass() const -> const Mass& { return mass_; }

 private:
  LogTarget log_target_;
  GradLogTarget grad_log_target_;
  T epsilon_;
  std::size_t n_steps_;
  Mass mass_;
  Mass inv_mass_;

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

  // K(p) = 0.5 * sum(p[i]^2 / m[i]) = 0.5 * sum(p[i]^2 * inv_mass[i])
  constexpr auto kineticEnergy(const Momentum& p) const -> T {
    T kinetic{0};
    for (std::size_t i = 0; i < N; ++i) {
      kinetic += p[i] * p[i] * inv_mass_[i];
    }
    return kinetic / T{2};
  }

  // Sample momentum: p[i] ~ N(0, m[i])
  template <std::uniform_random_bit_generator Generator>
  auto sampleMomentum(Generator& g) -> Momentum {
    Momentum p;
    for (std::size_t i = 0; i < N; ++i) {
      // Standard normal, then scale by sqrt(mass)
      Normal<T> dist{T{0}, std::sqrt(mass_[i])};
      p[i] = dist.sample(g);
    }
    return p;
  }
};

// =============================================================================
// Factory Functions
// =============================================================================

// Create HMC kernel with unit mass
template <typename T, std::size_t N, typename LogTarget, typename GradLogTarget>
constexpr auto makeHMC(LogTarget log_target, GradLogTarget grad_log_target,
                        T epsilon, std::size_t n_steps) {
  return HamiltonianMonteCarlo<T, N, LogTarget, GradLogTarget>{
      std::move(log_target), std::move(grad_log_target), epsilon, n_steps};
}

// Create HMC kernel with custom diagonal mass matrix
template <typename T, std::size_t N, typename LogTarget, typename GradLogTarget>
constexpr auto makeHMC(LogTarget log_target, GradLogTarget grad_log_target,
                        T epsilon, std::size_t n_steps,
                        std::array<T, N> mass) {
  return HamiltonianMonteCarlo<T, N, LogTarget, GradLogTarget>{
      std::move(log_target), std::move(grad_log_target), epsilon, n_steps, mass};
}

// =============================================================================
// Scalar specialization (1D HMC)
// =============================================================================

template <typename T, typename LogTarget, typename GradLogTarget>
class HamiltonianMonteCarloScalar {
 public:
  using StateType = T;
  using ProbType = T;

  constexpr HamiltonianMonteCarloScalar(LogTarget log_target,
                                         GradLogTarget grad_log_target,
                                         T epsilon, std::size_t n_steps,
                                         T mass = T{1})
      : log_target_{std::move(log_target)},
        grad_log_target_{std::move(grad_log_target)},
        epsilon_{epsilon},
        n_steps_{n_steps},
        mass_{mass},
        inv_mass_{T{1} / mass} {
    assert(epsilon > T{0} && "step size must be positive");
    assert(n_steps >= 1 && "must take at least one leapfrog step");
    assert(mass > T{0} && "mass must be positive");
  }

  constexpr auto logProb(T state) const -> T { return log_target_(state); }

  template <std::uniform_random_bit_generator Generator>
  auto step(T current, T current_log_prob, Generator& g) -> std::tuple<T, T, bool> {
    // Sample momentum
    Normal<T> momentum_dist{T{0}, std::sqrt(mass_)};
    T p = momentum_dist.sample(g);

    // Initial Hamiltonian
    T current_kinetic = p * p * inv_mass_ / T{2};
    T current_H = -current_log_prob + current_kinetic;

    // Leapfrog integration
    T q = current;

    // Half momentum step
    T grad = -grad_log_target_(q);
    p -= (epsilon_ / T{2}) * grad;

    // Full steps
    for (std::size_t step = 0; step < n_steps_; ++step) {
      q += epsilon_ * p * inv_mass_;
      if (step < n_steps_ - 1) {
        grad = -grad_log_target_(q);
        p -= epsilon_ * grad;
      }
    }

    // Half momentum step
    grad = -grad_log_target_(q);
    p -= (epsilon_ / T{2}) * grad;
    p = -p;  // Negate for reversibility

    // Proposed Hamiltonian
    T proposed_log_prob = log_target_(q);
    if (!std::isfinite(proposed_log_prob)) {
      return {current, current_log_prob, false};
    }

    T proposed_kinetic = p * p * inv_mass_ / T{2};
    T proposed_H = -proposed_log_prob + proposed_kinetic;

    // Accept/reject
    T log_accept_prob = current_H - proposed_H;
    bool accept = false;
    if (log_accept_prob >= T{0}) {
      accept = true;
    } else {
      T u = Uniform<T>{T{0}, T{1}}.sample(g);
      accept = std::log(u) < log_accept_prob;
    }

    if (accept) {
      return {q, proposed_log_prob, true};
    }
    return {current, current_log_prob, false};
  }

  constexpr auto epsilon() const -> T { return epsilon_; }
  constexpr auto nSteps() const -> std::size_t { return n_steps_; }
  constexpr auto mass() const -> T { return mass_; }

 private:
  LogTarget log_target_;
  GradLogTarget grad_log_target_;
  T epsilon_;
  std::size_t n_steps_;
  T mass_;
  T inv_mass_;
};

// Scalar factory
template <typename T, typename LogTarget, typename GradLogTarget>
constexpr auto makeHMCScalar(LogTarget log_target, GradLogTarget grad_log_target,
                              T epsilon, std::size_t n_steps, T mass = T{1}) {
  return HamiltonianMonteCarloScalar<T, LogTarget, GradLogTarget>{
      std::move(log_target), std::move(grad_log_target), epsilon, n_steps, mass};
}

// =============================================================================
// Dynamic-sized HMC (runtime-determined dimension)
// =============================================================================
//
// For models where the parameter dimension is determined at runtime (e.g.,
// hierarchical models with indexed latent parameters), use the dynamic variants.
// These use std::vector<T> instead of std::array<T, N>.

template <typename T, typename GradU>
struct LeapfrogIntegratorDynamic {
  using State = std::vector<T>;
  using Momentum = std::vector<T>;
  using Mass = std::vector<T>;

  GradU grad_u;         // Gradient of potential energy (negative gradient of log-prob)
  T epsilon;            // Step size
  std::size_t n_steps;  // Number of leapfrog steps
  Mass inv_mass;        // Inverse mass matrix (diagonal)

  // Returns (final_position, final_momentum)
  auto integrate(State q, Momentum p) const -> std::pair<State, Momentum> {
    std::size_t dim = q.size();
    assert(dim == p.size() && "position and momentum must have same dimension");
    assert(dim == inv_mass.size() && "inverse mass must have same dimension");

    // Half step for momentum
    State grad = grad_u(q);
    for (std::size_t i = 0; i < dim; ++i) {
      p[i] -= (epsilon / T{2}) * grad[i];
    }

    // Alternate full steps for position and momentum
    for (std::size_t step = 0; step < n_steps; ++step) {
      // Full step for position
      for (std::size_t i = 0; i < dim; ++i) {
        q[i] += epsilon * p[i] * inv_mass[i];
      }

      // Full step for momentum, except at end
      if (step < n_steps - 1) {
        grad = grad_u(q);
        for (std::size_t i = 0; i < dim; ++i) {
          p[i] -= epsilon * grad[i];
        }
      }
    }

    // Half step for momentum at the end
    grad = grad_u(q);
    for (std::size_t i = 0; i < dim; ++i) {
      p[i] -= (epsilon / T{2}) * grad[i];
    }

    // Negate momentum for reversibility (makes proposal symmetric)
    for (std::size_t i = 0; i < dim; ++i) {
      p[i] = -p[i];
    }

    return {std::move(q), std::move(p)};
  }
};

template <typename T, typename LogTarget, typename GradLogTarget>
class HamiltonianMonteCarloDynamic {
 public:
  using State = std::vector<T>;
  using StateType = State;
  using ProbType = T;
  using Momentum = std::vector<T>;
  using Mass = std::vector<T>;

  // Unit mass constructor
  HamiltonianMonteCarloDynamic(LogTarget log_target, GradLogTarget grad_log_target,
                                T epsilon, std::size_t n_steps, std::size_t dim)
      : log_target_{std::move(log_target)},
        grad_log_target_{std::move(grad_log_target)},
        epsilon_{epsilon},
        n_steps_{n_steps},
        dim_{dim},
        mass_(dim, T{1}),
        inv_mass_(dim, T{1}) {
    assert(epsilon > T{0} && "step size must be positive");
    assert(n_steps >= 1 && "must take at least one leapfrog step");
    assert(dim >= 1 && "dimension must be at least 1");
  }

  // Custom mass matrix constructor
  HamiltonianMonteCarloDynamic(LogTarget log_target, GradLogTarget grad_log_target,
                                T epsilon, std::size_t n_steps, Mass mass)
      : log_target_{std::move(log_target)},
        grad_log_target_{std::move(grad_log_target)},
        epsilon_{epsilon},
        n_steps_{n_steps},
        dim_{mass.size()},
        mass_{std::move(mass)},
        inv_mass_(dim_) {
    assert(epsilon > T{0} && "step size must be positive");
    assert(n_steps >= 1 && "must take at least one leapfrog step");
    assert(dim_ >= 1 && "dimension must be at least 1");
    for (std::size_t i = 0; i < dim_; ++i) {
      assert(mass_[i] > T{0} && "mass must be positive");
      inv_mass_[i] = T{1} / mass_[i];
    }
  }

  auto logProb(std::span<const T> state) const -> T {
    return log_target_(state);
  }

  // Single HMC step: returns (new_state, new_log_prob, accepted)
  template <std::uniform_random_bit_generator Generator>
  auto step(const State& current, T current_log_prob, Generator& g)
      -> std::tuple<State, T, bool> {
    // Sample momentum from N(0, M)
    Momentum p = sampleMomentum(g);

    // Compute initial Hamiltonian: H = U + K = -logProb + kinetic
    T current_kinetic = kineticEnergy(p);
    T current_H = -current_log_prob + current_kinetic;

    // Leapfrog integration
    // grad_u = -grad(log_prob) = gradient of potential energy
    auto grad_u = [this](const State& q) -> State {
      State grad_log = grad_log_target_(q);
      for (auto& val : grad_log) {
        val = -val;  // Negate to get gradient of potential
      }
      return grad_log;
    };

    LeapfrogIntegratorDynamic<T, decltype(grad_u)> integrator{
        grad_u, epsilon_, n_steps_, inv_mass_};

    auto [proposed, proposed_p] = integrator.integrate(current, p);

    // Compute proposed Hamiltonian
    T proposed_log_prob = log_target_(proposed);

    // Reject if outside support
    if (!std::isfinite(proposed_log_prob)) {
      return {current, current_log_prob, false};
    }

    T proposed_kinetic = kineticEnergy(proposed_p);
    T proposed_H = -proposed_log_prob + proposed_kinetic;

    // Metropolis accept/reject based on energy difference
    // Accept with probability min(1, exp(-H_new + H_old))
    T log_accept_prob = current_H - proposed_H;

    bool accept = false;
    if (log_accept_prob >= T{0}) {
      accept = true;
    } else {
      T u = Uniform<T>{T{0}, T{1}}.sample(g);
      accept = std::log(u) < log_accept_prob;
    }

    if (accept) {
      return {std::move(proposed), proposed_log_prob, true};
    }
    return {current, current_log_prob, false};
  }

  // Accessors for tuning
  auto epsilon() const -> T { return epsilon_; }
  auto nSteps() const -> std::size_t { return n_steps_; }
  auto dim() const -> std::size_t { return dim_; }
  auto mass() const -> const Mass& { return mass_; }

 private:
  LogTarget log_target_;
  GradLogTarget grad_log_target_;
  T epsilon_;
  std::size_t n_steps_;
  std::size_t dim_;
  Mass mass_;
  Mass inv_mass_;

  // K(p) = 0.5 * sum(p[i]^2 / m[i]) = 0.5 * sum(p[i]^2 * inv_mass[i])
  auto kineticEnergy(const Momentum& p) const -> T {
    T kinetic{0};
    for (std::size_t i = 0; i < dim_; ++i) {
      kinetic += p[i] * p[i] * inv_mass_[i];
    }
    return kinetic / T{2};
  }

  // Sample momentum: p[i] ~ N(0, m[i])
  template <std::uniform_random_bit_generator Generator>
  auto sampleMomentum(Generator& g) -> Momentum {
    Momentum p(dim_);
    for (std::size_t i = 0; i < dim_; ++i) {
      // Standard normal, then scale by sqrt(mass)
      Normal<T> dist{T{0}, std::sqrt(mass_[i])};
      p[i] = dist.sample(g);
    }
    return p;
  }
};

// Factory function for dynamic HMC with unit mass
template <typename T, typename LogTarget, typename GradLogTarget>
auto makeHMCDynamic(LogTarget log_target, GradLogTarget grad_log_target,
                    T epsilon, std::size_t n_steps, std::size_t dim) {
  return HamiltonianMonteCarloDynamic<T, LogTarget, GradLogTarget>{
      std::move(log_target), std::move(grad_log_target), epsilon, n_steps, dim};
}

// Factory function for dynamic HMC with custom diagonal mass matrix
template <typename T, typename LogTarget, typename GradLogTarget>
auto makeHMCDynamic(LogTarget log_target, GradLogTarget grad_log_target,
                    T epsilon, std::size_t n_steps, std::vector<T> mass) {
  return HamiltonianMonteCarloDynamic<T, LogTarget, GradLogTarget>{
      std::move(log_target), std::move(grad_log_target), epsilon, n_steps,
      std::move(mass)};
}

}  // namespace tempura::bayes
