#pragma once

#include <cassert>
#include <cmath>
#include <concepts>
#include <cstdint>
#include <functional>
#include <type_traits>

namespace tempura::root_finding {

struct Interval {
  double a;
  double b;
};

constexpr auto subIntervalSignChange(std::function<double(double)>& func,
                                     Interval interval, int64_t N)
    -> std::vector<Interval> {
  std::vector<Interval> intervals;
  auto& [a, b] = interval;
  double delta = (b - a) / N;
  double x = a;
  for (int64_t i = 0; i < N; i++) {
    double x_next = x + delta;
    if (std::signbit(func(x)) != std::signbit(func(x_next))) {
      intervals.push_back({x, x_next});
    }
    x = x_next;
  }
  return intervals;
}

// Below are standard root-finding methods, see Numberic Recipies thrid ed.
// ch 9.

// All of the methods below assume that the sign of the function at the
// endpoints of the interval is different and a < b. They reduce the size
// of the interval around the point where the function crosses the x-axis.

// Bisection Method
//
// This is just binary search for the root of a function.
constexpr auto bisectRoot(std::function<double(double)>& func,
                          Interval interval, int64_t max_iter = 1'000,
                          int64_t* N = nullptr) -> Interval {
  auto& [a, b] = interval;
  double delta = std::numeric_limits<double>::epsilon() * std::abs(b - a);
  double f_a = func(a);
  double f_b = func(b);
  assert(std::signbit(f_a) != std::signbit(f_b));

  int64_t i = 0;
  for (; i < max_iter; i++) {
    double mid = (a + b) / 2;
    double f_mid = func(mid);
    if (f_mid == 0) {
      a = mid;
      b = mid;
      if (N != nullptr) *N = i;
      return interval;
    }
    if (std::signbit(f_mid) == std::signbit(f_a)) {
      a = mid;
    } else {
      b = mid;
    }

    if (std::abs(b - a) < delta) {
      break;
    }
  }
  if (N != nullptr) *N = i;
  return interval;
}

// Secant Method
//
// Keep track of the last two seen points, use those to draw a line. The
// intersection of that line with the x-axis is our next point.
//
// The scant method is not guaranteed to remain bounded within the original
constexpr auto secantMethod(std::function<double(double)>& func,
                            Interval interval, int64_t max_iter = 1'000,
                            int64_t* N = nullptr) -> Interval {
  auto& [a, b] = interval;
  double delta = std::numeric_limits<double>::epsilon() * std::abs(b - a);
  double prev_x = a;
  double prev_f = func(a);
  double curr_x = b;
  double curr_f = func(b);
  assert(std::signbit(prev_f) != std::signbit(curr_f));
  if (std::abs(prev_f) < std::abs(curr_f)) {
    std::swap(prev_x, curr_x);
    std::swap(prev_f, curr_f);
  }

  int64_t i = 0;
  for (; i < max_iter; i++) {
    double next_x = curr_x - curr_f * (curr_x - prev_x) / (curr_f - prev_f);
    double next_f = func(next_x);
    if (next_f == 0) {
      a = next_x;
      b = next_x;
      if (N != nullptr) *N = i;
      return interval;
    }
    std::tie(prev_x, prev_f) = std::tie(curr_x, curr_f);
    std::tie(curr_x, curr_f) = std::tie(next_x, next_f);
    if (std::abs(curr_x - prev_x) < delta) {
      break;
    }
  }

  a = prev_x;
  b = curr_x;
  if (b < a) std::swap(a, b);

  if (N != nullptr) *N = i;
  return interval;
}

// False Position Method
//
// Like the secant method but instead of using the most recently seen 2 points,
// use the new point to subdivide the interval. Generally this more stable
// of a method as you cannot go outside the bounds of the interval.
//
// Can be slow if there is a large curvature in the function.
constexpr auto falsePosition(std::function<double(double)>& func,
                             Interval interval, int64_t max_iter = 1'000,
                             int64_t* N = nullptr) -> Interval {
  auto& [a, b] = interval;
  assert(a < b);
  double delta = std::numeric_limits<double>::epsilon() * std::abs(b - a);
  double f_a = func(a);
  double f_b = func(b);
  assert(std::signbit(f_a) != std::signbit(f_b));
  int64_t i = 0;
  for (; i < max_iter; i++) {
    double c = b - f_b * (b - a) / (f_b - f_a);
    double f_c = func(c);
    if (f_c == 0) {
      a = c;
      b = c;
      if (N != nullptr) *N = i;
      return interval;
    }
    if ((f_b > 0.0 && f_c > 0.0) || (f_b < 0.0 && f_c < 0.0)) {
      double del = std::abs(b - c);
      b = c;
      f_b = f_c;
      if (del < delta) {
        break;
      }
    } else {
      double del = std::abs(c - a);
      a = c;
      f_a = f_c;
      if (del < delta) {
        break;
      }
    }
  }

  if (N != nullptr) *N = i;
  return interval;
}

// Ridder's method
//
// Ridders method, (like bisection), evaluates the function at the midpoint
// of the interval. It then defined a new function:
//   h(x) = eᵃˣf(x)
// picking `a` such that
//   h(mid) = (h(l) + h(r))/2
// This "factors out" curvature from the function, and we use the line going
// through h(l), h(mid), and h(r)
//
// Numerical Recipes Third Edition 9.3.1
constexpr auto riddersMethod(std::function<double(double)>& func,
                             Interval interval, int64_t max_iter = 1'000,
                             int64_t* N = nullptr) -> Interval {
  int64_t i = 0;
  auto& [a, b] = interval;
  assert(a < b);
  double delta = std::numeric_limits<double>::epsilon() * std::abs(b - a);
  double f_a = func(a);
  double f_b = func(b);
  if (f_a == 0.0) {
    b = a;
    if (N != nullptr) *N = i;
    return interval;
  }
  if (f_b == 0.0) {
    a = b;
    if (N != nullptr) *N = i;
    return interval;
  }
  for (; i < max_iter; i++) {
    double m = (a + b) * 0.5;
    double f_m = func(m);
    if (f_m == 0.0) {
      a = m;
      b = m;
      if (N != nullptr) *N = i;
      return interval;
    }
    // s is guaranteed to be positive since f_a and f_b have different signs
    double s = std::sqrt(f_m * f_m - f_a * f_b);
    double x = m + (m - a) * ((f_a < f_b ? -f_m : f_m) / s);
    double f_x = func(x);
    if (f_x == 0.0) {
      a = x;
      b = x;
      if (N != nullptr) *N = i;
      return interval;
    }
    if ((f_m < 0.0 && 0.0 < f_x)) {
      std::tie(a, b) = std::tie(m, x);
      std::tie(f_a, f_b) = std::tie(f_m, f_x);
    } else if (f_x < 0.0 && 0.0 < f_m) {
      std::tie(a, b) = std::tie(x, m);
      std::tie(f_a, f_b) = std::tie(f_x, f_m);
    } else if (x < m) {
      b = x;
      f_b = f_x;
    } else /*m < x*/ {
      a = x;
      f_a = f_x;
    }
    if (std::abs(b - a) < delta) {
      break;
    }
  }
  if (N != nullptr) *N = i;
  return interval;
}

}  // namespace tempura::root_finding
