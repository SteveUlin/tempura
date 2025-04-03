#pragma once

#include <algorithm>
#include <array>
#include <iostream>
#include <type_traits>
#include <utility>

#include "matrix2/matrix.h"
#include "matrix2/storage/dense.h"
#include "optimization/util.h"

namespace tempura::optimization {

// Takes an input vector of length N and produces an array of N+1 vectors.
// This output array consists of
//  - a copy of the input
//  - N vectors that are copies of the input vector but with a term x added
template <typename Scalar, typename VectorT>
constexpr auto makeSimplex(Scalar x, const VectorT& input) {
  // Generic getters for any tuple-like type
  using std::get;
  using std::tuple_size_v;
  constexpr std::size_t N = tuple_size_v<VectorT>;
  std::array<VectorT, N + 1> simplex;
  std::ranges::fill(simplex, input);
  [&]<std::size_t... Is>(std::index_sequence<Is...>) {
    ((get<Is>(simplex[Is + 1]) += x), ...);
  }(std::make_index_sequence<N>());
  return simplex;
}

// Scales a vector wrt the opposing face of the simplex.
// - 2.0 will double the length wrt the opposing face
// - -1.0 will reverse the vector wrt the opposing face
// Takes in the input:
// - A scaling factor α
// - The sum of all vectors in the simplex
// - The vector to scale
template <typename VectorT>
constexpr auto scaleAgainstFace(double α, const VectorT& sum,
                                const VectorT& vec) {
  constexpr std::size_t N = std::tuple_size_v<VectorT>;
  // wall = (sum - vec) / N
  // diff = vec - wall
  // ans = wall + α * diff
  //
  // ans = wall + α (vec - wall)
  // ans = α vec + (1 - α) wall
  // ans = α vec + (1 - α) (sum - vec) / N
  // ans = (α - (1 - α) / N) vec + ((1 - α) / N) sum
  double fac1 = (1. - α) / N;
  double fac2 = α - fac1;
  VectorT scaled = {};
  using std::get;
  [&]<std::size_t... Is>(std::index_sequence<Is...>) {
    ((get<Is>(scaled) = fac2 * get<Is>(vec) + fac1 * get<Is>(sum)), ...);
  }(std::make_index_sequence<N>());
  return scaled;
}

// Downhill Simplex
// Given a N-Simplex, a matrix of N+1 points in N dimensional
// space, find the minimum of a function defined on that space.
//
// Downhill Simplex takes the point with the highest function value
// and "pushes" it through the opposing face to get to a hopefully
// lower value.
auto downhillSimplex(auto& simplex, auto&& func) {
  std::size_t MAX_FUNC_CALLS = 1'000;
  std::size_t calls = 0;
  using std::get;

  using VectorT = std::remove_cvref_t<decltype(simplex[0])>;
  constexpr std::size_t N =
      std::tuple_size_v<std::remove_cvref_t<decltype(simplex)>> - 1;

  // Get the sum of all points in the simplex
  VectorT sum = {};
  for (const auto& vec : simplex) {
    [&]<std::size_t... Is>(std::index_sequence<Is...>) {
      ((get<Is>(sum) += get<Is>(vec)), ...);
    }(std::make_index_sequence<N>());
  }

  auto evaluate = [&](const auto& vec) {
    calls++;
    return [&]<std::size_t... Is>(std::index_sequence<Is...>) {
      return func(get<Is>(vec)...);
    }(std::make_index_sequence<N>());
  };

  auto values = [&]<std::size_t... Is>(std::index_sequence<Is...>) {
    return std::array{evaluate(simplex[Is])...};
  }(std::make_index_sequence<N + 1>());

  // swap the provided vector with the one in the simplex at index i
  auto replace = [&](size_t i, const auto& value, auto&& vec) {
    // subtract the old value from the sum
    [&]<std::size_t... Is>(std::index_sequence<Is...>) {
      ((get<Is>(sum) -= get<Is>(simplex[i])), ...);
    }(std::make_index_sequence<N>());
    // Add the new value to the sum
    [&]<std::size_t... Is>(std::index_sequence<Is...>) {
      ((get<Is>(sum) += get<Is>(vec)), ...);
    }(std::make_index_sequence<N>());
    // Replace the value in the simplex
    simplex[i] = vec;
    values[i] = value;
  };

  while (true) {
    std::size_t largest = 0;
    std::size_t penultimate = 0;
    std::size_t smallest = 0;
    for (std::size_t i = 1; i < N + 1; ++i) {
      if (values[i] > values[largest]) {
        penultimate = largest;
        largest = i;
      } else if (values[i] < values[smallest]) {
        smallest = i;
      }
    }
    const double curr_tol =
        2.0 * std::abs(values[largest] - values[smallest]) /
        (std::abs(values[largest]) + std::abs(values[smallest]) + 1e-16);

    if (curr_tol < 1e-16 || MAX_FUNC_CALLS < calls) {
      // reorder simplex such that the smallest is first
      std::ranges::sort(std::views::zip(values, simplex), std::greater{},
                        [](const auto& tup) { return std::get<1>(tup); });
      std::println("function calls: {}", calls);
      return values[smallest];
    }

    auto next = scaleAgainstFace(-1., sum, simplex[largest]);
    const auto next_value = evaluate(next);
    if (next_value < values[smallest]) {
      // Better than the best, try to go even further
      auto stretch = scaleAgainstFace(-2., sum, simplex[largest]);
      const auto stretch_value = evaluate(stretch);
      if (stretch_value < next_value) {
        replace(largest, stretch_value, std::move(stretch));
      } else {
        replace(largest, next_value, std::move(next));
      }
    } else if (next_value < values[penultimate]) {
      // Better than the second worst, accept the step
      replace(largest, next_value, std::move(next));
    } else {
      if (next_value < values[largest]) {
        // Better than the worst, but not better than the second worst
        replace(largest, next_value, std::move(next));
      }
      // Try contracting
      auto contract = scaleAgainstFace(0.5, sum, simplex[largest]);
      const auto contract_value = evaluate(contract);
      if (contract_value < values[penultimate]) {
        // We have a new worst point
        replace(largest, contract_value, std::move(contract));
      } else {
        if (contract_value < values[largest]) {
          // Better than the worst, but not better than the second worst
          replace(largest, contract_value, std::move(contract));
        }
        // Shrink the simplex towards the best point
        for (size_t i = 0; i < N + 1; ++i) {
          if (i == smallest) continue;
          [&]<std::size_t... Is>(std::index_sequence<Is...>) {
            ((get<Is>(simplex[i]) =
                  get<Is>(simplex[smallest]) +
                  (get<Is>(simplex[i]) - get<Is>(simplex[smallest])) / 2),
             ...);
          }(std::make_index_sequence<N>());
          values[i] = evaluate(simplex[i]);
        }
        sum = {};
        for (const auto& vec : simplex) {
          [&]<std::size_t... Is>(std::index_sequence<Is...>) {
            ((get<Is>(sum) += get<Is>(vec)), ...);
          }(std::make_index_sequence<N>());
        }
      }
    }
  }
}

}  // namespace tempura::optimization
