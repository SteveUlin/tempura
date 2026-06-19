#include "exp.h"

#include <cmath>
#include <limits>
#include <numbers>
#include <span>
#include <vector>

#include "ulp_sweep.h"
#include "unit.h"

using namespace tempura;

// constexpr: exp(0) is exactly 1; exp(1) ≈ e.
static_assert(tempura::exp(0.0) == 1.0);
static_assert(isClose(tempura::exp(1.0), std::numbers::e_v<double>, {.rtol = 1e-14, .atol = 0}));

auto main() -> int {
  const double inf = std::numeric_limits<double>::infinity();
  const double nan = std::numeric_limits<double>::quiet_NaN();

  "edge cases and special values"_test = [&] {
    expectEq(tempura::exp(0.0), 1.0);
    expectEq(tempura::exp(710.0), inf);    // overflow
    expectEq(tempura::exp(-746.0), 0.0);   // underflow → +0
    expectEq(tempura::exp(inf), inf);
    expectEq(tempura::exp(-inf), 0.0);
    expectTrue(std::isnan(tempura::exp(nan)));
    expectWithinUlps(tempura::exp(1.0), std::exp(1.0), 2);
    expectWithinUlps(tempura::exp(-1.0), std::exp(-1.0), 2);
    expectWithinUlps(tempura::exp(709.0), std::exp(709.0), 2);   // near overflow, finite
    expectWithinUlps(tempura::exp(-720.0), std::exp(-720.0), 2);  // subnormal result (scalbn path)
  };

  "≤2 ULP vs std::exp across the whole domain"_test = [] {
    std::vector<double> xs;
    for (double x = -700.0; x <= 700.0; x += 0.37) xs.push_back(x);  // full range
    for (double x = -2.0; x <= 2.0; x += 0.0131) xs.push_back(x);    // dense near 0
    for (double x = -745.0; x <= -700.0; x += 0.53) xs.push_back(x);  // subnormal tail
    expectMaxUlps([](double x) { return tempura::exp(x); },
                  [](double x) { return std::exp(x); },
                  std::span<const double>{xs}, 2);
  };

  return TestRegistry::result();
}
