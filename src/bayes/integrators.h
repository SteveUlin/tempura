#pragma once

#include <concepts>
#include <cstdint>
#include <deque>
#include <print>
#include <ranges>
#include <type_traits>
#include <utility>

#include "broadcast_array.h"

namespace tempura::bayes {

namespace detail {

template <typename T>
concept AnyOptional = std::same_as<T, std::optional<typename T::value_type>>;

}  // namespace detail

// Precision scales roughly as 1/√n where n is the number of samples.
template <std::invocable<> Sampler,
          std::invocable<std::invoke_result_t<Sampler>> Func>
class MonteCarloIntegrator {
 public:
  using DomainT = std::invoke_result_t<Sampler>;
  using ResultT = std::invoke_result_t<Func, DomainT>;

  constexpr MonteCarloIntegrator(Func&& func, Sampler&& sampler,
                                 double scale = 1.0)
      : func_(std::move(func)), sampler_(std::move(sampler)), scale_(scale) {}

  constexpr void step(int64_t n) {
    while (0 < n--) {
      const DomainT x = sampler_();
      tape_.emplace_back(x, func_(x));

      const double w = static_cast<double>(samples() - 1) / samples();
      result_ = w * result_ + (1.0 - w) * tape_.back().output;
      square_result_ = w * square_result_ +
                       (1.0 - w) * tape_.back().output * tape_.back().output;
    }
  }

  constexpr auto result() const -> ResultT { return scale_ * result_; }

  constexpr auto variance() const -> ResultT {
    using std::sqrt;
    return scale_ * sqrt(square_result_ - result_ * result_) / samples();
  }

  constexpr auto samples() const -> int64_t { return tape_.size(); }

  constexpr auto tape() const -> const auto& { return tape_; }

 private:
  struct LogEntry {
    DomainT input;
    ResultT output;
  };

  Func func_;
  Sampler sampler_;
  double scale_;

  ResultT result_ = {};
  ResultT square_result_ = {};
  std::deque<LogEntry> tape_;
};

template <typename Func, typename Sampler>
MonteCarloIntegrator(Func&& func, Sampler&& sampler)
    -> MonteCarloIntegrator<Func, Sampler>;

}  // namespace tempura::bayes
