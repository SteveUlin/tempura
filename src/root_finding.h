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

auto subIntervalSignChange(std::function<double(double)>& func,
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

auto bisectRoot(std::function<double(double)>& func, Interval interval,
                int64_t max_iter = 1'000) -> Interval {
  auto& [a, b] = interval;
  double delta = std::numeric_limits<double>::epsilon() * std::abs(b - a);
  assert(std::signbit(func(a)) != std::signbit(func(b)));

  for (int64_t i = 0; i < max_iter; i++) {
    double mid = (a + b) / 2;
    if (func(mid) == 0) {
      a = mid;
      b = mid;
      return interval;
    }
    if (std::signbit(func(mid)) == std::signbit(func(a))) {
      a = mid;
    } else {
      b = mid;
    }

    if (std::abs(b - a) < delta) {
      break;
    }
  }
  return interval;
}

}  // namespace tempura::root_finding
