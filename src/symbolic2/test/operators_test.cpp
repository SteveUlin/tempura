#include "symbolic2/symbolic.h"

#include "unit.h"

using namespace tempura;

auto main() -> int {
  "Addition operator"_test = [] {
    Symbol a, b, c;

    static_assert(evaluate(a + a, BinderPack{a = 5}) == 10);
    static_assert(evaluate(a + b, BinderPack{a = 5, b = 2}) == 7);
    static_assert(evaluate(a + b + c, BinderPack{a = 5, b = 2, c = 1}) == 8);
  };

  "Subtraction operator"_test = [] {
    Symbol a, b;

    static_assert(evaluate(a - a, BinderPack{a = 5}) == 0);
    static_assert(evaluate(a - b, BinderPack{a = 5, b = 1}) == 4);
  };

  "Negation operator"_test = [] {
    Symbol a;

    static_assert(evaluate(-a, BinderPack{a = 5}) == -5);
    static_assert(evaluate(-a, BinderPack{a = -3}) == 3);
  };

  "Multiplication operator"_test = [] {
    Symbol a, b;

    static_assert(evaluate(a * a, BinderPack{a = 5}) == 25);
    static_assert(evaluate(a * b, BinderPack{a = 5, b = 2}) == 10);
  };

  "Division operator"_test = [] {
    Symbol a, b;

    static_assert(evaluate(a / a, BinderPack{a = 5}) == 1);
    static_assert(evaluate(a / b, BinderPack{a = 10, b = 2}) == 5);
  };

  "Power function"_test = [] {
    Symbol a, b;

    // pow(5, 5) = 3125
    static_assert(evaluate(pow(a, a), BinderPack{a = 5}) == 3125);
    // pow(10, 2) = 100
    static_assert(evaluate(pow(a, b), BinderPack{a = 10, b = 2}) == 100);
  };

  "Square root function"_test = [] {
    Symbol a;

    static_assert(evaluate(sqrt(a), BinderPack{a = 25}) == 5);
    static_assert(evaluate(sqrt(a), BinderPack{a = 16}) == 4);
  };

  "Exponential function"_test = [] {
    Symbol a;

    // exp(0) = 1
    static_assert(evaluate(exp(a), BinderPack{a = 0}) == 1);
    // We can't easily test exp(1) ≈ e at compile time without floating point
  };

  "Logarithm function"_test = [] {
    Symbol a;

    // log(1) = 0
    static_assert(evaluate(log(a), BinderPack{a = 1}) == 0);
  };

  "Sine function"_test = [] {
    Symbol a;

    // sin(0) = 0
    static_assert(evaluate(sin(a), BinderPack{a = 0}) == 0);
  };

  "Cosine function"_test = [] {
    Symbol a;

    // cos(0) = 1
    static_assert(evaluate(cos(a), BinderPack{a = 0}) == 1);
  };

  "Tangent function"_test = [] {
    Symbol a;

    // tan(0) = 0
    static_assert(evaluate(tan(a), BinderPack{a = 0}) == 0);
  };

  "Constant e"_test = [] {
    // e ≈ 2.718...
    // We can evaluate it at compile time
    constexpr auto result = evaluate(e, BinderPack{});
    static_assert(result > 2 && result < 3);
  };

  "Constant pi"_test = [] {
    // π ≈ 3.14159...
    constexpr auto result = evaluate(π, BinderPack{});
    static_assert(result > 3 && result < 4);
  };

  "Complex expression"_test = [] {
    Symbol x;

    // Test (x² + 2x + 1) at x = 5
    // Result should be 25 + 10 + 1 = 36
    auto expr = x * x + 2_c * x + 1_c;
    static_assert(evaluate(expr, BinderPack{x = 5}) == 36);
  };

  return 0;
}
