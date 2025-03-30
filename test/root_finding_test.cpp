#include "root_finding.h"

#include <print>

#include "unit.h"

using namespace tempura;
using namespace tempura::root_finding;

auto main() -> int {
  "Bisect Root"_test = [] {
    std::function<double(double)> func = [](double x) -> double {
      return x * x - 2.;
    };
    Interval interval{.a = 0, .b = 100};
    auto roots = subIntervalSignChange(func, interval, 10);
    expectEq(roots.size(), 1L);
    int64_t N;
    auto root = bisectRoot(func, roots[0], 1'000, &N);
    expectLessThan(std::abs(root.a - std::numbers::sqrt2), 1e-12);
    expectLessThan(std::abs(root.a - std::numbers::sqrt2), 1e-12);
    std::println("a: {}, b: {}", root.a, root.b);
    std::println("N: {}", N);
  };

  "secantMethod"_test = [] {
    std::function<double(double)> func = [](double x) -> double {
      return x * x - 2.;
    };
    Interval interval{.a = 0, .b = 100};
    auto roots = subIntervalSignChange(func, interval, 10);
    expectEq(roots.size(), 1L);
    int64_t N;
    auto root = secantMethod(func, roots[0], 1'000, &N);
    expectLessThan(std::abs(root.a - std::numbers::sqrt2), 1e-12);
    expectLessThan(std::abs(root.a - std::numbers::sqrt2), 1e-12);
    std::println("a: {}, b: {}", root.a, root.b);
    std::println("N: {}", N);
  };

  "falsePosition"_test = [] {
    std::function<double(double)> func = [](double x) -> double {
      return x * x - 2.;
    };
    Interval interval{.a = 0, .b = 100};
    auto roots = subIntervalSignChange(func, interval, 10);
    expectEq(roots.size(), 1L);
    int64_t N;
    auto root = falsePosition(func, roots[0], 1'000, &N);
    expectLessThan(std::abs(root.a - std::numbers::sqrt2), 1e-12);
    expectLessThan(std::abs(root.a - std::numbers::sqrt2), 1e-12);
    std::println("a: {}, b: {}", root.a, root.b);
    std::println("N: {}", N);
  };

  "riddersMethod"_test = [] {
    std::function<double(double)> func = [](double x) -> double {
      return x * x - 2.;
    };
    Interval interval{.a = 0, .b = 100};
    auto roots = subIntervalSignChange(func, interval, 10);
    expectEq(roots.size(), 1L);
    int64_t N;
    auto root = riddersMethod(func, roots[0], 1'000, &N);
    expectLessThan(std::abs(root.a - std::numbers::sqrt2), 1e-12);
    expectLessThan(std::abs(root.a - std::numbers::sqrt2), 1e-12);
    std::println("a: {}, b: {}", root.a, root.b);
    std::println("N: {}", N);
  };

  return 0;
}
