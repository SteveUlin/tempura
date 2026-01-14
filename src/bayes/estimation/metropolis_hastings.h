#pragma once

#include <array>
#include <cassert>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <limits>
#include <random>
#include <tuple>
#include <utility>
#include <vector>

#include "bayes/normal.h"
#include "bayes/uniform.h"

namespace tempura::bayes {

// =============================================================================
// Concepts
// =============================================================================

template <typename P, typename State, typename Gen>
concept Proposal = requires(P p, const State& s, Gen& g) {
  { p(s, g) } -> std::convertible_to<State>;
};

template <typename F, typename State, typename Prob>
concept LogProbFn = requires(F f, const State& s) {
  { f(s) } -> std::convertible_to<Prob>;
};

// =============================================================================
// Metropolis-Hastings Kernel
// =============================================================================

// Metropolis-Hastings MCMC kernel
//
// Defines the transition rule for a Markov chain whose stationary distribution
// is the target p(x). This is a "kernel" - it knows how to take one step but
// doesn't manage chain state. Use with Chain<> to run the sampler.
//
// The algorithm:
//   1. From current state x, propose x' ~ q(x'|x)
//   2. Accept x' with probability α = min(1, [p(x')q(x|x')] / [p(x)q(x'|x)])
//   3. If accepted, next state is x'; otherwise repeat x
//
// For symmetric proposals q(x'|x) = q(x|x'), α simplifies to min(1, p(x')/p(x)).
// We work in log-space to avoid underflow.
//
// Usage:
//   auto kernel = makeMetropolisHastings<double>(
//       [](double x) { return -x*x/2; },  // log p(x) ∝ N(0,1)
//       GaussianWalk{0.5}
//   );
//   Chain chain{kernel, generator, 0.0};
//   chain.advance(1000);  // burn-in
//   auto samples = chain.take(10000);

template <typename State, typename LogTarget, typename Proposal,
          typename Prob = double>
class MetropolisHastings {
 public:
  using StateType = State;
  using ProbType = Prob;

  constexpr MetropolisHastings(LogTarget log_target, Proposal propose)
      : log_target_{std::move(log_target)}, propose_{std::move(propose)} {}

  // Evaluate log-probability of a state
  constexpr auto logProb(const State& state) const -> Prob {
    return log_target_(state);
  }

  // Single MH step: returns (new_state, new_log_prob, accepted)
  // Not const: proposal may cache internal state (e.g., Box-Muller pairs)
  template <std::uniform_random_bit_generator Generator>
  auto step(const State& current, Prob current_log_prob, Generator& g)
      -> std::tuple<State, Prob, bool> {
    State proposed = propose_(current, g);
    Prob proposed_log_prob = log_target_(proposed);

    // Reject if proposed log-prob is -inf or NaN (outside support)
    if (!std::isfinite(proposed_log_prob)) {
      return {current, current_log_prob, false};
    }

    // log(α) = log p(x') - log p(x) for symmetric proposals
    Prob log_alpha = proposed_log_prob - current_log_prob;

    // Accept if log(u) < log(α) where u ~ Uniform(0,1)
    bool accept = false;
    if (log_alpha >= Prob{0}) {
      accept = true;
    } else {
      Prob u = Uniform<Prob>{Prob{0}, Prob{1}}.sample(g);
      accept = std::log(u) < log_alpha;
    }

    if (accept) {
      return {proposed, proposed_log_prob, true};
    }
    return {current, current_log_prob, false};
  }

 private:
  LogTarget log_target_;
  Proposal propose_;
};

// Helper to create MH kernel with explicit state type
template <typename T, typename LogTarget, typename Proposal>
constexpr auto makeMetropolisHastings(LogTarget log_target, Proposal propose) {
  return MetropolisHastings<T, LogTarget, Proposal>{std::move(log_target),
                                                    std::move(propose)};
}

// =============================================================================
// Chain - manages MCMC state and provides iteration
// =============================================================================

// Chain wraps an MCMC kernel and manages the running state.
// Separates "what is the transition rule" from "run the chain".
//
// Usage:
//   Chain chain{kernel, generator, initial_state};
//   chain.advance(1000);           // burn-in (discard samples)
//   auto samples = chain.take(5000);  // collect samples
//   auto [more, rate] = chain.takeWithStats(5000);  // with acceptance tracking

template <typename Kernel, typename Generator>
class Chain {
 public:
  using State = typename Kernel::StateType;
  using Prob = typename Kernel::ProbType;

  Chain(Kernel kernel, Generator gen, State initial)
      : kernel_{std::move(kernel)},
        gen_{std::move(gen)},
        state_{std::move(initial)},
        log_prob_{kernel_.logProb(state_)} {
    assert(std::isfinite(log_prob_) && "initial state must be in target support");
  }

  // Current chain state
  auto state() const -> const State& { return state_; }
  auto logProb() const -> Prob { return log_prob_; }

  // Set chain state (for checkpointing, parallel tempering, etc.)
  void setState(State s) {
    state_ = std::move(s);
    log_prob_ = kernel_.logProb(state_);
  }

  // Advance the chain by n steps (discarding samples)
  void advance(std::size_t n) {
    for (std::size_t i = 0; i < n; ++i) {
      auto [next, next_lp, accepted] = kernel_.step(state_, log_prob_, gen_);
      state_ = next;
      log_prob_ = next_lp;
      if (accepted) ++accepted_;
      ++total_;
    }
  }

  // Take one step and return the new state
  auto next() -> State {
    auto [next_state, next_lp, accepted] =
        kernel_.step(state_, log_prob_, gen_);
    state_ = next_state;
    log_prob_ = next_lp;
    if (accepted) ++accepted_;
    ++total_;
    return state_;
  }

  // Collect n samples with optional thinning
  auto take(std::size_t n, std::size_t thin = 1) -> std::vector<State> {
    assert(thin >= 1 && "thinning must be at least 1");

    std::vector<State> samples;
    samples.reserve(n);

    std::size_t steps_since_sample = 0;
    while (samples.size() < n) {
      auto [next_state, next_lp, accepted] =
          kernel_.step(state_, log_prob_, gen_);
      state_ = next_state;
      log_prob_ = next_lp;
      if (accepted) ++accepted_;
      ++total_;

      ++steps_since_sample;
      if (steps_since_sample >= thin) {
        samples.push_back(state_);
        steps_since_sample = 0;
      }
    }

    return samples;
  }

  // Collect n samples and return acceptance rate for this batch
  auto takeWithStats(std::size_t n, std::size_t thin = 1)
      -> std::pair<std::vector<State>, double> {
    std::size_t before_accepted = accepted_;
    std::size_t before_total = total_;

    auto samples = take(n, thin);

    std::size_t batch_accepted = accepted_ - before_accepted;
    std::size_t batch_total = total_ - before_total;
    double rate = static_cast<double>(batch_accepted) /
                  static_cast<double>(batch_total);

    return {std::move(samples), rate};
  }

  // Cumulative acceptance statistics
  auto acceptanceRate() const -> double {
    if (total_ == 0) return 0.0;
    return static_cast<double>(accepted_) / static_cast<double>(total_);
  }

  auto totalSteps() const -> std::size_t { return total_; }
  auto acceptedSteps() const -> std::size_t { return accepted_; }

  // Reset statistics (but keep chain state)
  void resetStats() {
    accepted_ = 0;
    total_ = 0;
  }

 private:
  Kernel kernel_;
  Generator gen_;
  State state_;
  Prob log_prob_;
  std::size_t accepted_ = 0;
  std::size_t total_ = 0;
};

// Deduction guide
template <typename Kernel, typename Generator, typename State>
Chain(Kernel, Generator, State) -> Chain<Kernel, Generator>;

// =============================================================================
// Scalar Proposal Distributions
// =============================================================================

// Symmetric uniform walk: x' = x + ε, ε ~ Uniform(-δ, δ)
template <typename T = double>
class UniformWalk {
 public:
  constexpr explicit UniformWalk(T delta) : delta_{delta} {
    assert(delta > T{0} && "step size must be positive");
  }

  template <std::uniform_random_bit_generator Generator>
  auto operator()(T current, Generator& g) const -> T {
    Uniform<T> dist{-delta_, delta_};
    return current + dist.sample(g);
  }

 private:
  T delta_;
};

// Symmetric Gaussian walk: x' = x + ε, ε ~ N(0, σ²)
template <typename T = double>
class GaussianWalk {
 public:
  constexpr explicit GaussianWalk(T sigma) : dist_{T{0}, sigma} {
    assert(sigma > T{0} && "σ must be positive");
  }

  template <std::uniform_random_bit_generator Generator>
  auto operator()(T current, Generator& g) -> T {
    return current + dist_.sample(g);
  }

 private:
  Normal<T> dist_;  // Caches Box-Muller pairs
};

// =============================================================================
// N-Dimensional Proposal Distributions (for std::array)
// =============================================================================

// Independent Gaussian walk in each dimension
template <typename T, std::size_t N>
class GaussianWalkND {
 public:
  constexpr explicit GaussianWalkND(T sigma) : dist_{T{0}, sigma} {
    assert(sigma > T{0} && "σ must be positive");
  }

  template <std::uniform_random_bit_generator Generator>
  auto operator()(const std::array<T, N>& current, Generator& g)
      -> std::array<T, N> {
    std::array<T, N> result;
    for (std::size_t i = 0; i < N; ++i) {
      result[i] = current[i] + dist_.sample(g);
    }
    return result;
  }

 private:
  Normal<T> dist_;
};

// Independent uniform walk in each dimension
template <typename T, std::size_t N>
class UniformWalkND {
 public:
  constexpr explicit UniformWalkND(T delta) : delta_{delta} {
    assert(delta > T{0} && "step size must be positive");
  }

  template <std::uniform_random_bit_generator Generator>
  auto operator()(const std::array<T, N>& current, Generator& g) const
      -> std::array<T, N> {
    Uniform<T> dist{-delta_, delta_};
    std::array<T, N> result;
    for (std::size_t i = 0; i < N; ++i) {
      result[i] = current[i] + dist.sample(g);
    }
    return result;
  }

 private:
  T delta_;
};

// =============================================================================
// Tuple Proposal Distributions
// =============================================================================

// Apply a tuple of proposals element-wise: TupleWalk{p1, p2, p3}
// Each proposal is applied to its corresponding tuple element.
template <typename... Proposals>
class TupleWalk {
 public:
  constexpr TupleWalk(Proposals... proposals)
      : proposals_{std::move(proposals)...} {}

  template <typename... Ts, std::uniform_random_bit_generator Generator>
  auto operator()(const std::tuple<Ts...>& current, Generator& g)
      -> std::tuple<Ts...> {
    return applyImpl(current, g, std::index_sequence_for<Ts...>{});
  }

 private:
  std::tuple<Proposals...> proposals_;

  template <typename... Ts, std::uniform_random_bit_generator Generator,
            std::size_t... Is>
  auto applyImpl(const std::tuple<Ts...>& current, Generator& g,
                 std::index_sequence<Is...>) -> std::tuple<Ts...> {
    return {std::get<Is>(proposals_)(std::get<Is>(current), g)...};
  }
};

// Deduction guide
template <typename... Proposals>
TupleWalk(Proposals...) -> TupleWalk<Proposals...>;

// Apply the same proposal to all elements of a tuple
template <typename Proposal>
class HomogeneousWalk {
 public:
  constexpr explicit HomogeneousWalk(Proposal proposal)
      : proposal_{std::move(proposal)} {}

  template <typename... Ts, std::uniform_random_bit_generator Generator>
  auto operator()(const std::tuple<Ts...>& current, Generator& g)
      -> std::tuple<Ts...> {
    return applyImpl(current, g, std::index_sequence_for<Ts...>{});
  }

 private:
  Proposal proposal_;

  template <typename... Ts, std::uniform_random_bit_generator Generator,
            std::size_t... Is>
  auto applyImpl(const std::tuple<Ts...>& current, Generator& g,
                 std::index_sequence<Is...>) -> std::tuple<Ts...> {
    return {proposal_(std::get<Is>(current), g)...};
  }
};

// Deduction guide
template <typename Proposal>
HomogeneousWalk(Proposal) -> HomogeneousWalk<Proposal>;

}  // namespace tempura::bayes
