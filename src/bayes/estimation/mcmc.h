#pragma once

#include <array>
#include <cassert>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <iterator>
#include <random>
#include <ranges>
#include <tuple>
#include <utility>

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
// Sample - a single MCMC draw with metadata
// =============================================================================

template <typename State, typename Prob = double>
struct Sample {
  State value;
  Prob log_prob;
  bool accepted;  // Was the proposal that led to this state accepted?
};

// =============================================================================
// Chain - an infinite input_range of MCMC samples
// =============================================================================
//
// Chain is a move-only input_range. It cannot be copied because it owns
// unique RNG state. Calling begin() twice is invalid - the chain is
// single-pass by nature.
//
// Usage with standard range adaptors:
//
//   auto samples = chain<double>(log_target, proposal, gen, x0)
//       | std::views::drop(1000)             // burn-in
//       | std::views::stride(10)             // thinning
//       | std::views::take(5000)             // collect 5000
//       | std::views::transform(&Sample<double>::value)  // extract values
//       | std::ranges::to<std::vector>();
//

template <typename State, typename Prob, typename LogTarget, typename Propose,
          typename Generator>
class Chain {
 public:
  class Iterator {
   public:
    using iterator_category = std::input_iterator_tag;
    using value_type = Sample<State, Prob>;
    using difference_type = std::ptrdiff_t;
    using pointer = const value_type*;
    using reference = value_type;

    Iterator() = default;
    explicit Iterator(Chain* chain) : chain_{chain} {
      if (chain_) {
        current_ = chain_->step();
      }
    }

    auto operator*() const -> reference { return current_; }

    auto operator++() -> Iterator& {
      if (chain_) {
        current_ = chain_->step();
      }
      return *this;
    }

    void operator++(int) { ++*this; }

    // Infinite range - never equal to sentinel
    auto operator==(std::default_sentinel_t) const -> bool { return false; }

   private:
    Chain* chain_ = nullptr;
    Sample<State, Prob> current_{};
  };

  Chain(LogTarget log_target, Propose propose, Generator gen, State initial)
      : log_target_{std::move(log_target)},
        propose_{std::move(propose)},
        gen_{std::move(gen)},
        state_{std::move(initial)},
        log_prob_{log_target_(state_)} {
    assert(std::isfinite(log_prob_) && "initial state must be in target support");
  }

  // Move-only: copying would duplicate RNG state unexpectedly
  Chain(const Chain&) = delete;
  auto operator=(const Chain&) -> Chain& = delete;
  Chain(Chain&&) = default;
  auto operator=(Chain&&) -> Chain& = default;

  auto begin() -> Iterator {
    assert(!begun_ && "Chain::begin() can only be called once");
    begun_ = true;
    return Iterator{this};
  }

  auto end() const -> std::default_sentinel_t { return {}; }

  // Access current chain state (for diagnostics, checkpointing)
  auto state() const -> const State& { return state_; }
  auto logProb() const -> Prob { return log_prob_; }

 private:
  auto step() -> Sample<State, Prob> {
    State proposed = propose_(state_, gen_);
    Prob proposed_lp = log_target_(proposed);

    // Reject if outside support
    if (!std::isfinite(proposed_lp)) {
      return {state_, log_prob_, false};
    }

    Prob log_alpha = proposed_lp - log_prob_;
    bool accept = false;

    if (log_alpha >= Prob{0}) {
      accept = true;
    } else {
      Prob u = Uniform<Prob>{Prob{0}, Prob{1}}.sample(gen_);
      accept = std::log(u) < log_alpha;
    }

    if (accept) {
      state_ = std::move(proposed);
      log_prob_ = proposed_lp;
    }

    return {state_, log_prob_, accept};
  }

  LogTarget log_target_;
  Propose propose_;
  Generator gen_;
  State state_;
  Prob log_prob_;
  bool begun_ = false;
};

// Factory function with explicit state type
template <typename State, typename Prob = double, typename LogTarget,
          typename Propose, typename Generator>
  requires LogProbFn<LogTarget, State, Prob> && Proposal<Propose, State, Generator>
auto chain(LogTarget log_target, Propose propose, Generator gen, State initial) {
  return Chain<State, Prob, LogTarget, Propose, Generator>{
      std::move(log_target), std::move(propose), std::move(gen),
      std::move(initial)};
}

// =============================================================================
// acceptanceRate - compute acceptance rate from a range of Samples
// =============================================================================

template <std::ranges::input_range R>
  requires requires(std::ranges::range_value_t<R> s) {
    { s.accepted } -> std::convertible_to<bool>;
  }
auto acceptanceRate(R&& samples) -> double {
  std::size_t total = 0;
  std::size_t accepted = 0;
  for (const auto& s : samples) {
    ++total;
    if (s.accepted) ++accepted;
  }
  if (total == 0) return 0.0;
  return static_cast<double>(accepted) / static_cast<double>(total);
}

// =============================================================================
// Scalar Proposal Distributions
// =============================================================================

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

template <typename T = double>
class GaussianWalk {
 public:
  constexpr explicit GaussianWalk(T sigma) : dist_{T{0}, sigma} {
    assert(sigma > T{0} && "sigma must be positive");
  }

  template <std::uniform_random_bit_generator Generator>
  auto operator()(T current, Generator& g) -> T {
    return current + dist_.sample(g);
  }

 private:
  Normal<T> dist_;
};

// =============================================================================
// N-Dimensional Proposal Distributions (for std::array)
// =============================================================================

template <typename T, std::size_t N>
class GaussianWalkND {
 public:
  constexpr explicit GaussianWalkND(T sigma) : dist_{T{0}, sigma} {
    assert(sigma > T{0} && "sigma must be positive");
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

template <typename Proposal>
HomogeneousWalk(Proposal) -> HomogeneousWalk<Proposal>;

}  // namespace tempura::bayes
