#include <iostream>

#include "symbolic3/evaluate.h"
#include "symbolic3/simplify.h"

using namespace tempura::symbolic3;

auto main() -> int {
  std::cout << "\n=== Term Collecting Debug ===\n\n";

  // Test 1: Simple like terms
  {
    Symbol x;
    auto expr = x + x;
    std::cout << "Expression: x + x\n";
    std::cout << "Type: " << typeid(expr).name() << "\n";

    auto result = full_simplify(expr, default_context());
    std::cout << "After simplification type: " << typeid(result).name() << "\n";

    auto val = evaluate(result, BinderPack{x = 5});
    std::cout << "Evaluates to: " << val << " (expected: 10)\n";
    std::cout << std::endl;
  }

  // Test 2: Factor simple
  {
    Symbol x;
    auto expr = x * 2_c + x;
    std::cout << "Expression: x*2 + x\n";
    std::cout << "Type: " << typeid(expr).name() << "\n";

    auto result = full_simplify(expr, default_context());
    std::cout << "After simplification type: " << typeid(result).name() << "\n";

    auto val = evaluate(result, BinderPack{x = 10});
    std::cout << "Evaluates to: " << val << " (expected: 30)\n";
    std::cout << std::endl;
  }

  // Test 3: Canonical ordering
  {
    Symbol x;
    Symbol y;

    auto expr1 = y + x;
    auto result1 = full_simplify(expr1, default_context());
    std::cout << "Expression: y + x\n";
    std::cout << "Type: " << typeid(result1).name() << "\n";

    auto expr2 = x + y;
    auto result2 = full_simplify(expr2, default_context());
    std::cout << "Expression: x + y\n";
    std::cout << "Type: " << typeid(result2).name() << "\n";
    std::cout << "Types are same: "
              << (typeid(result1) == typeid(result2) ? "YES" : "NO") << "\n";
    std::cout << "\n";
  }

  // Test 4: Many terms with alternating bases (linear chain)
  {
    Symbol x;
    Symbol y;
    Symbol z;

    // x + y + x + z + y + x = 3x + 2y + z
    auto expr = x + y + x + z + y + x;
    std::cout << "Expression: x + y + x + z + y + x\n";
    std::cout << "Type: " << typeid(expr).name() << "\n";

    auto result = full_simplify(expr, default_context());
    std::cout << "After simplification type: " << typeid(result).name() << "\n";

    // Check structure - should have collected all x's together
    bool has_x_collected =
        match(result, x * 𝐜 + 𝐚𝐧𝐲) || match(result, 𝐚𝐧𝐲 + x * 𝐜) ||
        match(result, 𝐜 * x + 𝐚𝐧𝐲) || match(result, 𝐚𝐧𝐲 + 𝐜 * x);
    std::cout << "Has x terms collected: "
              << (has_x_collected ? "YES" : "NO - FAILED!") << "\n";

    auto val = evaluate(result, BinderPack{x = 10, y = 5, z = 3});
    std::cout << "Evaluates to: " << val << " (expected: 43)\n";
    std::cout << "\n";
  }

  // Test 5: Many coefficient terms with alternating bases
  {
    Symbol x;
    Symbol y;

    // 2x + 3y + 4x + 5y + 6x = 12x + 8y (should collect to just 2 terms!)
    auto expr = x * 2_c + y * 3_c + x * 4_c + y * 5_c + x * 6_c;
    std::cout << "Expression: 2x + 3y + 4x + 5y + 6x (5 terms)\n";
    std::cout << "Type: " << typeid(expr).name() << "\n";

    auto result = full_simplify(expr, default_context());
    std::cout << "After simplification type: " << typeid(result).name() << "\n";

    // Should be JUST TWO TERMS: 12x + 8y (or 8y + 12x)
    bool is_two_terms =
        match(result, 𝐚𝐧𝐲 + 𝐚𝐧𝐲) && !match(result, (𝐚𝐧𝐲 + 𝐚𝐧𝐲) + 𝐚𝐧𝐲);
    std::cout << "Is exactly 2 terms (not 5): "
              << (is_two_terms ? "YES"
                               : "NO - FAILED! Still has multiple terms")
              << "\n";

    auto val = evaluate(result, BinderPack{x = 10, y = 100});
    std::cout << "Evaluates to: " << val << " (expected: 920)\n";
    std::cout << "\n";
  }

  // Test 6: Non-linear tree structure (nested parentheses)
  {
    Symbol x;
    Symbol y;
    Symbol z;

    // ((x + y) + (z + x)) + ((y + z) + x) = 3x + 2y + 2z (should be 3 terms!)
    auto expr = ((x + y) + (z + x)) + ((y + z) + x);
    std::cout
        << "Expression: ((x + y) + (z + x)) + ((y + z) + x) [7 symbols]\n";
    std::cout << "Type: " << typeid(expr).name() << "\n";

    auto result = full_simplify(expr, default_context());
    std::cout << "After simplification type: " << typeid(result).name() << "\n";

    // Should be 3 terms: 3x + 2y + 2z (with coefficients)
    bool has_x_coefficient =
        match(result, x * 𝐜 + 𝐚𝐧𝐲) || match(result, 𝐚𝐧𝐲 + x * 𝐜);
    std::cout << "Has x with coefficient: "
              << (has_x_coefficient ? "YES" : "NO - x terms not collected!")
              << "\n";

    auto val = evaluate(result, BinderPack{x = 10, y = 5, z = 2});
    std::cout << "Evaluates to: " << val << " (expected: 44)\n";
    std::cout << "\n";
  }

  // Test 7: Deep nested tree with mixed operations
  {
    Symbol x;
    Symbol y;

    // (x*2 + y*3) + ((x*4 + y) + (x + y*2))
    // = 2x + 3y + 4x + y + x + 2y
    // = 7x + 6y
    auto expr = (x * 2_c + y * 3_c) + ((x * 4_c + y) + (x + y * 2_c));
    std::cout << "Expression: (x*2 + y*3) + ((x*4 + y) + (x + y*2))\n";
    std::cout << "Type: " << typeid(expr).name() << "\n";

    auto result = full_simplify(expr, default_context());
    std::cout << "After simplification type: " << typeid(result).name() << "\n";

    auto val = evaluate(result, BinderPack{x = 10, y = 100});
    std::cout << "Evaluates to: " << val << " (expected: 7*10 + 6*100 = 670)\n";
    std::cout << "\n";
  }

  // Test 8: Multiplication tree - power combining across tree
  {
    Symbol x;

    // (x^2 * x^3) * (x * x^4) = x^10
    auto expr = (pow(x, 2_c) * pow(x, 3_c)) * (x * pow(x, 4_c));
    std::cout << "Expression: (x^2 * x^3) * (x * x^4)\n";
    std::cout << "Type: " << typeid(expr).name() << "\n";

    auto result = full_simplify(expr, default_context());
    std::cout << "After simplification type: " << typeid(result).name() << "\n";

    auto val = evaluate(result, BinderPack{x = 2});
    std::cout << "Evaluates to: " << val << " (expected: 2^10 = 1024)\n";
    std::cout << "\n";
  }

  // Test 9: Mixed tree with both addition and multiplication
  {
    Symbol x;
    Symbol y;

    // (x + x*2) + (y*3 + y) + (x*4 + y*5)
    // = x + 2x + 3y + y + 4x + 5y
    // = 7x + 9y
    auto expr = (x + x * 2_c) + (y * 3_c + y) + (x * 4_c + y * 5_c);
    std::cout << "Expression: (x + x*2) + (y*3 + y) + (x*4 + y*5)\n";
    std::cout << "Type: " << typeid(expr).name() << "\n";

    auto result = full_simplify(expr, default_context());
    std::cout << "After simplification type: " << typeid(result).name() << "\n";

    auto val = evaluate(result, BinderPack{x = 10, y = 100});
    std::cout << "Evaluates to: " << val << " (expected: 7*10 + 9*100 = 970)\n";
    std::cout << "\n";
  }

  // Test 10: Unbalanced tree (right-heavy)
  {
    Symbol x;

    // x + (x + (x + (x + (x + x))))
    // = 6x
    auto expr = x + (x + (x + (x + (x + x))));
    std::cout << "Expression: x + (x + (x + (x + (x + x))))\n";
    std::cout << "Type: " << typeid(expr).name() << "\n";

    auto result = full_simplify(expr, default_context());
    std::cout << "After simplification type: " << typeid(result).name() << "\n";

    auto val = evaluate(result, BinderPack{x = 7});
    std::cout << "Evaluates to: " << val << " (expected: 6*7 = 42)\n";
    std::cout << "\n";
  }

  std::cout << "All tests completed (check output manually)\n";
  return 0;
}
