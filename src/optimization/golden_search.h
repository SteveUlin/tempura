#pragma once

#include <cassert>
#include <cmath>
#include <cstdlib>
#include <functional>
#include <print>
#include <type_traits>

#include "optimization/util.h"

namespace tempura::optimization {

// Search for the minimum of a function by extending bisection to three points.
// Each step selects the next evaluation point such that it maximizes the worst
// case reduction of the bracket's width.
//
// This calculation involves the golden ratio, hence the name.
template <typename T, std::invocable<T> Func>
constexpr auto goldenSectionSearch(
    const Bracket<Record<T, std::invoke_result_t<Func, T>>>& bracket,
    Func&& func, Tolerance<std::invoke_result_t<Func, T>> tol = {})
    -> Bracket<Record<T, std::invoke_result_t<Func, T>>> {
  constexpr T R = std::numbers::phi - 1.;
  constexpr T C = 1. - R;

  using RecordT = Record<T, std::invoke_result_t<Func, T>>;
  RecordT r0 = bracket.a;
  RecordT r3 = bracket.c;

  RecordT r1;
  RecordT r2;

  // make x0 to x1 the smaller segment;
  using std::abs;
  if (abs(bracket.b.input - bracket.a.input) <
      abs(bracket.c.output - bracket.b.output)) {
    r1 = bracket.b;
    r2 = mkRecord(bracket.b.input + C * (bracket.c.input - bracket.b.input),
                  func);
  } else {
    r2 = bracket.b;
    r1 = mkRecord(bracket.b.input - C * (bracket.b.input - bracket.a.input),
                  func);
  }

  auto& [x0, f0] = r0;
  auto& [x1, f1] = r1;
  auto& [x2, f2] = r2;
  auto& [x3, f3] = r3;

  while (tol.value * (abs(x1) + abs(x2)) < abs(x3 - x0)) {
    if (f1 < f2) {
      r3 = r2;
      r2 = r1;
      r1 = mkRecord(R * x1 + C * x0, func);
    } else {
      r0 = r1;
      r1 = r2;
      r2 = mkRecord(R * x2 + C * x3, func);
    }
  }

  if (f1 < f2) {
    return {r0, r1, r2};
  }
  return {r1, r2, r3};
}

}  // namespace tempura::optimization
