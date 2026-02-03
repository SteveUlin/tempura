#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <iterator>
#include <numeric>
#include <vector>

#include "symbolic4/distributions/random_var.h"
#include "symbolic4/mcmc/state.h"

// ============================================================================
// samples.h - Samples container with symbol-based access
// ============================================================================
//
// Stores MCMC draws and provides type-safe access by parameter symbol.
//
// Usage:
//   Samples<decltype(alpha), decltype(beta)> samples;
//   samples.push_back({1.0, 2.0});
//   samples.push_back({1.1, 2.1});
//
//   // Access all draws for one parameter (for statistics)
//   auto alpha_draws = samples[alpha];  // vector<double>
//   double m = mean(alpha_draws);
//
//   // Access single draw
//   auto draw = samples[0];  // ParameterState
//   double a = draw[alpha];
//
//   // Iteration
//   for (const auto& draw : samples) {
//     std::cout << draw[alpha] << "\n";
//   }
//
// ============================================================================

namespace tempura::symbolic4 {

// ============================================================================
// Samples - Container for MCMC draws with symbol-based access
// ============================================================================

template <typename... Params>
class Samples {
 public:
  static constexpr std::size_t N = sizeof...(Params);
  using StateType = ParameterState<Params...>;
  using ArrayType = std::array<double, N>;

  // Add a draw
  void push_back(const ArrayType& draw) { draws_.push_back(draw); }

  void reserve(std::size_t n) { draws_.reserve(n); }

  auto size() const -> std::size_t { return draws_.size(); }

  auto empty() const -> bool { return draws_.empty(); }

  // Access all values for one parameter (for statistics)
  template <typename P>
    requires IsRandomVar<P>
  auto operator[](const P&) const -> std::vector<double> {
    constexpr std::size_t idx = StateType::template indexOfParam<P>();
    static_assert(idx < N, "Parameter not found in Samples");

    std::vector<double> result;
    result.reserve(draws_.size());
    for (const auto& d : draws_) {
      result.push_back(d[idx]);
    }
    return result;
  }

  // Access single draw as ParameterState
  auto operator[](std::size_t i) const -> StateType {
    return StateType{draws_[i]};
  }

  // Raw access to underlying storage
  auto draws() const -> const std::vector<ArrayType>& { return draws_; }

  // -------------------------------------------------------------------------
  // Iterator support - iterates over draws as ParameterState
  // -------------------------------------------------------------------------

  class Iterator {
   public:
    using iterator_category = std::forward_iterator_tag;
    using difference_type = std::ptrdiff_t;
    using value_type = StateType;
    using pointer = void;  // No pointer type for proxy iterator
    using reference = StateType;  // Returns by value (proxy)

    Iterator() = default;
    explicit Iterator(typename std::vector<ArrayType>::const_iterator it) : it_{it} {}

    auto operator*() const -> StateType { return StateType{*it_}; }

    auto operator++() -> Iterator& {
      ++it_;
      return *this;
    }

    auto operator++(int) -> Iterator {
      Iterator tmp = *this;
      ++it_;
      return tmp;
    }

    auto operator==(const Iterator& other) const -> bool { return it_ == other.it_; }
    auto operator!=(const Iterator& other) const -> bool { return it_ != other.it_; }

   private:
    typename std::vector<ArrayType>::const_iterator it_;
  };

  auto begin() const -> Iterator { return Iterator{draws_.begin()}; }
  auto end() const -> Iterator { return Iterator{draws_.end()}; }

 private:
  std::vector<ArrayType> draws_;
};

// ============================================================================
// Statistics helpers - operate on vector<double> from samples[param]
// ============================================================================

inline auto mean(const std::vector<double>& values) -> double {
  if (values.empty()) return 0.0;
  return std::accumulate(values.begin(), values.end(), 0.0) /
         static_cast<double>(values.size());
}

inline auto sd(const std::vector<double>& values) -> double {
  if (values.size() < 2) return 0.0;
  double m = mean(values);
  double sum_sq = 0.0;
  for (double v : values) {
    sum_sq += (v - m) * (v - m);
  }
  return std::sqrt(sum_sq / static_cast<double>(values.size()));
}

inline auto variance(const std::vector<double>& values) -> double {
  if (values.size() < 2) return 0.0;
  double m = mean(values);
  double sum_sq = 0.0;
  for (double v : values) {
    sum_sq += (v - m) * (v - m);
  }
  return sum_sq / static_cast<double>(values.size());
}

// Quantile (0.0 to 1.0)
inline auto quantile(std::vector<double> values, double p) -> double {
  if (values.empty()) return 0.0;
  std::sort(values.begin(), values.end());
  double idx = p * static_cast<double>(values.size() - 1);
  std::size_t lo = static_cast<std::size_t>(std::floor(idx));
  std::size_t hi = static_cast<std::size_t>(std::ceil(idx));
  if (lo == hi) return values[lo];
  double frac = idx - static_cast<double>(lo);
  return values[lo] * (1.0 - frac) + values[hi] * frac;
}

inline auto median(const std::vector<double>& values) -> double {
  return quantile(values, 0.5);
}

}  // namespace tempura::symbolic4
