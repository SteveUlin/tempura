#include "root_finding.h"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numbers>

#include "unit.h"

using namespace tempura;

// The payoff of templating on the callable: every solver runs at compile time.
// f(x) = x² − 2 has its positive root at √2; each method must land on it.
static_assert([] {
  auto f = [](double x) { return x * x - 2.0; };
  const Interval br{.a = 1.0, .b = 2.0};
  const Tolerance tol{.atol = 1e-12};
  return isClose(bisectRoot(f, br).a, std::numbers::sqrt2, tol) &&
         isClose(secantMethod(f, br).a, std::numbers::sqrt2, tol) &&
         isClose(falsePosition(f, br).a, std::numbers::sqrt2, tol) &&
         isClose(riddersMethod(f, br).a, std::numbers::sqrt2, tol) &&
         subIntervalSignChange(f, Interval{.a = 0.0, .b = 100.0}, 10).size() == 1;
}());

auto main() -> int {
  "subIntervalSignChange brackets each sign flip"_test = [] {
    auto f = [](double x) { return x * x - 2.0; };  // root √2 ∈ [0,10]
    const auto brackets = subIntervalSignChange(f, Interval{.a = 0.0, .b = 100.0}, 10);
    expectEq(brackets.size(), std::size_t{1});
    expectLE(brackets[0].a, std::numbers::sqrt2);
    expectLE(std::numbers::sqrt2, brackets[0].b);
  };

  "each method converges to √2 from a wide bracket"_test = [] {
    auto f = [](double x) { return x * x - 2.0; };
    const Interval br{.a = 0.0, .b = 10.0};
    const Tolerance tol{.atol = 1e-10};
    expectClose(bisectRoot(f, br).a, std::numbers::sqrt2, tol);
    expectClose(secantMethod(f, br).a, std::numbers::sqrt2, tol);
    expectClose(falsePosition(f, br).a, std::numbers::sqrt2, tol);
    expectClose(riddersMethod(f, br).a, std::numbers::sqrt2, tol);
  };

  "methods work on a transcendental root (cos at π/2)"_test = [] {
    auto f = [](double x) { return std::cos(x); };  // root π/2 ∈ [1,2]
    const Interval br{.a = 1.0, .b = 2.0};
    expectClose(riddersMethod(f, br).a, std::numbers::pi / 2, {.atol = 1e-10});
    expectClose(bisectRoot(f, br).a, std::numbers::pi / 2, {.atol = 1e-10});
  };

  "iteration count is reported through the out-param"_test = [] {
    auto f = [](double x) { return x * x - 2.0; };
    std::int64_t iters = -1;
    bisectRoot(f, Interval{.a = 0.0, .b = 2.0}, 1'000, &iters);
    expectGT(iters, std::int64_t{0});   // it did real work
    expectLT(iters, std::int64_t{100});  // and converged well within the cap
  };

  return TestRegistry::result();
}
