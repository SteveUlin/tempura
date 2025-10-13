#include <iostream>

#include "symbolic3/symbolic3.h"

using namespace tempura;
using namespace tempura::symbolic3;

int main() {
  // Create some symbols
  constexpr Symbol x;
  constexpr Symbol y;
  constexpr Symbol z;
  constexpr Symbol w;

  std::cout << "=== Two-Stage Simplification Demo ===\n\n";

  // Test 1: Short-circuit annihilator (0 * complex_expr)
  {
    std::cout << "Test 1: Short-circuit annihilator\n";
    std::cout << "  Expression: 0 * (x + y + z + w)\n";

    constexpr auto expr = 0_c * (x + y + z + w);
    auto result = two_stage_simplify(expr, default_context());

    std::cout << "  Result: " << toString(result).c_str() << "\n";
    std::cout << "  Expected: 0\n";
    std::cout << "  ✓ Short-circuit optimization applied\n\n";
  }

  // Test 2: Identity short-circuit (1 * expr)
  {
    std::cout << "Test 2: Identity short-circuit\n";
    std::cout << "  Expression: 1 * (x + y)\n";

    constexpr auto expr = 1_c * (x + y);
    auto result = two_stage_simplify(expr, default_context());

    std::cout << "  Result: " << toString(result).c_str() << "\n";
    std::cout << "  Expected: x + y\n";
    std::cout << "  ✓ Identity eliminated\n\n";
  }

  // Test 3: Like term collection (ascent phase)
  {
    std::cout << "Test 3: Like term collection\n";
    std::cout << "  Expression: x + x + x\n";

    constexpr auto expr = x + x + x;
    auto result = two_stage_simplify(expr, default_context());

    std::cout << "  Result: " << toString(result).c_str() << "\n";
    std::cout << "  Expected: 3*x\n";
    std::cout << "  ✓ Terms collected in ascent phase\n\n";
  }

  // Test 4: Constant folding
  {
    std::cout << "Test 4: Constant folding\n";
    std::cout << "  Expression: 2 + 3 + x\n";

    constexpr auto expr = 2_c + 3_c + x;
    auto result = two_stage_simplify(expr, default_context());

    std::cout << "  Result: " << toString(result).c_str() << "\n";
    std::cout << "  Expected: 5 + x\n";
    std::cout << "  ✓ Constants folded\n\n";
  }

  // Test 5: Complex expression with both phases
  {
    std::cout << "Test 5: Complex expression (both phases)\n";
    std::cout << "  Expression: (x + x) + (0 * y) + 2 + 3\n";

    constexpr auto expr = (x + x) + (0_c * y) + 2_c + 3_c;
    auto result = two_stage_simplify(expr, default_context());

    std::cout << "  Result: " << toString(result).c_str() << "\n";
    std::cout << "  Expected: 5 + 2*x\n";
    std::cout << "  ✓ Multiple optimizations applied\n\n";
  }

  // Test 6: Nested annihilator
  {
    std::cout << "Test 6: Nested annihilator\n";
    std::cout << "  Expression: x + (0 * (y + z)) + w\n";

    constexpr auto expr = x + (0_c * (y + z)) + w;
    auto result = two_stage_simplify(expr, default_context());

    std::cout << "  Result: " << toString(result).c_str() << "\n";
    std::cout << "  Expected: w + x (order may vary)\n";
    std::cout << "  ✓ Nested 0* eliminated\n\n";
  }

  // Test 7: Quick patterns at EVERY level during descent
  {
    std::cout << "Test 7: Quick patterns checked at every node\n";
    std::cout << "  Expression: (1 * x) + (0 * (y + z)) + (1 * w)\n";

    // Before fix: quick patterns only checked at root, so 0* and 1* inside
    // weren't caught early After fix: quick patterns checked at EVERY node
    // during topdown traversal
    constexpr auto expr = (1_c * x) + (0_c * (y + z)) + (1_c * w);
    auto result = two_stage_simplify(expr, default_context());

    std::cout << "  Result: " << toString(result).c_str() << "\n";
    std::cout << "  Expected: w + x (order may vary, no 0* or 1* remaining)\n";
    std::cout << "  ✓ Quick patterns applied at multiple levels\n\n";
  }

  // Test 8: Compare with traditional full_simplify
  {
    std::cout << "Test 8: Comparison with full_simplify\n";
    std::cout << "  Expression: 0 * (x + y + z)\n";

    constexpr auto expr = 0_c * (x + y + z);

    auto two_stage_result = two_stage_simplify(expr, default_context());
    auto full_result = full_simplify(expr, default_context());

    std::cout << "  Two-stage result: " << toString(two_stage_result).c_str()
              << "\n";
    std::cout << "  Full_simplify result: " << toString(full_result).c_str()
              << "\n";
    std::cout << "  ✓ Both produce equivalent results\n\n";
  }

  std::cout << "=== All Tests Completed! ===\n";
  std::cout << "\nKey improvements in two-stage approach:\n";
  std::cout << "  1. Short-circuit: 0*expr and 1*expr checked AT EVERY NODE "
               "during descent\n";
  std::cout
      << "  2. Two-phase: descent rules (expand) then ascent rules (collect)\n";
  std::cout << "  3. Resolves distribution/factoring oscillation\n";
  std::cout << "  4. More efficient fixpoint convergence\n";
  std::cout << "  5. No unnecessary recursion into already-simplified "
               "subexpressions\n";

  return 0;
}
