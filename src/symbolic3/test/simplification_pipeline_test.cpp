//==============================================================================
// Consolidated Simplification Pipeline Tests
//
// This file consolidates:
// - full_simplify_test.cpp (comprehensive simplification pipelines)
// - traversal_simplify_test.cpp (traversal strategies with simplification)
// - term_collecting_test.cpp (term collection and factoring)
// - nested_simplify_test.cpp (nested expression handling)
//
// Tests the high-level simplification pipelines and strategies that combine
// basic rules with traversal algorithms for comprehensive simplification.
//==============================================================================

#include "symbolic3/constants.h"
#include "symbolic3/context.h"
#include "symbolic3/core.h"
#include "symbolic3/matching.h"
#include "symbolic3/operators.h"
#include "symbolic3/simplify.h"
#include "symbolic3/traversal.h"
#include "unit.h"

using namespace tempura;
using namespace tempura::symbolic3;

auto main() -> int {
  //============================================================================
  // COMPREHENSIVE SIMPLIFICATION PIPELINES
  //============================================================================

  "Full simplify - exhaustive nested simplification"_test = [] {
    constexpr Symbol x;
    constexpr Symbol y;
    constexpr Symbol z;
    constexpr auto ctx = default_context();

    // Nested expression: x * (y + (z * 0)) → x * y
    constexpr auto expr = x * (y + (z * 0_c));
    constexpr auto result = full_simplify(expr, ctx);

    // NOTE: If this fails, it indicates a bug in the simplification library
    static_assert(match(result, x * y) || match(result, y * x),
                  "BUG: x * (y + (z * 0)) should simplify to x * y");
  };

  "Algebraic simplify recursive - fast recursive"_test = [] {
    constexpr Symbol x;
    constexpr auto ctx = default_context();

    // (x + 0) * 1 + 0 → x
    constexpr auto expr = (x + 0_c) * 1_c + 0_c;
    constexpr auto result = algebraic_simplify_recursive(expr, ctx);

    // NOTE: If this fails, it indicates a bug in the simplification library
    static_assert(match(result, x),
                  "BUG: (x + 0) * 1 + 0 should simplify to x");
  };

  "Bottomup simplify - post-order traversal"_test = [] {
    constexpr Symbol x;
    constexpr Symbol y;
    constexpr auto ctx = default_context();

    // (x * 1) + (y * 0) → x + 0 → x
    constexpr auto expr = (x * 1_c) + (y * 0_c);
    constexpr auto result = bottomup_simplify(expr, ctx);

    // NOTE: If this fails, it indicates a bug in the simplification library
    static_assert(match(result, x),
                  "BUG: (x * 1) + (y * 0) should simplify to x");
  };

  "Topdown simplify - pre-order traversal"_test = [] {
    constexpr Symbol x;
    constexpr auto ctx = default_context();

    // log(exp(x)) → x
    constexpr auto expr = log(exp(x));
    constexpr auto result = topdown_simplify(expr, ctx);

    // NOTE: If this fails, it indicates a bug in the simplification library
    static_assert(match(result, x), "BUG: log(exp(x)) should simplify to x");
  };

  "Trig aware simplify - trigonometric expressions"_test = [] {
    constexpr Symbol x;
    constexpr auto ctx = default_context();

    // sin(0) + cos(0) * x → 0 + 1 * x → x
    constexpr auto expr = sin(0_c) + cos(0_c) * x;
    constexpr auto result = trig_aware_simplify(expr, ctx);

    // NOTE: If this fails, it indicates a bug in the simplification library
    static_assert(match(result, x),
                  "BUG: sin(0) + cos(0) * x should simplify to x");
  };

  //============================================================================
  // TRAVERSAL STRATEGIES WITH SIMPLIFICATION
  //============================================================================

  "Simple rule vs traversal comparison"_test = [] {
    constexpr Symbol x;
    constexpr Symbol y;
    constexpr auto ctx = default_context();

    // Expression: x * (y + 0)
    // The (y + 0) is nested, so top-level rules won't see it
    constexpr auto expr = x * (y + 0_c);

    // Top-level only (won't simplify the inner (y + 0))
    constexpr auto top_level = algebraic_simplify.apply(expr, ctx);

    // With innermost traversal (simplifies from leaves up)
    constexpr auto with_traversal =
        innermost(algebraic_simplify).apply(expr, ctx);

    static_assert(!std::is_same_v<decltype(with_traversal), decltype(expr)>,
                  "Innermost should simplify nested expressions");
  };

  "Deep nesting requires traversal"_test = [] {
    constexpr Symbol x;
    constexpr Symbol y;
    constexpr auto ctx = default_context();

    // Expression: (x + 0) * ((y * 1) + 0)
    // Multiple nested simplification opportunities
    constexpr auto expr = (x + 0_c) * ((y * 1_c) + 0_c);

    // Innermost will simplify from deepest level:
    // Step 1: (y * 1) → y
    // Step 2: (y + 0) → y
    // Step 3: (x + 0) → x
    // Result: x * y
    constexpr auto simplified = innermost(algebraic_simplify).apply(expr, ctx);
    (void)simplified;  // TODO: Add full verification when rules are stable
  };

  "Fixpoint iteration with traversal"_test = [] {
    constexpr Symbol x;
    constexpr auto ctx = default_context();

    // Expression that requires multiple passes: ((x * 1) + 0) * 1
    constexpr auto expr = ((x * 1_c) + 0_c) * 1_c;

    // Using innermost with fixpoint for exhaustive simplification
    constexpr auto fully_simplified =
        innermost(simplify_fixpoint).apply(expr, ctx);
    (void)fully_simplified;  // TODO: Add verification when simplification
                             // stabilizes
  };

  "Transcendental functions with traversal"_test = [] {
    constexpr Symbol x;
    constexpr auto ctx = default_context();

    // Expression: log(exp(x + 0))
    // Inner (x + 0) needs simplification, then log(exp(...)) can simplify
    constexpr auto expr = log(exp(x + 0_c));

    // Various traversal strategies can be applied:
    constexpr auto result = topdown(algebraic_simplify).apply(expr, ctx);
    (void)result;  // TODO: Add verification
  };

  //============================================================================
  // TERM COLLECTION AND FACTORING
  //============================================================================

  "Like terms collection: x + x"_test = [] {
    constexpr Symbol x;
    constexpr auto ctx = default_context();
    constexpr auto expr = x + x;
    constexpr auto result = full_simplify(expr, ctx);

    // Check structure: should be x * 2 (factored form)
    // The result should be either x * 2 or 2 * x depending on ordering
    // NOTE: If this fails, it indicates a bug in the simplification library
    // that needs to be fixed. The LikeTerms rule should convert x + x → x * 2
    static_assert(
        match(result, x * Constant<2>{}) || match(result, Constant<2>{} * x),
        "BUG: x + x should simplify to 2*x or x*2. "
        "The LikeTerms rule may not be firing correctly.");
  };

  "Factor simple: x*2 + x"_test = [] {
    constexpr Symbol x;
    constexpr auto ctx = default_context();
    constexpr auto expr = x * 2_c + x;
    constexpr auto result = full_simplify(expr, ctx);

    // Should simplify to x * 3 (factoring out x)
    // The result should be either x * 3 or 3 * x
    // NOTE: If this fails, it indicates a bug in the simplification library
    static_assert(
        match(result, x * Constant<3>{}) || match(result, Constant<3>{} * x),
        "BUG: x*2 + x should simplify to 3*x or x*3. "
        "The Factoring rules may not be firing correctly.");
  };

  "Factor both sides: x*2 + x*3"_test = [] {
    constexpr Symbol x;
    constexpr auto ctx = default_context();
    constexpr auto expr = x * 2_c + x * 3_c;
    constexpr auto result = full_simplify(expr, ctx);

    // Should simplify to x * 5 (collecting coefficients)
    // NOTE: If this fails, it indicates a bug in the simplification library
    static_assert(
        match(result, x * Constant<5>{}) || match(result, Constant<5>{} * x),
        "BUG: x*2 + x*3 should simplify to 5*x or x*5. "
        "The Factoring rules may not be firing correctly.");
  };

  "Factor reversed: x + x*2"_test = [] {
    constexpr Symbol x;
    constexpr auto ctx = default_context();
    constexpr auto expr = x + x * 2_c;
    constexpr auto result = full_simplify(expr, ctx);

    // Should simplify to x * 3 (factoring regardless of order)
    // NOTE: If this fails, it indicates a bug in the simplification library
    static_assert(
        match(result, x * Constant<3>{}) || match(result, Constant<3>{} * x),
        "BUG: x + x*2 should simplify to 3*x or x*3. "
        "The Factoring rules may not be firing correctly.");
  };

  "Complex factoring: x*2 + x*3 + x*4"_test = [] {
    constexpr Symbol x;
    constexpr auto ctx = default_context();
    constexpr auto expr = x * 2_c + x * 3_c + x * 4_c;
    constexpr auto result = full_simplify(expr, ctx);

    // Should simplify to x * 9 (collecting all coefficients)
    // NOTE: If this fails, it indicates a bug in the simplification library
    static_assert(
        match(result, x * Constant<9>{}) || match(result, Constant<9>{} * x),
        "BUG: x*2 + x*3 + x*4 should simplify to 9*x or x*9. "
        "The Factoring rules may not be firing correctly.");
  };

  //============================================================================
  // NESTED EXPRESSION HANDLING
  //============================================================================

  "Nested expression simplification: (x + x) + y"_test = [] {
    constexpr Symbol x;
    constexpr Symbol y;
    constexpr auto ctx = default_context();

    // Create manually nested expression: (x + x) + y
    constexpr auto inner = x + x;
    constexpr auto outer = inner + y;

    // Apply AdditionRules to inner: x + x → 2*x
    constexpr auto inner_result = AdditionRules.apply(inner, ctx);
    static_assert(match(inner_result, x * Constant<2>{}) ||
                      match(inner_result, Constant<2>{} * x),
                  "x + x should simplify to 2*x");

    // Apply innermost to outer: should simplify nested terms first
    constexpr auto outer_result = innermost(AdditionRules).apply(outer, ctx);
    // Result should contain simplified inner expression
    // Exact form depends on simplification pipeline

    // Apply full_simplify to outer: should fully simplify
    constexpr auto full_result = full_simplify(outer, ctx);
    // Result should be 2*x + y or y + 2*x (various forms possible)
  };

  //============================================================================
  // POWER RULE COMBINATIONS
  //============================================================================

  "Power rules combination: x^1 * x^2"_test = [] {
    constexpr Symbol x;
    constexpr auto ctx = default_context();

    // x^1 * x^2 should simplify through:
    // Step 1: x^1 → x (power_one rule)
    // Step 2: x * x^2 → x^(1+2) = x^3 (power combining)
    constexpr auto expr = pow(x, 1_c) * pow(x, 2_c);
    constexpr auto result = full_simplify(expr, ctx);

    // Result should be x^3
    // NOTE: If this fails, it indicates a bug in the simplification library
    static_assert(match(result, pow(x, Constant<3>{})),
                  "BUG: x^1 * x^2 should simplify to x^3. "
                  "The PowerCombining rules may not be firing correctly.");
  };

  //============================================================================
  // COMPLEX NESTED EXPRESSIONS
  //============================================================================

  "Complex nesting: ((x + 0) * 1) + ((y * 0) + z)"_test = [] {
    constexpr Symbol x;
    constexpr Symbol y;
    constexpr Symbol z;
    constexpr auto ctx = default_context();

    // ((x + 0) * 1) + ((y * 0) + z) should simplify to x + z
    // Step by step:
    // - (x + 0) → x
    // - (y * 0) → 0
    // - (x * 1) → x
    // - (0 + z) → z
    // - x + z (final)
    constexpr auto expr = ((x + 0_c) * 1_c) + ((y * 0_c) + z);
    constexpr auto result = full_simplify(expr, ctx);

    // Result should be x + z or z + x (order may vary)
    // NOTE: If this fails, it indicates a bug in the simplification library
    static_assert(
        match(result, x + z) || match(result, z + x),
        "BUG: ((x + 0) * 1) + ((y * 0) + z) should simplify to x + z. "
        "The Identity rules may not be applied recursively.");
  };

  //============================================================================
  // PIPELINE COMPARISON
  //============================================================================

  "Pipeline comparison: recursive vs full"_test = [] {
    constexpr Symbol x;
    constexpr Symbol y;
    constexpr Symbol z;
    constexpr auto ctx = default_context();

    // x * (y + (z * 0)) should simplify to x * y
    // Step by step:
    // - (z * 0) → 0
    // - (y + 0) → y
    // - x * y (final)
    constexpr auto expr = x * (y + (z * 0_c));

    // Recursive simplifies nested expressions
    constexpr auto recursive = algebraic_simplify_recursive(expr, ctx);
    // NOTE: If this fails, it indicates a bug in the simplification library
    static_assert(match(recursive, x * y) || match(recursive, y * x),
                  "BUG: recursive should simplify x * (y + (z * 0)) to x * y");

    // Full with fixpoint ensures complete simplification
    constexpr auto full = full_simplify(expr, ctx);
    // NOTE: If this fails, it indicates a bug in the simplification library
    static_assert(
        match(full, x * y) || match(full, y * x),
        "BUG: full_simplify should simplify x * (y + (z * 0)) to x * y");
  };

  return 0;
}
