#include <iostream>

#include "symbolic2/symbolic.h"

using namespace tempura;

auto main() -> int {
  std::cout << "=== Tempura Symbolic 2.0 - Basic Usage ===\n\n";

  // --- Symbol Creation ---
  std::cout << "1. Symbol Creation\n";
  Symbol x;
  Symbol y;
  Symbol z;
  std::cout << "   Created symbols: x, y, z\n";
  std::cout << "   (Each is a unique type)\n\n";

  // --- Building Expressions ---
  std::cout << "2. Building Expressions\n";
  auto expr1 = x + y;
  auto expr2 = x * y;
  auto expr3 = pow(x, 2_c);
  auto expr4 = sin(x) + cos(y);
  std::cout << "   expr1 = x + y\n";
  std::cout << "   expr2 = x * y\n";
  std::cout << "   expr3 = x^2\n";
  std::cout << "   expr4 = sin(x) + cos(y)\n\n";

  // --- Evaluation ---
  std::cout << "3. Evaluation\n";
  {
    auto result1 = evaluate(expr1, BinderPack{x = 5, y = 3});
    auto result2 = evaluate(expr2, BinderPack{x = 5, y = 3});
    auto result3 = evaluate(expr3, BinderPack{x = 5});

    std::cout << "   (x + y) with x=5, y=3: " << result1 << "\n";
    std::cout << "   (x * y) with x=5, y=3: " << result2 << "\n";
    std::cout << "   x^2 with x=5: " << result3 << "\n\n";
  }

  // --- Constants ---
  std::cout << "4. Constants\n";
  auto const_expr = 2_c + 3_c;
  auto const_result = evaluate(const_expr, BinderPack{});
  std::cout << "   2 + 3 = " << const_result << "\n\n";

  // --- Special Constants ---
  std::cout << "5. Special Constants\n";
  auto pi_expr = π * 2_c;
  auto e_expr = pow(e, 1_c);
  std::cout << "   π and e are built-in constants\n";
  std::cout << "   2π evaluated: " << evaluate(pi_expr, BinderPack{}) << "\n";
  std::cout << "   e^1 evaluated: " << evaluate(e_expr, BinderPack{}) << "\n\n";

  // --- Simplification ---
  std::cout << "6. Simplification\n";
  {
    auto simple1 = x + 0_c;
    auto simplified1 = simplify(simple1);
    std::cout << "   x + 0 simplifies (to x)\n";

    auto simple2 = x * 1_c;
    auto simplified2 = simplify(simple2);
    std::cout << "   x * 1 simplifies (to x)\n";

    auto simple3 = pow(x, 1_c);
    auto simplified3 = simplify(simple3);
    std::cout << "   x^1 simplifies (to x)\n";

    auto simple4 = log(1_c);
    auto simplified4 = simplify(simple4);
    std::cout << "   log(1) simplifies (to 0)\n";
  }
  std::cout << "\n";

  // --- Pattern Matching ---
  std::cout << "7. Pattern Matching\n";
  {
    auto test_expr = x + y;
    if constexpr (match(test_expr, AnyArg{} + AnyArg{})) {
      std::cout << "   ✓ (x + y) matches pattern (? + ?)\n";
    }

    auto test_expr2 = sin(x);
    if constexpr (match(test_expr2, sin(AnyArg{}))) {
      std::cout << "   ✓ sin(x) matches pattern sin(?)\n";
    }

    if constexpr (!match(test_expr2, cos(AnyArg{}))) {
      std::cout << "   ✓ sin(x) does NOT match pattern cos(?)\n";
    }
  }
  std::cout << "\n";

  // --- Complex Example ---
  std::cout << "8. Complex Expression\n";
  {
    // Build: (x + 1)^2 + 2x
    auto complex = pow(x + 1_c, 2_c) + 2_c * x;
    std::cout << "   expr = (x + 1)^2 + 2x\n";

    auto result = evaluate(complex, BinderPack{x = 5});
    std::cout << "   With x=5: " << result << "\n";
    std::cout << "   (Expected: (5+1)^2 + 2*5 = 36 + 10 = 46)\n";
  }
  std::cout << "\n";

  // --- Mathematical Functions ---
  std::cout << "9. Mathematical Functions\n";
  {
    auto trig = sin(1.0_c);
    auto log_expr = log(e);
    auto exp_expr = exp(0_c);

    std::cout << "   sin(1.0) = " << evaluate(trig, BinderPack{}) << "\n";
    std::cout << "   log(e) simplified and evaluated = "
              << evaluate(simplify(log_expr), BinderPack{}) << "\n";
    std::cout << "   exp(0) = " << evaluate(exp_expr, BinderPack{}) << "\n";
  }
  std::cout << "\n";

  std::cout << "=== All examples complete! ===\n";

  return 0;
}
