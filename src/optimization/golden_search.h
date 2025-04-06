#pragma once

#include <cassert>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <print>
#include <type_traits>
#include <utility>

#include "optimization/util.h"

namespace tempura::optimization {

// Search for the minimum of a function by extending bisection to three points.
// Each step selects the next evaluation point such that it maximizes the worst
// case bracket width reduction.
//
// A bracket is defined to be 3 points on a line, with the middle point being
// lower than the other two.
//
// 
// a      b               c
//
// Where should we pick the next evaluation point `x`?
// 
// a      b        x      c
//
// Here we want to maximize the reduction of the interval's width. After
// evaluating the functions at `x`, we will be able to reduce the interval
// to either:
//   - a, b, x
//   - b, x, c
//
// To minimize the worst case, we want to pick `x` such that the distance
// [a, b] is equal to the distance [x, c].
//
// Now suppose that we could pick where b is. What position of b would
// lead to the "best" reduction of the interval's width?
//
// Here I defined best to be a that is also in the "best" location as a
// point that would also be in the "best" location on the next iteration. That
// way I can reuse the function eval.
//
// Whenever you have self referential distance reductions, you'll probably
// find ϕ. Here is no different, the ideal is to have the point b at
// (2. - ϕ) * [a, c] and x at (1. - ϕ) * [a, c].
//
// But unfortunately, we can't guarantee that b will be given to us at this
// golden ratio point. But we can pick an initial x such that b lines up
// on the next iteration.
template <typename T, std::invocable<T> Func>
constexpr auto goldenSectionSearch(
    const Bracket<Record<T, std::invoke_result_t<Func, T>>>& bracket,
    Func&& func, Tolerance<std::invoke_result_t<Func, T>> tol = {},
    std::size_t maxItr = 10'000)
    -> Bracket<Record<T, std::invoke_result_t<Func, T>>> {
  constexpr T R = std::numbers::phi - 1.;
  constexpr T C = 1. - R;

  using RecordT = Record<T, std::invoke_result_t<Func, T>>;
  RecordT r0 = bracket.a;
  RecordT r3 = bracket.c;

  RecordT r1;
  RecordT r2;

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

  size_t calls = 1;
  while (calls < maxItr) {
    std::println("{:.6} {:.6} {:.6} {:.6}", x0, x1, x2, x3);
    if (f1 < f2) {
      if (abs(x2 - x0) <
          tol.value * (abs(x1) + std::numeric_limits<double>::epsilon())) {
        return {r0, r1, r2};
      }
      r3 = r2;
      r2 = r1;
      r1 = mkRecord(R * x1 + C * x0, func);
    } else {
      if (abs(x3 - x1) <
          tol.value * (abs(x1) + std::numeric_limits<double>::epsilon())) {
        return {r1, r2, r3};
      }
      r0 = r1;
      r1 = r2;
      r2 = mkRecord(R * x2 + C * x3, func);
    }
    ++calls;
  }

  assert((void("Maximum iterations reached"), false));

  std::unreachable();
}

}  // namespace tempura::optimization
