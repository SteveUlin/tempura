#include "sqrt_approx.h"

#include <cmath>
#include <limits>
#include <span>
#include <vector>

#include "ulp_sweep.h"
#include "unit.h"

using namespace tempura;

// constexpr: both run at compile time (bit_cast + the iterations).
static_assert(isClose(sqrtNewton(4.0), 2.0, {.rtol = 1e-12, .atol = 1e-12}));
static_assert(isClose(fastInvSqrt(4.0), 0.5, {.rtol = 1e-9, .atol = 1e-9}));

auto main() -> int {
  std::vector<double> xs;
  for (double x = 0.01; x < 100.0; x += 0.013) xs.push_back(x);   // dense small
  for (double x = 1.0; x < 1e12; x *= 1.7) xs.push_back(x);        // wide dynamic range
  for (double x : {1.0, 2.0, 4.0, 0.25, 1e-6, 1e8}) xs.push_back(x);
  const std::span<const double> sweep{xs};

  "sqrtNewton matches std::sqrt to a few ULP"_test = [&] {
    expectMaxUlps([](double x) { return sqrtNewton(x); },
                  [](double x) { return std::sqrt(x); }, sweep, 2);
  };

  "fastInvSqrt matches 1/std::sqrt to a few ULP"_test = [&] {
    expectMaxUlps([](double x) { return fastInvSqrt(x); },
                  [](double x) { return 1.0 / std::sqrt(x); }, sweep, 4);
  };

  "edge cases"_test = [] {
    expectEq(sqrtNewton(0.0), 0.0);
    expectTrue(std::isnan(sqrtNewton(-1.0)));
    expectEq(sqrtNewton(std::numeric_limits<double>::infinity()),
             std::numeric_limits<double>::infinity());
  };

  return TestRegistry::result();
}
