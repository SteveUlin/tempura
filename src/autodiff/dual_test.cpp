#include "dual.h"

#include <cmath>
#include <concepts>

#include "unit.h"

using namespace tempura;
using namespace tempura::autodiff;

// constexpr: derivative of x² at x=3 is 2x=6 (exact polynomial — proves the constexpr path).
constexpr auto derivXSquared() -> double {
  Dual<double> x{3.0, 1.0};
  return (x * x).gradient;
}
static_assert(derivXSquared() == 6.0);

// constexpr transcendental — also proves std::sin/cos are constexpr under GCC trunk.
constexpr auto sinDerivConstexpr() -> bool {
  Dual<double> x{0.5, 1.0};
  return isClose(sin(x).gradient, std::cos(0.5), {.rtol = 1e-12, .atol = 1e-12});
}
static_assert(sinDerivConstexpr());

auto main() -> int {
  "value-only comparison ignores the tangent"_test = [] {
    Dual<double> a{2.0, 1.0};
    Dual<double> b{2.0, 99.0};  // same value, different derivative
    expectTrue(a == b);
    expectFalse(a < b);
    expectTrue(Dual<double>{1.0, 5.0} < Dual<double>{2.0, 0.0});
  };

  "product and quotient rules"_test = [] {
    Dual<double> x{3.0, 1.0};
    auto p = x * x;  // x² → 9, 2x = 6
    expectClose((p.value), 9.0, {.rtol = 1e-12, .atol = 1e-12});
    expectClose((p.gradient), 6.0, {.rtol = 1e-12, .atol = 1e-12});
    auto q = Dual<double>{1.0, 0.0} / x;  // 1/x → d = −1/x² = −1/9
    expectClose((q.value), 1.0 / 3.0, {.rtol = 1e-12, .atol = 1e-12});
    expectClose((q.gradient), -1.0 / 9.0, {.rtol = 1e-12, .atol = 1e-12});
  };

  "scalar * Dual keeps the dual's value type and does not truncate"_test = [] {
    auto y = 2 * Dual<double>{2.5, 1.0};  // int scalar — old bug deduced Dual<int> → 4.0
    static_assert(std::same_as<decltype(y), Dual<double>>);
    expectClose((y.value), 5.0, {.rtol = 1e-12, .atol = 1e-12});
    expectClose((y.gradient), 2.0, {.rtol = 1e-12, .atol = 1e-12});
    // all four scalar-mixed ops carry the constant (zero-tangent) correctly
    Dual<double> x{4.0, 1.0};
    expectClose(((x + 10).value), 14.0, {.rtol = 1e-12, .atol = 1e-12});
    expectClose(((10 - x).gradient), -1.0, {.rtol = 1e-12, .atol = 1e-12});
    expectClose(((x / 2).gradient), 0.5, {.rtol = 1e-12, .atol = 1e-12});
    expectClose(((8 / x).gradient), -0.5, {.rtol = 1e-12, .atol = 1e-12});  // −8/x² = −0.5
  };

  "elementary function derivatives (chain rule)"_test = [] {
    Dual<double> x{0.7, 1.0};
    expectClose((sin(x).gradient), std::cos(0.7), {.rtol = 1e-12, .atol = 1e-12});
    expectClose((cos(x).gradient), -std::sin(0.7), {.rtol = 1e-12, .atol = 1e-12});
    expectClose((exp(x).gradient), std::exp(0.7), {.rtol = 1e-12, .atol = 1e-12});
    expectClose((log(x).gradient), 1.0 / 0.7, {.rtol = 1e-12, .atol = 1e-12});
    expectClose((sqrt(x).gradient), 1.0 / (2 * std::sqrt(0.7)), {.rtol = 1e-12, .atol = 1e-12});
    // composition sin(x²): dy/dx = cos(x²)·2x
    auto y = sin(x * x);
    expectClose((y.gradient), std::cos(0.49) * 1.4, {.rtol = 1e-12, .atol = 1e-12});
    // pow with constant exponent: d/dx x³ = 3x²
    expectClose((pow(x, 3.0).gradient), 3 * 0.49, {.rtol = 1e-12, .atol = 1e-12});
  };

  "nested Dual<Dual> gives exact second derivatives"_test = [] {
    // x = a + ε₁ + ε₂ (ε₁ε₂ coeff 0); after f, the ε₁ε₂ coefficient is f''(a).
    using DD = Dual<Dual<double>>;
    DD x{Dual<double>{1.0, 1.0}, Dual<double>{1.0, 0.0}};
    auto r = x * x * x;  // f = x³ at a=1 → f=1, f'=3, f''=6
    expectClose((r.value.value), 1.0, {.rtol = 1e-12, .atol = 1e-12});
    expectClose((r.value.gradient), 3.0, {.rtol = 1e-12, .atol = 1e-12});  // f'
    expectClose((r.gradient.gradient), 6.0, {.rtol = 1e-12, .atol = 1e-12});  // f''
  };

  return TestRegistry::result();
}
