#include <cassert>
#include <cmath>

#include "symbolic3/symbolic3.h"
#include "unit.h"

using namespace tempura;
using namespace tempura::symbolic3;

auto main() -> int {
  // ============================================================================
  // BASIC EVALUATION TESTS
  // ============================================================================

  "Evaluate constant"_test = [] {
    constexpr auto result = evaluate(Constant<42>{}, BinderPack{});
    static_assert(result == 42);
  };

  "Evaluate symbol with binding"_test = [] {
    constexpr Symbol x;
    constexpr auto result = evaluate(x, BinderPack{x = 5});
    static_assert(result == 5);
  };

  "Evaluate symbol with double binding"_test = [] {
    constexpr Symbol x;
    auto result = evaluate(x, BinderPack{x = 3.14});
    assert(result == 3.14);
  };

  // ============================================================================
  // ARITHMETIC OPERATIONS
  // ============================================================================

  "Evaluate addition"_test = [] {
    constexpr Symbol x;
    constexpr Symbol y;
    auto expr = x + y;
    auto result = evaluate(expr, BinderPack{x = 2, y = 3});
    assert(result == 5);
  };

  "Evaluate subtraction"_test = [] {
    constexpr Symbol x;
    constexpr Symbol y;
    auto expr = x - y;
    auto result = evaluate(expr, BinderPack{x = 10, y = 3});
    assert(result == 7);
  };

  "Evaluate multiplication"_test = [] {
    constexpr Symbol x;
    constexpr Symbol y;
    auto expr = x * y;
    auto result = evaluate(expr, BinderPack{x = 4, y = 5});
    assert(result == 20);
  };

  "Evaluate division"_test = [] {
    constexpr Symbol x;
    constexpr Symbol y;
    auto expr = x / y;
    auto result = evaluate(expr, BinderPack{x = 20.0, y = 4.0});
    assert(result == 5.0);
  };

  "Evaluate negation"_test = [] {
    constexpr Symbol x;
    auto expr = -x;
    auto result = evaluate(expr, BinderPack{x = 7});
    assert(result == -7);
  };

  "Evaluate complex arithmetic"_test = [] {
    constexpr Symbol x;
    constexpr Symbol y;
    auto expr = (x + y) * (x - y);  // x² - y²
    auto result = evaluate(expr, BinderPack{x = 5, y = 3});
    assert(result == 16);  // 25 - 9 = 16
  };

  // ============================================================================
  // POWER OPERATIONS
  // ============================================================================

  "Evaluate power with integer exponent"_test = [] {
    constexpr Symbol x;
    auto expr = pow(x, Constant<2>{});
    auto result = evaluate(expr, BinderPack{x = 5.0});
    assert(result == 25.0);
  };

  "Evaluate power with variable exponent"_test = [] {
    constexpr Symbol x;
    constexpr Symbol n;
    auto expr = pow(x, n);
    auto result = evaluate(expr, BinderPack{x = 2.0, n = 3.0});
    assert(result == 8.0);
  };

  "Evaluate sqrt"_test = [] {
    constexpr Symbol x;
    auto expr = sqrt(x);
    auto result = evaluate(expr, BinderPack{x = 16.0});
    assert(result == 4.0);
  };

  // ============================================================================
  // TRANSCENDENTAL FUNCTIONS
  // ============================================================================

  "Evaluate sin"_test = [] {
    constexpr Symbol x;
    auto expr = sin(x);
    auto result = evaluate(expr, BinderPack{x = 0.0});
    assert(result == 0.0);
  };

  "Evaluate cos"_test = [] {
    constexpr Symbol x;
    auto expr = cos(x);
    auto result = evaluate(expr, BinderPack{x = 0.0});
    assert(result == 1.0);
  };

  "Evaluate tan"_test = [] {
    constexpr Symbol x;
    auto expr = tan(x);
    auto result = evaluate(expr, BinderPack{x = 0.0});
    assert(result == 0.0);
  };

  "Evaluate exp"_test = [] {
    constexpr Symbol x;
    auto expr = exp(x);
    auto result = evaluate(expr, BinderPack{x = 0.0});
    assert(result == 1.0);
  };

  "Evaluate log"_test = [] {
    constexpr Symbol x;
    auto expr = log(x);
    auto result = evaluate(expr, BinderPack{x = 1.0});
    assert(result == 0.0);
  };

  // ============================================================================
  // COMPILE-TIME EVALUATION
  // ============================================================================

  "Compile-time constant evaluation"_test = [] {
    constexpr auto expr = Constant<2>{} + Constant<3>{};
    constexpr auto result = evaluate(expr, BinderPack{});
    static_assert(result == 5);
  };

  "Compile-time with constant bindings"_test = [] {
    constexpr Symbol x;
    constexpr auto expr = x * x;
    constexpr auto result = evaluate(expr, BinderPack{x = 7});
    static_assert(result == 49);
  };

  "Compile-time power"_test = [] {
    constexpr Symbol x;
    constexpr auto expr = pow(x, Constant<3>{});
    // Note: pow with floating point is not fully constexpr in current C++
    auto result = evaluate(expr, BinderPack{x = 2.0});
    assert(result == 8.0);
  };

  "Compile-time sqrt"_test = [] {
    constexpr auto expr = sqrt(Constant<25.0>{});
    // Note: sqrt not fully constexpr yet
    auto result = evaluate(expr, BinderPack{});
    assert(result == 5.0);
  };

  "Compile-time sin"_test = [] {
    constexpr auto expr = sin(Constant<0.0>{});
    // Note: trig functions not fully constexpr yet
    auto result = evaluate(expr, BinderPack{});
    assert(result == 0.0);
  };

  "Compile-time exp"_test = [] {
    constexpr auto expr = exp(Constant<0.0>{});
    // Note: exp not fully constexpr yet
    auto result = evaluate(expr, BinderPack{});
    assert(result == 1.0);
  };

  // ============================================================================
  // NESTED EXPRESSIONS
  // ============================================================================

  "Evaluate nested expression"_test = [] {
    constexpr Symbol x;
    auto expr = sin(x * x);  // sin(x²)
    auto result = evaluate(expr, BinderPack{x = 0.0});
    assert(result == 0.0);
  };

  "Evaluate chain rule expression"_test = [] {
    constexpr Symbol x;
    auto expr = exp(log(x));  // Should equal x
    auto result = evaluate(expr, BinderPack{x = 5.0});
    assert(std::abs(result - 5.0) < 1e-10);
  };

  "Evaluate complex nested expression"_test = [] {
    constexpr Symbol x;
    constexpr Symbol y;
    auto expr = sin(x) * cos(y) + exp(x - y);
    auto result = evaluate(expr, BinderPack{x = 0.0, y = 0.0});
    assert(result == 1.0);  // sin(0)*cos(0) + exp(0) = 0 + 1 = 1
  };

  // ============================================================================
  // PRACTICAL EXAMPLES
  // ============================================================================

  "Evaluate quadratic formula"_test = [] {
    constexpr Symbol x;
    auto expr = x * x + Constant<2>{} * x + Constant<1>{};  // (x+1)²
    auto result = evaluate(expr, BinderPack{x = 3.0});
    assert(result == 16.0);  // 9 + 6 + 1 = 16
  };

  "Evaluate polynomial"_test = [] {
    constexpr Symbol x;
    auto expr = pow(x, Constant<3>{}) - Constant<2>{} * pow(x, Constant<2>{}) +
                x - Constant<5>{};
    auto result = evaluate(expr, BinderPack{x = 2.0});
    assert(result == -3.0);  // 8 - 8 + 2 - 5 = -3
  };

  "Evaluate gaussian"_test = [] {
    constexpr Symbol x;
    auto expr = exp(-(x * x) / Constant<2.0>{});  // e^(-x²/2)
    auto result = evaluate(expr, BinderPack{x = 0.0});
    assert(result == 1.0);
  };

  "Evaluate pythagorean identity"_test = [] {
    constexpr Symbol x;
    auto expr = sin(x) * sin(x) + cos(x) * cos(x);  // sin²(x) + cos²(x)
    auto result = evaluate(expr, BinderPack{x = 0.5});
    assert(std::abs(result - 1.0) < 1e-10);  // Should equal 1
  };

  "Evaluate with zero"_test = [] {
    constexpr Symbol x;
    auto expr = x * Constant<0>{};
    auto result = evaluate(expr, BinderPack{x = 999});
    assert(result == 0);
  };

  "Evaluate with one"_test = [] {
    constexpr Symbol x;
    auto expr = x * Constant<1>{};
    auto result = evaluate(expr, BinderPack{x = 42});
    assert(result == 42);
  };

  return TestRegistry::result();
}
