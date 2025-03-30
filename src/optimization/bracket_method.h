#pragma once

#include <cassert>
#include <cmath>
#include <cstdlib>
#include <print>

#include "optimization/util.h"

namespace tempura::optimization {

// Given a function and initial guess where a minimum could be, search downhill
// until we find some bracket [left, right] and middle such that
// f(middle) < f(low) and f(middle) < f(high).
template <typename T, std::invocable<T> Func>
constexpr auto bracketMethod(T ax, T bx, Func&& func)
    -> Bracket<Record<T, std::invoke_result_t<Func, T>>> {
  // Limit the max parabolic fit step to kGlimit * (b - c);
  constexpr double kGLimit = 100.;

  Bracket<Record<T, std::invoke_result_t<Func, T>>> result{};
  result.a = {ax, func(ax)};
  result.b = {bx, func(bx)};
  // While doing calculations, b is always in the downhill direction from a.
  if (result.a.input > result.b.input) {
    std::swap(result.a, result.b);
  }
  auto& [a, f_a] = result.a;
  auto& [b, f_b] = result.b;
  auto& [c, f_c] = result.c;

  // Sample a point a bit further out than b and see if we keep going downhill
  c = b + std::numbers::phi * (b - a);
  f_c = func(c);

  // While we are going downhill
  while (f_c < f_b) {
    // Fit a parabola to the last seen three points
    const double r = (b - a) * (f_b - f_c);
    const double q = (b - c) * (f_b - f_a);
    double s = q - r;
    // If s is too close to zero, avoid zero division by setting s to 1.e-20 but
    // with the same sign
    if (std::abs(s) < 1e-20) {
      s = std::copysign(1e-20, s);
    }
    double u = b - ((b - c) * q - (b - a) * r) / (2.0 * s);
    double f_u;
    const double u_lim = b + kGLimit * (c - b);

    // Decide how to update a, b, c with respect to the point (u, f_u).
    // All paths in if-else block should either:
    //   - break if a valid bracket has been found
    //   - set u and f_u to a value past c
    // We then update <a, b, c> to <b, c, u> and iterate
    if ((b < u) && (u < c)) {
      // The parabolic fit puts u inside [b, c]
      f_u = func(u);
      if (f_u < f_c) {
        // The current minimum u is between [b, c]
        result.a = std::move(result.b);
        b = std::move(u);
        f_b = std::move(f_u);
        break;
      }
      if (f_b < f_u) {
        // The current minimum b is between [a, u]
        c = std::move(u);
        f_c = std::move(f_u);
        break;
      }
      // No help from parabolic interpolation, take a golden ratio step to get u
      u = c + std::numbers::phi * (c - b);
      f_u = func(u);
    } else if ((c < u) && (u < u_lim)) {
      // The parabolic interpolation is between c and the allowed limit
      f_u = func(u);
      if (f_u > f_c) {
        // The current minimum c is between [b, u]
        result.a = std::move(result.b);
        result.b = std::move(result.c);
        c = std::move(u);
        f_c = std::move(f_u);
        break;
      }
      result.b = std::move(result.c);
      c = std::move(u);
      f_c = std::move(f_u);
      // Make sure we don't get stuck in parabolic interpolation, take
      // a golden ratio step to get the next u
      u = c + std::numbers::phi * (c - b);
      f_u = func(u);
    } else if ((u_lim < u) && (c < u_lim)) {
      // Limit the parabolic step so we don't move too far
      u = u_lim;
      f_u = func(u);
    } else {
      // u_lim is so small it can't reach past c, take a normal golden ratio
      // step.
      //
      // I think this is normally unreachable, unless the definition of ulim
      // changes.
      u = c + std::numbers::phi * (c - b);
      f_u = func(u);
    }
    result.a = std::move(result.b);
    result.b = std::move(result.c);
    c = std::move(u);
    f_c = std::move(f_u);
  }

  if (a > c) {
    std::swap(result.a, result.c);
  }

  return result;
}

}  // namespace tempura::optimization
