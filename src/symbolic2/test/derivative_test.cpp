#include "symbolic2/derivative.h"

#include "symbolic2/binding.h"
#include "symbolic2/evaluate.h"
#include "symbolic2/simplify.h"
#include "unit.h"

using namespace tempura;

auto main() -> int {
  "Derivative of constant"_test = [] {
    Symbol x;
    auto expr = 1_c;
    auto d = diff(expr, x);
    static_assert(match(d, 0_c));
  };

  "Derivative of different symbol"_test = [] {
    Symbol x, y;
    auto expr = y;
    auto d = diff(expr, x);
    static_assert(match(d, 0_c));
  };

  "Derivative of same symbol"_test = [] {
    Symbol x;
    auto expr = x;
    auto d = diff(expr, x);
    static_assert(match(d, 1_c));
  };

  "Derivative of x + 1"_test = [] {
    Symbol x;
    auto expr = x + 1_c;
    auto d = diff(expr, x);
    static_assert(match(d, 1_c + 0_c));
  };

  "Derivative of x - 1"_test = [] {
    Symbol x;
    auto expr = x - 1_c;
    auto d = diff(expr, x);
    static_assert(match(d, 1_c - 0_c));
  };

  "Derivative of -x"_test = [] {
    Symbol x;
    auto expr = -x;
    auto d = diff(expr, x);
    static_assert(match(d, -1_c));
  };

  "Derivative of x * x (product rule)"_test = [] {
    Symbol x;
    auto expr = x * x;
    auto d = diff(expr, x);
    // d/dx(x*x) = 1*x + x*1
    static_assert(match(d, 1_c * x + x * 1_c));
  };

  "Derivative of x / x (quotient rule)"_test = [] {
    Symbol x;
    auto expr = x / x;
    auto d = diff(expr, x);
    // d/dx(x/x) = (1*x - x*1) / x²
    static_assert(match(d, (1_c * x - x * 1_c) / pow(x, 2_c)));
  };

  "Derivative of x^2 (power rule)"_test = [] {
    Symbol x;
    auto expr = pow(x, 2_c);
    auto d = diff(expr, x);
    // d/dx(x²) = 2*x^(2-1)*1 = 2*x^1*1
    static_assert(match(d, 2_c * pow(x, 2_c - 1_c) * 1_c));
  };

  "Derivative of x^3"_test = [] {
    Symbol x;
    auto expr = pow(x, 3_c);
    auto d = diff(expr, x);
    // d/dx(x³) = 3*x²
    static_assert(match(d, 3_c * pow(x, 3_c - 1_c) * 1_c));
  };

  "Derivative of sin(x)"_test = [] {
    Symbol x;
    auto expr = sin(x);
    auto d = diff(expr, x);
    // d/dx(sin(x)) = cos(x)*1
    static_assert(match(d, cos(x) * 1_c));
  };

  "Derivative of cos(x)"_test = [] {
    Symbol x;
    auto expr = cos(x);
    auto d = diff(expr, x);
    // d/dx(cos(x)) = -sin(x)*1
    static_assert(match(d, -sin(x) * 1_c));
  };

  "Derivative of tan(x)"_test = [] {
    Symbol x;
    auto expr = tan(x);
    auto d = diff(expr, x);
    // d/dx(tan(x)) = 1/cos²(x)
    static_assert(match(d, (1_c / pow(cos(x), 2_c)) * 1_c));
  };

  "Derivative of exp(x)"_test = [] {
    Symbol x;
    auto expr = exp(x);
    auto d = diff(expr, x);
    // d/dx(e^x) = e^x
    static_assert(match(d, exp(x) * 1_c));
  };

  "Derivative of log(x)"_test = [] {
    Symbol x;
    auto expr = log(x);
    auto d = diff(expr, x);
    // d/dx(log(x)) = 1/x
    static_assert(match(d, (1_c / x) * 1_c));
  };

  "Derivative of sqrt(x)"_test = [] {
    Symbol x;
    auto expr = sqrt(x);
    auto d = diff(expr, x);
    // d/dx(√x) = 1/(2√x)
    static_assert(match(d, (1_c / (2_c * sqrt(x))) * 1_c));
  };

  "Chain rule: sin(x^2)"_test = [] {
    Symbol x;
    auto expr = sin(pow(x, 2_c));
    auto d = diff(expr, x);
    // d/dx(sin(x²)) = cos(x²) * 2x
    static_assert(match(d, cos(pow(x, 2_c)) * (2_c * pow(x, 2_c - 1_c) * 1_c)));
  };

  "Chain rule: exp(x^2)"_test = [] {
    Symbol x;
    auto expr = exp(pow(x, 2_c));
    auto d = diff(expr, x);
    // d/dx(e^(x²)) = e^(x²) * 2x
    static_assert(match(d, exp(pow(x, 2_c)) * (2_c * pow(x, 2_c - 1_c) * 1_c)));
  };

  "Complex: x² + 2x + 1"_test = [] {
    Symbol x;
    auto expr = pow(x, 2_c) + 2_c * x + 1_c;
    auto d = diff(expr, x);
    // d/dx(x² + 2x + 1) = 2x + 2
    // The actual result will be: 2*x^1*1 + (0*x + 2*1) + 0
    auto expected = 2_c * pow(x, 2_c - 1_c) * 1_c + (0_c * x + 2_c * 1_c) + 0_c;
    static_assert(match(d, expected));
  };

  "Complex: (x + 1) * (x - 1)"_test = [] {
    Symbol x;
    auto expr = (x + 1_c) * (x - 1_c);
    auto d = diff(expr, x);
    // d/dx((x+1)(x-1)) = (1+0)*(x-1) + (x+1)*(1-0)
    auto expected = (1_c + 0_c) * (x - 1_c) + (x + 1_c) * (1_c - 0_c);
    static_assert(match(d, expected));
  };

  "Evaluation of derivative"_test = [] {
    Symbol x;
    auto expr = pow(x, 2_c);  // f(x) = x²
    auto d = diff(expr, x);   // f'(x) = 2x
    auto simplified = simplify(d);

    // At x=5, f'(5) = 2*5 = 10
    constexpr auto result = evaluate(simplified, BinderPack{x = 5});
    static_assert(result == 10);
  };

  "Evaluation: derivative of sin(x) at π"_test = [] {
    Symbol x;
    auto expr = sin(x);
    auto d = diff(expr, x);         // cos(x) * 1
    auto simplified = simplify(d);  // Should simplify to cos(x)

    // cos(π) ≈ -1
    auto result = evaluate(simplified, BinderPack{x = M_PI});
    expectNear<0.0001>(result, -1.0);
  };

  return 0;
}
