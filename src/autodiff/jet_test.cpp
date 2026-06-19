#include "jet.h"

#include <cmath>
#include <cstddef>

#include "unit.h"

using namespace tempura;
using namespace tempura::autodiff;

// constexpr: f(x)=x³ at x=2 → derivatives [8, 12, 12, 6] (exact polynomial).
constexpr auto cubicDerivs() -> bool {
  auto x = jetVariable<double, 3>(2.0);
  auto y = x * x * x;
  return nthDerivative(y, 0) == 8.0 && nthDerivative(y, 1) == 12.0 &&
         nthDerivative(y, 2) == 12.0 && nthDerivative(y, 3) == 6.0;
}
static_assert(cubicDerivs());

auto main() -> int {
  "exp: every derivative equals exp(x)"_test = [] {
    auto x = jetVariable<double, 5>(0.5);
    auto y = exp(x);
    for (std::size_t k = 0; k <= 5; ++k)
      expectClose((nthDerivative(y, k)), std::exp(0.5), {.rtol = 1e-11, .atol = 1e-11});
  };

  "sin: cyclic derivatives sin, cos, −sin, −cos"_test = [] {
    auto x = jetVariable<double, 4>(0.7);
    auto y = sin(x);
    expectClose((nthDerivative(y, 0)), std::sin(0.7), {.rtol = 1e-11, .atol = 1e-11});
    expectClose((nthDerivative(y, 1)), std::cos(0.7), {.rtol = 1e-11, .atol = 1e-11});
    expectClose((nthDerivative(y, 2)), -std::sin(0.7), {.rtol = 1e-11, .atol = 1e-11});
    expectClose((nthDerivative(y, 3)), -std::cos(0.7), {.rtol = 1e-11, .atol = 1e-11});
    expectClose((nthDerivative(y, 4)), std::sin(0.7), {.rtol = 1e-11, .atol = 1e-11});
  };

  "log: f⁽ᵏ⁾(x) = (−1)^(k−1)(k−1)!/xᵏ"_test = [] {
    const double x0 = 2.0;
    auto y = log(jetVariable<double, 4>(x0));
    expectClose((nthDerivative(y, 0)), std::log(x0), {.rtol = 1e-11, .atol = 1e-11});
    expectClose((nthDerivative(y, 1)), 1.0 / x0, {.rtol = 1e-11, .atol = 1e-11});
    expectClose((nthDerivative(y, 2)), -1.0 / (x0 * x0), {.rtol = 1e-11, .atol = 1e-11});
    expectClose((nthDerivative(y, 3)), 2.0 / (x0 * x0 * x0), {.rtol = 1e-11, .atol = 1e-11});
  };

  "sqrt and division recurrences"_test = [] {
    const double x0 = 3.0;
    auto s = sqrt(jetVariable<double, 2>(x0));
    expectClose((nthDerivative(s, 1)), 1.0 / (2 * std::sqrt(x0)), {.rtol = 1e-11, .atol = 1e-11});
    expectClose((nthDerivative(s, 2)), -1.0 / (4 * std::pow(x0, 1.5)), {.rtol = 1e-11, .atol = 1e-11});
    // 1/x: f' = −1/x², f'' = 2/x³
    auto r = jetConstant<double, 2>(1.0) / jetVariable<double, 2>(x0);
    expectClose((nthDerivative(r, 1)), -1.0 / (x0 * x0), {.rtol = 1e-11, .atol = 1e-11});
    expectClose((nthDerivative(r, 2)), 2.0 / (x0 * x0 * x0), {.rtol = 1e-11, .atol = 1e-11});
  };

  "composition sin(x²): second derivative against the closed form"_test = [] {
    const double x0 = 0.9;
    auto x = jetVariable<double, 2>(x0);
    auto y = sin(x * x);
    // d/dx sin(x²) = 2x cos(x²);  d²/dx² = 2cos(x²) − 4x² sin(x²)
    expectClose((nthDerivative(y, 1)), 2 * x0 * std::cos(x0 * x0), {.rtol = 1e-10, .atol = 1e-10});
    expectClose((nthDerivative(y, 2)),
                2 * std::cos(x0 * x0) - 4 * x0 * x0 * std::sin(x0 * x0),
                {.rtol = 1e-10, .atol = 1e-10});
  };

  return TestRegistry::result();
}
