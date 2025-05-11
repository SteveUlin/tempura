#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <functional>
#include <iostream>
#include <tuple>
#include <type_traits>
#include <utility>

#include "optimization/bracket_method.h"
#include "optimization/brents_method.h"
#include "optimization/util.h"

namespace tempura::optimization {

// Average Dissent
//
// Average Dissent finds a local minimum of an n-dimensional function.
//
// Given some starting location, we try to use a 1-dimensional optimization
// method (brent's method) along each dimension. But there is no guarantee
// that any of these directions will be in the direction of steepest descent.
// The result is a bunch of small movements without much movement to the
// minimum.
//
// So we instead take the average of all of the update vectors and hope this is
// pointing closer to the direction of steepest descent. We keep an array
// of update directions, we take this average vector and replace the vector
// in our update set that is closest to already pointing in this direction.

template <typename Point, typename Func>
constexpr auto averageDissent(Point&& point, Func&& func,
                              Tolerance<double> tol = {}) {
  using VectorT = std::remove_cvref_t<Point>;
  constexpr std::size_t N = std::tuple_size_v<VectorT>;

  VectorT curr = std::forward<Point>(point);

  auto directionalEval = [&curr, &func, &N](const auto& direction) {
    return [&, direction](const auto& λ) {
      return [&]<std::size_t... Is>(std::index_sequence<Is...>) {
        return func(std::get<Is>(curr) + λ * std::get<Is>(direction)...);
      }(std::make_index_sequence<N>{});
    };
  };

  std::array<VectorT, N> directions = {};
  [&directions]<std::size_t... Is>(std::index_sequence<Is...>) {
    ((std::get<Is>(directions[Is]) += 1.0), ...);
  }(std::make_index_sequence<N>{});

  while (true) {
    std::array<double, N> steps = {};
    for (std::size_t i = 0; i < N; ++i) {
      auto projected_func = directionalEval(directions[i]);
      // Find a minimum along this direction
      auto bracket = bracketMethod(-1., 1., projected_func);
      // Optimize the minimum
      bracket = brentsMethod(bracket, projected_func, tol);
      steps[i] = bracket.b.input;
      [&]<std::size_t... Is>(std::index_sequence<Is...>) {
        ((std::get<Is>(curr) += std::get<Is>(directions[i]) * steps[i]), ...);
      }(std::make_index_sequence<N>{});
    }

    VectorT avg_step = {};
    std::size_t index_of_largest_step = 0;
    for (std::size_t i = 0; i < N; ++i) {
      [&]<std::size_t... Is>(std::index_sequence<Is...>) {
        ((std::get<Is>(avg_step) += std::get<Is>(directions[i]) * steps[i]),
         ...);
        if (std::abs(steps[i]) > std::abs(steps[index_of_largest_step])) {
          index_of_largest_step = i;
        }
      }(std::make_index_sequence<N>{});
    }

    double magnitude = 0.0;
    [&]<std::size_t... Is>(std::index_sequence<Is...>) {
      ((magnitude += std::get<Is>(avg_step) * std::get<Is>(avg_step)), ...);
    }(std::make_index_sequence<N>{});
    magnitude = std::sqrt(magnitude);
    for (std::size_t i = 0; i < N; ++i) {
      [&]<std::size_t... Is>(std::index_sequence<Is...>) {
        ((std::get<Is>(directions[index_of_largest_step]) =
              std::get<Is>(avg_step) / magnitude),
         ...);
      }(std::make_index_sequence<N>{});
    }
    if (magnitude < tol.value) {
      break;
    }
  }
  return curr;
}

}  // namespace tempura::optimization
