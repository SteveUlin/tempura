// Differentiation interpreter tests
#include <cmath>

#include "symbolic4/core.h"
#include "symbolic4/interpreter/diff.h"
#include "symbolic4/interpreter/eval.h"
#include "symbolic4/interpreter/to_string.h"
#include "symbolic4/operators.h"
#include "unit.h"

using namespace tempura;
using namespace tempura::symbolic4;

// Helper to verify derivative both symbolically and numerically
template <Symbolic E, Symbolic D, typename Var, typename... Bindings>
void expectDerivative(E /*expr*/, D deriv, Var var, std::string_view expected_form,
                      double at_value, double expected_value,
                      Bindings... bindings) {
  // Check symbolic form
  auto form = toString(deriv, bindings...);
  expectEq(form, std::string(expected_form));

  // Check numerical value
  expectNear(evaluate(deriv, var = at_value), expected_value);
}

auto main() -> int {
  Symbol<struct X> x;
  Symbol<struct Y> y;

  "diff constant"_test = [&] {
    auto deriv = diff(Constant<5>{}, x);
    static_assert(is_constant_v<decltype(deriv)>);
    static_assert(deriv.value == 0);
    expectEq(toString(deriv), std::string("0"));
  };

  "diff variable wrt itself"_test = [&] {
    auto deriv = diff(x, x);
    static_assert(is_constant_v<decltype(deriv)>);
    static_assert(deriv.value == 1);
    expectEq(toString(deriv), std::string("1"));
  };

  "diff variable wrt different variable"_test = [&] {
    auto deriv = diff(x, y);
    static_assert(is_constant_v<decltype(deriv)>);
    static_assert(deriv.value == 0);
    expectEq(toString(deriv), std::string("0"));
  };

  "diff addition"_test = [&] {
    // d/dx(x + 3) = 1 + 0 = 1
    auto expr = x + Constant<3>{};
    auto deriv = diff(expr, x);
    expectDerivative(expr, deriv, x, "1 + 0", 5.0, 1.0, name(x, "x"));
  };

  "diff subtraction"_test = [&] {
    // d/dx(x - 3) = 1 - 0 = 1
    auto expr = x - Constant<3>{};
    auto deriv = diff(expr, x);
    expectDerivative(expr, deriv, x, "1 - 0", 5.0, 1.0, name(x, "x"));
  };

  "diff multiplication (product rule)"_test = [&] {
    // d/dx(x * x) = x * 1 + 1 * x
    auto expr = x * x;
    auto deriv = diff(expr, x);
    expectDerivative(expr, deriv, x, "x * 1 + 1 * x", 3.0, 6.0, name(x, "x"));
    expectNear(evaluate(deriv, x = 5.0), 10.0);  // Additional check
  };

  "diff division (quotient rule)"_test = [&] {
    // d/dx(x / 2) = (1 * 2 - x * 0) / (2 * 2)
    auto expr = x / Constant<2>{};
    auto deriv = diff(expr, x);
    expectDerivative(expr, deriv, x, "(1 * 2 - x * 0) / (2 * 2)", 10.0, 0.5,
                     name(x, "x"));
  };

  "diff sin"_test = [&] {
    // d/dx(sin(x)) = cos(x) * 1 = cos(x)
    auto deriv = diff(sin(x), x);
    expectDerivative(sin(x), deriv, x, "cos(x) * 1", 0.0, 1.0, name(x, "x"));
    expectNear(evaluate(deriv, x = M_PI / 2), 0.0, 1e-10);
  };

  "diff cos"_test = [&] {
    // d/dx(cos(x)) = -sin(x) * 1
    auto deriv = diff(cos(x), x);
    expectDerivative(cos(x), deriv, x, "-(sin(x)) * 1", 0.0, 0.0, name(x, "x"));
    expectNear(evaluate(deriv, x = M_PI / 2), -1.0, 1e-10);
  };

  "diff exp"_test = [&] {
    // d/dx(e^x) = e^x * 1
    auto deriv = diff(exp(x), x);
    expectDerivative(exp(x), deriv, x, "exp(x) * 1", 0.0, 1.0, name(x, "x"));
    expectNear(evaluate(deriv, x = 1.0), M_E, 1e-10);
  };

  "diff log"_test = [&] {
    // d/dx(log(x)) = 1/x (chain rule simplified since dx/dx = 1)
    auto deriv = diff(log(x), x);
    expectDerivative(log(x), deriv, x, "1 / x", 1.0, 1.0, name(x, "x"));
    expectNear(evaluate(deriv, x = 2.0), 0.5);
  };

  "diff sqrt"_test = [&] {
    // d/dx(sqrt(x)) = 1/(2*sqrt(x)) (chain rule simplified)
    auto deriv = diff(sqrt(x), x);
    expectDerivative(sqrt(x), deriv, x, "1 / (2 * sqrt(x))", 4.0, 0.25,
                     name(x, "x"));
    expectNear(evaluate(deriv, x = 1.0), 0.5);
  };

  "diff chain rule sin(x^2)"_test = [&] {
    // d/dx(sin(x*x)) = cos(x*x) * (x*1 + 1*x)
    auto expr = sin(x * x);
    auto deriv = diff(expr, x);
    expectDerivative(expr, deriv, x, "cos(x * x) * (x * 1 + 1 * x)", 0.0, 0.0,
                     name(x, "x"));
    // At x=1: cos(1) * 2 ≈ 1.08
    expectNear(evaluate(deriv, x = 1.0), std::cos(1.0) * 2.0, 1e-10);
  };

  "diff chain rule exp(sin(x))"_test = [&] {
    // d/dx(exp(sin(x))) = exp(sin(x)) * cos(x) * 1
    auto expr = exp(sin(x));
    auto deriv = diff(expr, x);
    // Chain rule: outer' * inner' = exp(sin(x)) * cos(x) * 1
    expectDerivative(expr, deriv, x, "exp(sin(x)) * cos(x) * 1", 0.0,
                     1.0,  // exp(0) * cos(0) = 1 * 1
                     name(x, "x"));
  };

  "diff power"_test = [&] {
    // d/dx(x^3) uses the general power rule
    auto expr = pow(x, Constant<3>{});
    auto deriv = diff(expr, x);
    // Power rule: x^n * (n * 1/x * dx/dx + ln(x) * dn/dx)
    // For constant exponent: x^3 * (3 * 1/x * 1 + log(x) * 0) = x^3 * 3/x = 3x^2
    expectNear(evaluate(deriv, x = 2.0), 12.0);  // 3 * 2^2 = 12
    expectNear(evaluate(deriv, x = 3.0), 27.0);  // 3 * 3^2 = 27
  };

  "diff multivariate"_test = [&] {
    // d/dx(x * y) = 1 * y + x * 0 = y (treating y as constant wrt x)
    auto expr = x * y;
    auto deriv = diff(expr, x);
    expectEq(toString(deriv, name(x, "x"), name(y, "y")),
             std::string("x * 0 + 1 * y"));
    // At x=2, y=3: derivative wrt x is y = 3
    expectNear(evaluate(deriv, x = 2.0, y = 3.0), 3.0);
  };

  return TestRegistry::result();
}
