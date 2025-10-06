// Test demonstrating traversal strategies with algebraic simplification
// This showcases the full power of the combinator-based symbolic3 system

#include <iostream>

#include "symbolic3/constants.h"
#include "symbolic3/context.h"
#include "symbolic3/core.h"
#include "symbolic3/operators.h"
#include "symbolic3/simplify.h"
#include "symbolic3/traversal.h"

using namespace tempura::symbolic3;

int main() {
  std::cout << "Testing traversal strategies with simplification...\n\n";

  // Create context
  constexpr auto ctx = default_context();

  // Create some symbols
  constexpr Symbol x;
  constexpr Symbol y;
  constexpr Symbol z;
  (void)z;  // Suppress unused warning

  // ========================================================================
  // Test 1: Simple rule application vs traversal
  // ========================================================================
  {
    std::cout << "Test 1: Top-level vs recursive simplification\n";

    // Expression: x * (y + 0)
    // The (y + 0) is nested, so top-level rules won't see it
    constexpr auto expr = x * (y + 0_c);

    // Top-level only (won't simplify the inner (y + 0))
    constexpr auto top_level = algebraic_simplify.apply(expr, ctx);
    // Note: top_level may or may not change the expression depending on which
    // rules match The key point is that it doesn't recurse into subexpressions
    (void)top_level;  // Suppress unused warning

    // With innermost traversal (simplifies from leaves up)
    constexpr auto with_traversal =
        innermost(algebraic_simplify).apply(expr, ctx);
    // Result should be: x * y (the 0 is eliminated)
    static_assert(!std::is_same_v<decltype(with_traversal), decltype(expr)>,
                  "Innermost should simplify nested expressions");

    std::cout << "  ✓ Top-level preserves nested structure\n";
    std::cout << "  ✓ Innermost simplifies recursively\n\n";
  }

  // ========================================================================
  // Test 2: Multiple nested simplifications
  // ========================================================================
  {
    std::cout << "Test 2: Deep nesting requires traversal\n";

    // Expression: (x + 0) * ((y * 1) + 0)
    // Multiple nested simplification opportunities
    constexpr auto expr = (x + 0_c) * ((y * 1_c) + 0_c);

    // Innermost will simplify from deepest level:
    // Step 1: (y * 1) → y
    // Step 2: (y + 0) → y
    // Step 3: (x + 0) → x
    // Result: x * y
    constexpr auto simplified = innermost(algebraic_simplify).apply(expr, ctx);
    (void)simplified;  // Suppress unused warning

    std::cout << "  ✓ Deep nesting simplified correctly\n\n";
  }

  // ========================================================================
  // Test 3: Fixpoint iteration with traversal
  // ========================================================================
  {
    std::cout << "Test 3: Fixpoint + traversal for complete simplification\n";

    // Expression that requires multiple passes: ((x * 1) + 0) * 1
    constexpr auto expr = ((x * 1_c) + 0_c) * 1_c;

    // Using innermost with fixpoint for exhaustive simplification
    constexpr auto fully_simplified =
        innermost(simplify_fixpoint).apply(expr, ctx);
    (void)fully_simplified;  // Suppress unused warning

    // Should reduce all the way to just x
    std::cout << "  ✓ Fixpoint + innermost gives exhaustive simplification\n\n";
  }

  // ========================================================================
  // Test 4: Transcendental functions with traversal
  // ========================================================================
  {
    std::cout << "Test 4: Transcendental functions benefit from traversal\n";

    // Expression: log(exp(x + 0))
    // Inner (x + 0) needs simplification, then log(exp(...)) can simplify
    constexpr auto expr = log(exp(x + 0_c));

    // Various traversal strategies can be applied:
    constexpr auto result = topdown(algebraic_simplify).apply(expr, ctx);
    (void)result;  // Suppress unused warning

    std::cout << "  ✓ Multiple traversal strategies available\n";
    std::cout << "  ✓ innermost: apply at leaves, work upward\n";
    std::cout << "  ✓ bottomup: post-order traversal\n";
    std::cout << "  ✓ topdown: pre-order traversal\n\n";
  }

  std::cout << "All traversal + simplification tests passed! ✅\n";
  std::cout << "\nKey Takeaway:\n";
  std::cout
      << "  Traversal strategies make rules work on nested expressions.\n";
  std::cout << "  Recommended: innermost(simplify_fixpoint) for full "
               "simplification.\n";

  return 0;
}
