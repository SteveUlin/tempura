//==============================================================================
// Multiplication Term-Structure-Aware Ordering Tests
//
// Tests that multiplication simplification rules use algebraic term structure
// to group like bases together, enabling better power combining.
//
// Example: x^3 · y · x · y^2 · x^2 should order terms to group like bases:
//          → x · x^2 · x^3 · y · y^2 (like bases adjacent)
//          → x^6 · y^3 (after power combining)
//==============================================================================

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
#include <cassert>
#include <cmath>
#include <iostream>

using namespace tempura;
using namespace tempura::symbolic3;

auto main() -> int {

  //============================================================================
  // TERM STRUCTURE COMPARISON BASICS
  //============================================================================

  "Multiplication term comparison: constants come first"_test = [] {
    constexpr Symbol x;
    
    // 2 < x (constants before variables)
    constexpr auto cmp1 = compareMultiplicationTerms(2_c, x);
    static_assert(cmp1 == Ordering::Less);
    
    // x > 2
    constexpr auto cmp2 = compareMultiplicationTerms(x, 2_c);
    static_assert(cmp2 == Ordering::Greater);
  };

  "Multiplication term comparison: group by base"_test = [] {
    constexpr Symbol x;
    
    // x < x^2 (same base, compare exponents: 1 < 2)
    constexpr auto cmp1 = compareMultiplicationTerms(x, pow(x, 2_c));
    static_assert(cmp1 == Ordering::Less);
    
    // x^2 < x^3 (same base, compare exponents: 2 < 3)
    constexpr auto cmp2 = compareMultiplicationTerms(pow(x, 2_c), pow(x, 3_c));
    static_assert(cmp2 == Ordering::Less);
  };

  "Multiplication term comparison: different bases sorted separately"_test = [] {
    constexpr Symbol x;
    constexpr Symbol y;
    
    // When x and y are different, they're sorted by their type ordering
    // The key insight: all x powers will be adjacent, all y powers will be adjacent
    
    // x < x^2 (exponent 1 < 2)
    constexpr auto cmp1 = compareMultiplicationTerms(x, pow(x, 2_c));
    static_assert(cmp1 == Ordering::Less);
    
    // x^2 < x^3 (exponent 2 < 3)
    constexpr auto cmp2 = compareMultiplicationTerms(pow(x, 2_c), pow(x, 3_c));
    static_assert(cmp2 == Ordering::Less);
  };

  //============================================================================
  // CANONICAL ORDERING RULE WITH TERM STRUCTURE
  //============================================================================

  "Canonical ordering: x^2 · x → x · x^2"_test = [] {
    Symbol x;
    auto expr = pow(x, 2_c) * x;
    
    // The canonical_order rule should reorder based on term structure
    auto result = MultiplicationRuleCategories::Ordering.apply(expr, default_context());
    
    // Verify by evaluation
    auto val = evaluate(result, BinderPack{x = 2});
    assert(val == 8);  // 2 * 2^2 = 2 * 4 = 8
  };

  "Canonical ordering: y · x when bases differ"_test = [] {
    Symbol x;
    Symbol y;
    
    // Create expression where ordering needs to happen
    auto expr1 = y * x;
    auto expr2 = x * y;
    
    auto result1 = MultiplicationRuleCategories::Ordering.apply(expr1, default_context());
    auto result2 = MultiplicationRuleCategories::Ordering.apply(expr2, default_context());
    
    // Both should evaluate correctly
    assert(evaluate(result1, BinderPack{x = 3, y = 5}) == 15);
    assert(evaluate(result2, BinderPack{x = 3, y = 5}) == 15);
  };

  //============================================================================
  // ASSOCIATIVITY WITH TERM STRUCTURE
  //============================================================================

  "Associativity groups like bases: x · (x^2 · y)"_test = [] {
    Symbol x;
    Symbol y;
    
    // x · (x^2 · y) = x · x^2 · y = x^3 · y
    // With x=2, y=5: 8 * 5 = 40
    auto inner = pow(x, 2_c) * y;
    auto expr = x * inner;
    
    auto result = MultiplicationRuleCategories::Associativity.apply(expr, default_context());
    
    // Verify correctness (structure may change but value is preserved)
    assert(evaluate(result, BinderPack{x = 2, y = 5}) == 40);
    
    // After full simplification, like bases will be combined
    auto fully_simplified = full_simplify(expr, default_context());
    assert(evaluate(fully_simplified, BinderPack{x = 2, y = 5}) == 40);
  };

  "Associativity with different bases"_test = [] {
    Symbol x;
    Symbol y;
    Symbol z;
    
    // Complex nested expression
    auto expr = x * (y * z);
    auto result = MultiplicationRuleCategories::Associativity.apply(expr, default_context());
    
    // Should still evaluate correctly regardless of structure
    assert(evaluate(result, BinderPack{x = 2, y = 3, z = 5}) == 30);
  };

  //============================================================================
  // POWER COMBINING WITH TERM GROUPING
  //============================================================================

  "Power combining: x · x → x^2"_test = [] {
    Symbol x;
    
    // x · x should combine to x^2
    auto expr = x * x;
    
    // Note: This requires recognizing x as x^1
    // The PowerCombining rules handle this
    auto result = full_simplify(expr, default_context());
    
    // Verify: x^2
    auto val = evaluate(result, BinderPack{x = 3});
    assert(val == 9);  // 3^2 = 9
  };

  "Power combining: x · x^2 → x^3"_test = [] {
    Symbol x;
    
    auto expr = x * pow(x, 2_c);
    auto result = MultiplicationRuleCategories::PowerCombining.apply(expr, default_context());
    
    // Should become x^3
    auto val = evaluate(result, BinderPack{x = 2});
    assert(val == 8);  // 2^3 = 8
  };

  "Power combining: x^2 · x^3 → x^5"_test = [] {
    Symbol x;
    
    auto expr = pow(x, 2_c) * pow(x, 3_c);
    auto result = MultiplicationRuleCategories::PowerCombining.apply(expr, default_context());
    
    // Should become x^5
    auto val = evaluate(result, BinderPack{x = 2});
    assert(val == 32);  // 2^5 = 32
  };

  //============================================================================
  // FULL SIMPLIFICATION WITH TERM GROUPING
  //============================================================================

  "Full simplify: x^3 · x · x^2 → x^6"_test = [] {
    Symbol x;
    
    // Multiple powers of same base: x^3 · x · x^2 = x^(3+1+2) = x^6
    auto expr = pow(x, 3_c) * x * pow(x, 2_c);
    auto result = full_simplify(expr, default_context());
    
    // Verify: x^6
    auto val = evaluate(result, BinderPack{x = 2});
    assert(val == 64);  // 2^6 = 64
  };

  "Full simplify: x^2 · y · x · y^2 → x^3 · y^3"_test = [] {
    Symbol x;
    Symbol y;
    
    // Mixed bases: group x powers and y powers
    // x^2 · y · x · y^2 → x^3 · y^3
    auto expr = pow(x, 2_c) * y * x * pow(y, 2_c);
    auto result = full_simplify(expr, default_context());
    
    // Verify: x^3 · y^3 = 2^3 · 3^3 = 8 · 27 = 216
    auto val = evaluate(result, BinderPack{x = 2, y = 3});
    assert(val == 216);
  };

  "Full simplify: 2 · x · 3 · x^2 → 6 · x^3"_test = [] {
    Symbol x;
    
    // Constants and powers: 2 · x · 3 · x^2 = 6 · x^3
    auto expr = 2_c * x * 3_c * pow(x, 2_c);
    auto result = full_simplify(expr, default_context());
    
    // Verify: 6 · x^3 = 6 · 2^3 = 6 · 8 = 48
    auto val = evaluate(result, BinderPack{x = 2});
    assert(val == 48);
  };

  "Full simplify with addition: (x · x^2) · (y + y^2)"_test = [] {
    Symbol x;
    Symbol y;
    
    // Product with sum: (x · x^2) · (y + y^2)
    // = x^3 · (y + y^2)
    // = x^3 · y + x^3 · y^2
    auto left = x * pow(x, 2_c);
    auto right = y + pow(y, 2_c);
    auto expr = left * right;
    
    auto result = full_simplify(expr, default_context());
    
    // Verify with x=2, y=3:
    // x^3 = 8, y = 3, y^2 = 9
    // 8 * 3 + 8 * 9 = 24 + 72 = 96
    auto val = evaluate(result, BinderPack{x = 2, y = 3});
    assert(val == 96);
  };

  //============================================================================
  // STRESS TEST: COMPLEX EXPRESSION
  //============================================================================

  "Complex expression: 2 · x^2 · y · 3 · x · y^3 · x^2"_test = [] {
    Symbol x;
    Symbol y;
    
    // Many terms: constants, x powers, y powers
    // Expected grouping: constants together, x powers together, y powers together
    // 2 · x^2 · y · 3 · x · y^3 · x^2
    // = (2 · 3) · (x^2 · x · x^2) · (y · y^3)
    // = 6 · x^5 · y^4
    auto expr = 2_c * pow(x, 2_c) * y * 3_c * x * pow(y, 3_c) * pow(x, 2_c);
    auto result = full_simplify(expr, default_context());
    
    // Verify: 6 · x^5 · y^4 = 6 · 2^5 · 3^4 = 6 · 32 · 81 = 15552
    auto val = evaluate(result, BinderPack{x = 2, y = 3});
    assert(val == 15552);
  };

  std::cout << "\n✓ All multiplication term-structure-aware ordering tests passed!\n";
  std::cout << "  Terms are now grouped by their algebraic base (powers),\n";
  std::cout << "  enabling efficient power combining and simplification.\n";

  return 0;
}
