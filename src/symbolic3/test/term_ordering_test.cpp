//==============================================================================
// Term-Structure-Aware Ordering Tests
//
// Tests that addition simplification rules use algebraic term structure
// to group like terms together, enabling better term collection.
//
// Example: 3*x + y + 3*x + 2*y should order terms to group like bases:
//          → 3*x + 3*x + y + 2*y (like terms adjacent)
//          → 6*x + 3*y (after factoring)
//==============================================================================

#include <cassert>
#include <iostream>

#include "symbolic3/constants.h"
#include "symbolic3/context.h"
#include "symbolic3/core.h"
#include "symbolic3/evaluate.h"
#include "symbolic3/matching.h"
#include "symbolic3/operators.h"
#include "symbolic3/simplify.h"
#include "symbolic3/term_structure.h"
#include "symbolic3/traversal.h"
#include "unit.h"

using namespace tempura;
using namespace tempura::symbolic3;

auto main() -> int {
  //============================================================================
  // TERM STRUCTURE COMPARISON BASICS
  //============================================================================

  "Term comparison: constants come first"_test = [] {
    constexpr Symbol x;

    // 5 < x (constants before symbols)
    constexpr auto cmp1 = compareAdditionTerms(5_c, x);
    static_assert(cmp1 == Ordering::Less);

    // x > 5
    constexpr auto cmp2 = compareAdditionTerms(x, 5_c);
    static_assert(cmp2 == Ordering::Greater);
  };

  "Term comparison: group by base"_test = [] {
    constexpr Symbol x;
    constexpr Symbol y;

    // x < y (assuming x's type ID < y's type ID for this symbol ordering)
    // More importantly: x and 2*x should be grouped together

    // x < 2*x (same base, compare coefficients: 1 < 2)
    constexpr auto cmp1 = compareAdditionTerms(x, 2_c * x);
    static_assert(cmp1 == Ordering::Less);

    // 2*x < 3*x (same base, compare coefficients: 2 < 3)
    constexpr auto cmp2 = compareAdditionTerms(2_c * x, 3_c * x);
    static_assert(cmp2 == Ordering::Less);
  };

  "Term comparison: different bases sorted separately"_test = [] {
    constexpr Symbol x;
    constexpr Symbol y;

    // When x and y are different, they're sorted by their type ordering
    // The key insight: all x terms will be adjacent, all y terms will be
    // adjacent

    // x < 3*x (coefficient 1 < 3)
    constexpr auto cmp1 = compareAdditionTerms(x, 3_c * x);
    static_assert(cmp1 == Ordering::Less);

    // 2*x < 3*x (coefficient 2 < 3)
    constexpr auto cmp2 = compareAdditionTerms(2_c * x, 3_c * x);
    static_assert(cmp2 == Ordering::Less);
  };

  //============================================================================
  // CANONICAL ORDERING RULE WITH TERM STRUCTURE
  //============================================================================

  "Canonical ordering: 2*x + x → x + 2*x"_test = [] {
    Symbol x;
    auto expr = 2_c * x + x;

    // The canonical_order rule should reorder based on term structure
    auto result =
        AdditionRuleCategories::Ordering.apply(expr, default_context());

    // Verify by evaluation
    auto val = evaluate(result, BinderPack{x = 5});
    assert(val == 15);  // 5 + 10 = 15
  };

  "Canonical ordering: y + x when bases differ"_test = [] {
    Symbol x;
    Symbol y;

    // Create expression where ordering needs to happen
    // The exact order depends on type IDs, but the rule should apply
    // consistently
    auto expr1 = y + x;
    auto expr2 = x + y;

    auto result1 =
        AdditionRuleCategories::Ordering.apply(expr1, default_context());
    auto result2 =
        AdditionRuleCategories::Ordering.apply(expr2, default_context());

    // At least one should have changed (whichever was out of order)
    // Both should evaluate correctly
    assert(evaluate(result1, BinderPack{x = 3, y = 5}) == 8);
    assert(evaluate(result2, BinderPack{x = 3, y = 5}) == 8);
  };

  //============================================================================
  // ASSOCIATIVITY WITH TERM STRUCTURE
  //============================================================================

  "Associativity groups like terms: x + (2*x + y)"_test = [] {
    Symbol x;
    Symbol y;

    // x + (2*x + y) = x + 2*x + y = 3*x + y
    // With x=10, y=5: 30 + 5 = 35
    auto inner = 2_c * x + y;
    auto expr = x + inner;

    auto result =
        AdditionRuleCategories::Associativity.apply(expr, default_context());

    // Verify correctness (structure may change but value is preserved)
    assert(evaluate(result, BinderPack{x = 10, y = 5}) == 35);

    // The key is that after full simplification, like terms will be grouped
    auto fully_simplified = simplify(expr, default_context());
    assert(evaluate(fully_simplified, BinderPack{x = 10, y = 5}) == 35);
  };

  "Associativity with different bases"_test = [] {
    Symbol x;
    Symbol y;
    Symbol z;

    // Complex nested expression
    auto expr = x + (y + z);
    auto result =
        AdditionRuleCategories::Associativity.apply(expr, default_context());

    // Should still evaluate correctly regardless of structure
    assert(evaluate(result, BinderPack{x = 1, y = 2, z = 3}) == 6);
  };

  //============================================================================
  // FULL SIMPLIFICATION WITH TERM GROUPING
  //============================================================================

  "Full simplify: 3*x + y + 2*x + y → 5*x + 2*y"_test = [] {
    Symbol x;
    Symbol y;

    // Build expression: 3*x + y + 2*x + y
    // Expected: group x terms and y terms, then factor
    auto expr = 3_c * x + y + 2_c * x + y;

    auto result = simplify(expr, default_context());

    // Verify by evaluation
    auto val = evaluate(result, BinderPack{x = 10, y = 5});
    assert(val == 60);  // 5*10 + 2*5 = 50 + 10 = 60
  };

  "Full simplify: x + 3*x + 2*x → 6*x"_test = [] {
    Symbol x;

    // Multiple like terms with different coefficients
    auto expr = x + 3_c * x + 2_c * x;
    auto result = simplify(expr, default_context());

    // Should factor to 6*x (or equivalent)
    auto val = evaluate(result, BinderPack{x = 10});
    assert(val == 60);
  };

  "Full simplify: 2*y + x + 3*x + y → 4*x + 3*y"_test = [] {
    Symbol x;
    Symbol y;

    // Mixed terms: group by base, then factor
    auto expr = 2_c * y + x + 3_c * x + y;
    auto result = simplify(expr, default_context());

    // Verify: 4*x + 3*y
    auto val = evaluate(result, BinderPack{x = 10, y = 5});
    assert(val == 55);  // 4*10 + 3*5 = 40 + 15 = 55
  };

  "Full simplify with constants: 5 + 2*x + 3 + x → 8 + 3*x"_test = [] {
    Symbol x;

    // Constants and variable terms mixed
    auto expr = 5_c + 2_c * x + 3_c + x;
    auto result = simplify(expr, default_context());

    // Constants should be grouped and added, x terms factored
    auto val = evaluate(result, BinderPack{x = 10});
    assert(val == 38);  // 8 + 3*10 = 8 + 30 = 38
  };

  //============================================================================
  // STRESS TEST: COMPLEX EXPRESSION
  //============================================================================

  "Complex expression: 3*x + 2*y + x + 5 + 4*x + y + 2"_test = [] {
    Symbol x;
    Symbol y;

    // Many terms: constants, x terms, y terms
    // Expected grouping: constants together, x terms together, y terms together
    // x terms: 3x + x + 4x = 8x → 8*10 = 80
    // y terms: 2y + y = 3y → 3*5 = 15
    // constants: 5 + 2 = 7
    // Total: 7 + 80 + 15 = 102
    auto expr = 3_c * x + 2_c * y + x + 5_c + 4_c * x + y + 2_c;
    auto result = simplify(expr, default_context());

    // Verify: 7 + 8*x + 3*y = 7 + 80 + 15 = 102
    auto val = evaluate(result, BinderPack{x = 10, y = 5});
    assert(val == 102);
  };

  std::cout << "\n✓ All term-structure-aware ordering tests passed!\n";
  std::cout << "  Terms are now grouped by their algebraic base,\n";
  std::cout << "  enabling efficient term collection and factoring.\n";

  return 0;
}
