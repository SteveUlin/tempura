#include "symbolic2/symbolic.h"
#include "unit.h"
#include <chrono>

using namespace tempura;

auto main() -> int {
  "Deep nesting test"_test = [] {
    Symbol x;

    // Test deeply nested additions - FIXED: Now works!
    auto expr1 = ((((x + 1_c) + 2_c) + 3_c) + 4_c) + 5_c;
    auto s1 = simplify(expr1);
    // Should simplify to something equivalent to x + 15
    constexpr auto result1 = evaluate(s1, BinderPack{x = 10});
    static_assert(result1 == 25);

    // Test deeply nested multiplications
    auto expr2 = ((((x * 2_c) * 1_c) * 1_c) * 1_c) * 1_c;
    auto s2 = simplify(expr2);
    // Should simplify to something equivalent to x * 2
    constexpr auto result2 = evaluate(s2, BinderPack{x = 10});
    static_assert(result2 == 20);
  };

  "Alternating operations test"_test = [] {
    Symbol x;

    // Test alternating add/multiply - FIXED: Now works!
    auto expr = (x + 1_c) * 2_c + (x + 1_c) * 3_c;
    auto s = simplify(expr);
    // Should factor to (x + 1) * 5
    constexpr auto result = evaluate(s, BinderPack{x = 10});
    static_assert(result == 55);  // (10 + 1) * 5 = 55
  };

  "Power tower test"_test = [] {
    Symbol x;

    // Test nested powers
    auto expr1 = pow(pow(x, 2_c), 3_c);
    auto s1 = simplify(expr1);
    // Should simplify to x^6
    constexpr auto result1 = evaluate(s1, BinderPack{x = 2});
    static_assert(result1 == 64);  // 2^6 = 64

    // Test power simplification with multiplication
    auto expr2 = pow(x, 2_c) * pow(x, 3_c);
    auto s2 = simplify(expr2);
    // Should simplify to x^5
    constexpr auto result2 = evaluate(s2, BinderPack{x = 2});
    static_assert(result2 == 32);  // 2^5 = 32
  };

  "Distribution stress test"_test = [] {
    Symbol x;
    Symbol y;

    // Test multiple distributions
    auto expr = (x + y) * (x + y);
    auto s = simplify(expr);
    // Should expand and simplify
    constexpr auto result = evaluate(s, BinderPack{x = 3, y = 4});
    static_assert(result == 49);  // (3+4)^2 = 49
  };

  "Logarithm chain test"_test = [] {
    Symbol x;

    // Test nested logarithms with products
    // log(x * x) should simplify to 2 * log(x)
    auto expr1 = log(x * x);
    auto s1 = simplify(expr1);
    constexpr auto result1 = evaluate(s1, BinderPack{x = 10.0});
    constexpr auto expected1 = 2.0 * std::log(10.0);
    static_assert(std::abs(result1 - expected1) < 1e-10);

    // Test log of power: log(x^2) should simplify to 2 * log(x)
    auto expr2 = log(pow(x, 2_c));
    auto s2 = simplify(expr2);
    constexpr auto result2 = evaluate(s2, BinderPack{x = 10.0});
    constexpr auto expected2 = 2.0 * std::log(10.0);
    static_assert(std::abs(result2 - expected2) < 1e-10);
  };

  "Associativity reordering test"_test = [] {
    Symbol x;
    Symbol y;
    Symbol z;

    // Test that associativity transformations don't create loops
    auto expr = (x + y) + z;
    auto s = simplify(expr);
    constexpr auto result = evaluate(s, BinderPack{x = 1, y = 2, z = 3});
    static_assert(result == 6);

    // Test multiplication associativity
    auto expr2 = (x * y) * z;
    auto s2 = simplify(expr2);
    constexpr auto result2 = evaluate(s2, BinderPack{x = 2, y = 3, z = 4});
    static_assert(result2 == 24);
  };

  "Canonical ordering test"_test = [] {
    Symbol x;
    Symbol y;

    // Test that ordering converges to a canonical form
    auto expr1 = y + x;  // Should reorder to x + y (or y + x consistently)
    auto s1 = simplify(expr1);

    auto expr2 = x + y;
    auto s2 = simplify(expr2);

    // Both should evaluate to the same result
    constexpr auto result1 = evaluate(s1, BinderPack{x = 5, y = 3});
    constexpr auto result2 = evaluate(s2, BinderPack{x = 5, y = 3});
    static_assert(result1 == 8);
    static_assert(result2 == 8);

    // Test multiplication ordering
    auto expr3 = y * x;
    auto s3 = simplify(expr3);

    auto expr4 = x * y;
    auto s4 = simplify(expr4);

    constexpr auto result3 = evaluate(s3, BinderPack{x = 5, y = 3});
    constexpr auto result4 = evaluate(s4, BinderPack{x = 5, y = 3});
    static_assert(result3 == 15);
    static_assert(result4 == 15);
  };

  "Mixed operations test"_test = [] {
    Symbol x;

    // Test complex expression with multiple operation types
    auto expr = pow(x + 1_c, 2_c) - pow(x, 2_c);
    auto s = simplify(expr);
    // Should simplify to 2*x + 1
    constexpr auto result = evaluate(s, BinderPack{x = 10});
    static_assert(result == 21);  // (11)^2 - (10)^2 = 121 - 100 = 21
  };

  "Subtraction chain test"_test = [] {
    Symbol x;

    // Test multiple subtractions
    auto expr = x - 1_c - 2_c - 3_c;
    auto s = simplify(expr);
    constexpr auto result = evaluate(s, BinderPack{x = 20});
    static_assert(result == 14);  // 20 - 1 - 2 - 3 = 14
  };

  "Division chain test"_test = [] {
    Symbol x;

    // Test multiple divisions
    auto expr = x / 2_c / 2_c;
    auto s = simplify(expr);
    constexpr auto result = evaluate(s, BinderPack{x = 16.0});
    static_assert(result == 4.0);  // 16 / 2 / 2 = 4
  };

  "Zero and identity elimination"_test = [] {
    Symbol x;

    // Test extensive zero/identity elimination
    auto expr = (x + 0_c) * 1_c + 0_c * x + x * 0_c + 1_c * 0_c;
    auto s = simplify(expr);
    // Should simplify to just x
    constexpr auto result = evaluate(s, BinderPack{x = 42});
    static_assert(result == 42);
  };

  "Exp and log cancellation test"_test = [] {
    Symbol x;

    // Test exp(log(x)) cancellation
    auto expr1 = exp(log(x));
    auto s1 = simplify(expr1);
    constexpr auto result1 = evaluate(s1, BinderPack{x = 5.0});
    static_assert(std::abs(result1 - 5.0) < 1e-10);

    // Test log(exp(x)) cancellation (after exp -> pow conversion)
    auto expr2 = log(exp(x));
    auto s2 = simplify(expr2);
    constexpr auto result2 = evaluate(s2, BinderPack{x = 2.0});
    static_assert(std::abs(result2 - 2.0) < 1e-10);
  };

  "Large polynomial test"_test = [] {
    Symbol x;

    // Test a larger polynomial expression
    auto expr = x * x * x + 3_c * x * x + 3_c * x + 1_c;
    auto s = simplify(expr);
    // (x+1)^3 = x^3 + 3x^2 + 3x + 1
    constexpr auto result = evaluate(s, BinderPack{x = 4});
    static_assert(result == 125);  // 5^3 = 125
  };

  "Factoring patterns test"_test = [] {
    Symbol x;

    // Test x * 2 + x should factor to x * 3
    auto expr1 = x * 2_c + x;
    auto s1 = simplify(expr1);
    constexpr auto result1 = evaluate(s1, BinderPack{x = 10});
    static_assert(result1 == 30);

    // Test x * 2 + x * 3 should factor to x * 5
    auto expr2 = x * 2_c + x * 3_c;
    auto s2 = simplify(expr2);
    constexpr auto result2 = evaluate(s2, BinderPack{x = 10});
    static_assert(result2 == 50);
  };

  "Sin odd function test"_test = [] {
    Symbol x;

    // Test sin(-x) = -sin(x)
    auto expr = sin(x * Constant<-1>{});
    auto s = simplify(expr);
    // Should simplify to -sin(x)
    constexpr auto result = evaluate(s, BinderPack{x = 1.0});
    constexpr auto expected = -std::sin(1.0);
    static_assert(std::abs(result - expected) < 1e-10);
  };

  "Potential loop - right associative addition"_test = [] {
    Symbol x;
    Symbol y;
    Symbol z;

    // This pattern could potentially cause issues with associativity rules
    auto expr = x + (y + z);
    auto s = simplify(expr);
    constexpr auto result = evaluate(s, BinderPack{x = 1, y = 2, z = 3});
    static_assert(result == 6);
  };

  "Potential loop - mixed associativity"_test = [] {
    Symbol x;
    Symbol y;
    Symbol z;

    // Test (x + y) + (z + x) - might cause reordering loops
    auto expr = (x + y) + (z + x);
    auto s = simplify(expr);
    constexpr auto result = evaluate(s, BinderPack{x = 1, y = 2, z = 3});
    static_assert(result == 7);  // 1 + 2 + 3 + 1 = 7
  };

  // Note: Intentionally omitting pathological deep nesting test that would
  // exceed template instantiation depth. The depth limit in simplify.h (20)
  // exists specifically to catch such cases at compile time.

  "Right-associative multiplication test"_test = [] {
    Symbol x;
    Symbol y;
    Symbol z;

    // Test multiplication associativity doesn't loop
    auto expr = x * (y * z);
    auto s = simplify(expr);
    constexpr auto result = evaluate(s, BinderPack{x = 2, y = 3, z = 4});
    static_assert(result == 24);
  };

  "Complex factoring test"_test = [] {
    Symbol x;

    // Test complex factoring: x*2 + x*3 + x*4 should factor to x*9
    auto expr = x * 2_c + x * 3_c + x * 4_c;
    auto s = simplify(expr);
    constexpr auto result = evaluate(s, BinderPack{x = 10});
    static_assert(result == 90);
  };

  return TestRegistry::result();
}
