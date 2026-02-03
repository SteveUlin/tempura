#pragma once

#include <array>
#include <cstddef>
#include <random>
#include <tuple>
#include <vector>

#include "bayes/estimation/hmc.h"
#include "symbolic4/mcmc/transforms.h"

// ============================================================================
// hmc_adapter.h - Integrate TransformedPosterior with HMC
// ============================================================================
//
// Provides helper functions to create HMC kernels from TransformedPosterior,
// automatically handling parameter transforms and Jacobian corrections.
//
// Usage:
//   // Define model with constrained parameters
//   auto mu = normal(lit(0.0), lit(5.0));
//   auto sigma = halfNormal(lit(2.0));
//   auto y = normal(mu, sigma);
//
//   // Create transformed posterior with automatic log-transform for sigma
//   auto posterior = makeTransformedPosterior(
//       logProb(mu, sigma, y),
//       unconstrained(mu),
//       positive(sigma)
//   ).observe(y = 3.5);
//
//   // Create HMC kernel - operates in unconstrained space
//   auto hmc = makeHMCFromPosterior<double>(posterior, 0.1, 10);
//
//   // Sample in unconstrained space, transform back to constrained
//   auto [z, lp, accepted] = hmc.step(z_current, lp_current, rng);
//   auto x = posterior.transform(z);  // Get constrained values
//
// ============================================================================

namespace tempura::symbolic4 {

// ============================================================================
// HMC Adapter - Wraps TransformedPosterior for use with HMC
// ============================================================================

template <typename T, typename TransformedPosteriorT>
class HMCAdapter {
 public:
  static constexpr std::size_t NumParams = TransformedPosteriorT::NumParams;
  using State = std::array<T, NumParams>;
  using StateType = State;
  using ProbType = T;

  constexpr HMCAdapter(TransformedPosteriorT posterior, T epsilon, std::size_t n_steps)
      : posterior_{posterior}, epsilon_{epsilon}, n_steps_{n_steps} {}

  // Evaluate log-probability in unconstrained space (includes Jacobian)
  auto logProb(const State& z) const -> T {
    return static_cast<T>(posterior_.logProb(toDoubleState(z)));
  }

  // Evaluate gradient in unconstrained space
  auto gradient(const State& z) const -> State {
    auto grad = posterior_.gradient(toDoubleState(z));
    return fromDoubleState(grad);
  }

  // Transform from unconstrained z to constrained x
  auto transform(const State& z) const -> State {
    auto x = posterior_.transform(toDoubleState(z));
    return fromDoubleState(x);
  }

  // Inverse: constrained x to unconstrained z
  auto inverse(const State& x) const -> State {
    auto z = posterior_.inverse(toDoubleState(x));
    return fromDoubleState(z);
  }

  // Single HMC step
  template <std::uniform_random_bit_generator Generator>
  auto step(const State& current, T current_log_prob, Generator& g)
      -> std::tuple<State, T, bool> {
    auto log_target = [this](const State& s) { return logProb(s); };
    auto grad_log_target = [this](const State& s) { return gradient(s); };

    auto hmc = bayes::makeHMC<T, NumParams>(log_target, grad_log_target, epsilon_, n_steps_);
    return hmc.step(current, current_log_prob, g);
  }

  // Run multiple HMC steps, collecting samples
  template <std::uniform_random_bit_generator Generator>
  auto sample(const State& initial, std::size_t n_samples, std::size_t n_warmup,
              Generator& g) -> std::vector<State> {
    std::vector<State> samples;
    samples.reserve(n_samples);

    State current = initial;
    T current_lp = logProb(current);

    // Warmup
    for (std::size_t i = 0; i < n_warmup; ++i) {
      auto [next, next_lp, accepted] = step(current, current_lp, g);
      current = next;
      current_lp = next_lp;
    }

    // Sampling
    for (std::size_t i = 0; i < n_samples; ++i) {
      auto [next, next_lp, accepted] = step(current, current_lp, g);
      current = next;
      current_lp = next_lp;
      samples.push_back(transform(current));  // Store in constrained space
    }

    return samples;
  }

  // Accessors
  constexpr auto epsilon() const -> T { return epsilon_; }
  constexpr auto nSteps() const -> std::size_t { return n_steps_; }

 private:
  TransformedPosteriorT posterior_;
  T epsilon_;
  std::size_t n_steps_;

  // Convert State to double array (if T != double)
  static auto toDoubleState(const State& s) -> std::array<double, NumParams> {
    if constexpr (std::is_same_v<T, double>) {
      return s;
    } else {
      std::array<double, NumParams> result;
      for (std::size_t i = 0; i < NumParams; ++i) {
        result[i] = static_cast<double>(s[i]);
      }
      return result;
    }
  }

  // Convert double array to State (if T != double)
  static auto fromDoubleState(const std::array<double, NumParams>& s) -> State {
    if constexpr (std::is_same_v<T, double>) {
      return s;
    } else {
      State result;
      for (std::size_t i = 0; i < NumParams; ++i) {
        result[i] = static_cast<T>(s[i]);
      }
      return result;
    }
  }
};

// ============================================================================
// Factory function
// ============================================================================

// Create HMC adapter from TransformedPosterior
template <typename T, typename TransformedPosteriorT>
auto makeHMCFromPosterior(TransformedPosteriorT posterior, T epsilon, std::size_t n_steps) {
  return HMCAdapter<T, TransformedPosteriorT>{posterior, epsilon, n_steps};
}

// ============================================================================
// Convenience: Sample and return constrained values
// ============================================================================

template <typename T, typename TransformedPosteriorT, std::uniform_random_bit_generator Generator>
auto samplePosterior(TransformedPosteriorT posterior,
                     const std::array<double, TransformedPosteriorT::NumParams>& initial_constrained,
                     std::size_t n_samples, std::size_t n_warmup,
                     T epsilon, std::size_t n_steps,
                     Generator& g) {
  auto adapter = makeHMCFromPosterior<T>(posterior, epsilon, n_steps);

  // Convert initial values to unconstrained space
  auto initial_unconstrained = posterior.inverse(initial_constrained);

  // Sample
  return adapter.sample(initial_unconstrained, n_samples, n_warmup, g);
}

}  // namespace tempura::symbolic4
