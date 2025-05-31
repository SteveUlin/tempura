#pragma once

#include <concepts>
#include <cstdint>
#include <deque>
#include <ranges>
#include <type_traits>
#include <utility>
#include <print>

#include "broadcast_array.h"

namespace tempura::quadature {

template <typename T, std::invocable<T> Func, std::invocable<> Sampler,
          typename U = std::invoke_result_t<Func, T>>
  requires(std::convertible_to<T, std::invoke_result_t<Sampler>>)
class MonteCarloIntegrator {
 public:
  constexpr MonteCarloIntegrator(Func&& func, Sampler&& sampler)
      : func_(std::move(func)), sampler_(std::move(sampler)) {}

  constexpr void step(int64_t n) {
    while (0 < n--) {
      const T x = sampler_();
      tape_.emplace_back(x, func_(x));
    }
  }

  auto result() {
    BroadcastArray<double, 3> out = {};
    uint64_t hits = 0;
    for (const auto& [_, res] : tape_) {
      if (res.has_value()) {
        ++hits;
        out += *res;
      }
    }
    std::println("hits: {}", hits);
    return out / hits;
  }

  constexpr auto tape() const -> const auto& { return tape_; }

 private:
  struct LogEntry {
    T input;
    U result;
  };

  Func func_;
  Sampler sampler_;

  std::deque<LogEntry> tape_;
};

template <typename Func, typename Sampler>
MonteCarloIntegrator(Func&& func, Sampler&& sampler)
    -> MonteCarloIntegrator<decltype(sampler()), Func, Sampler>;

  // namespace tempura::quadature
