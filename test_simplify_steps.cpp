#include <iostream>

#include "symbolic3/symbolic3.h"

using namespace tempura::symbolic3;

int main() {
  constexpr Symbol x;
  constexpr Symbol y;
  constexpr Symbol z;
  constexpr auto ctx = default_context();

  // Test 1: Simple zero multiplication
  {
    constexpr auto expr = z * 0_c;
    constexpr auto result = algebraic_simplify.apply(expr, ctx);
    std::cout << "Test 1: z * 0\n";
    std::cout << "  Result type: "
              << std::is_same_v<decltype(result),
                                decltype(expr)> << " (1=unchanged)\n";
    std::cout << "  Is Constant<0>: "
              << std::is_same_v<decltype(result), Constant<0>> << "\n\n";
  }

  // Test 2: Simple addition identity
  {
    constexpr auto expr = y + 0_c;
    constexpr auto result = algebraic_simplify.apply(expr, ctx);
    std::cout << "Test 2: y + 0\n";
    std::cout << "  Result type unchanged: "
              << std::is_same_v<decltype(result), decltype(expr)> << "\n";
    std::cout << "  Is Symbol: "
              << std::is_same_v<decltype(result), Symbol> << "\n\n";
  }

  // Test 3: Nested - y + (z * 0)
  {
    constexpr auto expr = y + (z * 0_c);
    constexpr auto result = algebraic_simplify.apply(expr, ctx);
    std::cout << "Test 3: y + (z * 0) with algebraic_simplify (no traversal)\n";
    std::cout << "  Result type unchanged: "
              << std::is_same_v<decltype(result), decltype(expr)> << "\n\n";
  }

  // Test 4: Same with innermost
  {
    constexpr auto expr = y + (z * 0_c);
    constexpr auto result =
        innermost(try_strategy(algebraic_simplify)).apply(expr, ctx);
    std::cout << "Test 4: y + (z * 0) with "
                 "innermost(try_strategy(algebraic_simplify))\n";
    std::cout << "  Result is y: "
              << std::is_same_v<decltype(result), Symbol> << "\n\n";
  }

  // Test 5: Full nested expression
  {
    constexpr auto expr = x * (y + (z * 0_c));
    std::cout << "Test 5: x * (y + (z * 0))\n";

    // Try algebraic_simplify alone
    constexpr auto step1 = algebraic_simplify.apply(expr, ctx);
    std::cout << "  After algebraic_simplify: unchanged="
              << std::is_same_v<decltype(step1), decltype(expr)> << "\n";

    // Try innermost
    constexpr auto step2 =
        innermost(try_strategy(algebraic_simplify)).apply(expr, ctx);
    std::cout << "  After innermost: is x*y="
              << (match(step2, x * y) || match(step2, y * x)) << "\n";

    // Try hybrid_simplify
    constexpr auto step3 = hybrid_simplify.apply(expr, ctx);
    std::cout << "  After hybrid_simplify: is x*y="
              << (match(step3, x * y) || match(step3, y * x)) << "\n";

    // Try full_simplify
    constexpr auto step4 = full_simplify(expr, ctx);
    std::cout << "  After full_simplify: is x*y="
              << (match(step4, x * y) || match(step4, y * x)) << "\n\n";
  }

  return 0;
}
