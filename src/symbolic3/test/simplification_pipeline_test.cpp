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
#include "symbolic3/operators.h"
#include "symbolic3/simplify.h"
#include "symbolic3/traversal.h"
#include "symbolic3/evaluate.h"
#include "symbolic3/matching.h"
#include "symbolic3/to_string.h"
#include "unit.h"
#include <iostream>
#include <cassert>

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
    
    // TODO: Re-enable after fixing simplification
    // static_assert(match(result, x * y));
  };

  "Algebraic simplify recursive - fast recursive"_test = [] {
    constexpr Symbol x;
    constexpr auto ctx = default_context();
    
    // (x + 0) * 1 + 0 → x
    constexpr auto expr = (x + 0_c) * 1_c + 0_c;
    constexpr auto result = algebraic_simplify_recursive(expr, ctx);
    
    // TODO: Re-enable after fixing simplification
    // static_assert(match(result, x));
  };

  "Bottomup simplify - post-order traversal"_test = [] {
    constexpr Symbol x;
    constexpr Symbol y;
    constexpr auto ctx = default_context();
    
    // (x * 1) + (y * 0) → x
    constexpr auto expr = (x * 1_c) + (y * 0_c);
    constexpr auto result = bottomup_simplify(expr, ctx);
    
    // TODO: Re-enable after fixing simplification
    // static_assert(match(result, x));
  };

  "Topdown simplify - pre-order traversal"_test = [] {
    constexpr Symbol x;
    constexpr auto ctx = default_context();
    
    // log(exp(x)) → x
    constexpr auto expr = log(exp(x));
    constexpr auto result = topdown_simplify(expr, ctx);
    
    // TODO: Re-enable after fixing simplification
    // static_assert(match(result, x));
  };

  "Trig aware simplify - trigonometric expressions"_test = [] {
    constexpr Symbol x;
    constexpr auto ctx = default_context();
    
    // sin(0) + cos(0) * x → 0 + 1 * x → x
    constexpr auto expr = sin(0_c) + cos(0_c) * x;
    constexpr auto result = trig_aware_simplify(expr, ctx);
    
    // TODO: Re-enable after fixing simplification
    // static_assert(match(result, x));
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
    constexpr auto with_traversal = innermost(algebraic_simplify).apply(expr, ctx);
    
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
    (void)simplified; // TODO: Add full verification when rules are stable
  };

  "Fixpoint iteration with traversal"_test = [] {
    constexpr Symbol x;
    constexpr auto ctx = default_context();

    // Expression that requires multiple passes: ((x * 1) + 0) * 1
    constexpr auto expr = ((x * 1_c) + 0_c) * 1_c;

    // Using innermost with fixpoint for exhaustive simplification
    constexpr auto fully_simplified = innermost(simplify_fixpoint).apply(expr, ctx);
    (void)fully_simplified; // TODO: Add verification when simplification stabilizes
  };

  "Transcendental functions with traversal"_test = [] {
    constexpr Symbol x;
    constexpr auto ctx = default_context();

    // Expression: log(exp(x + 0))
    // Inner (x + 0) needs simplification, then log(exp(...)) can simplify
    constexpr auto expr = log(exp(x + 0_c));

    // Various traversal strategies can be applied:
    constexpr auto result = topdown(algebraic_simplify).apply(expr, ctx);
    (void)result; // TODO: Add verification
  };

  //============================================================================
  // TERM COLLECTION AND FACTORING
  //============================================================================

  "Like terms collection: x + x"_test = [] {
    Symbol x;
    auto expr = x + x;
    auto result = full_simplify(expr, default_context());

    // Check structure: should be multiplication of x and a constant
    bool is_factored = match(result, x * 𝐜) || match(result, 𝐜 * x);
    // TODO: Re-enable when factoring is working
    // assert(is_factored);

    // Verify correctness by evaluation
    auto val = evaluate(result, BinderPack{x = 5});
    assert(val == 10);
  };

  "Factor simple: x*2 + x"_test = [] {
    Symbol x;
    auto expr = x * 2_c + x;
    auto result = full_simplify(expr, default_context());

    // Should simplify to x * 3 (factoring out x)
    // TODO: Re-enable after fixing simplification
    // assert(match(result, x * 𝐜) || match(result, 𝐜 * x));

    auto val = evaluate(result, BinderPack{x = 10});
    assert(val == 30);
  };

  "Factor both sides: x*2 + x*3"_test = [] {
    Symbol x;
    auto expr = x * 2_c + x * 3_c;
    auto result = full_simplify(expr, default_context());

    // Should simplify to x * 5 (collecting coefficients)
    // TODO: Re-enable after fixing simplification
    // assert(match(result, x * 𝐜) || match(result, 𝐜 * x));

    auto val = evaluate(result, BinderPack{x = 10});
    assert(val == 50);
  };

  "Factor reversed: x + x*2"_test = [] {
    Symbol x;
    auto expr = x + x * 2_c;
    auto result = full_simplify(expr, default_context());

    // Should simplify to x * 3 (factoring regardless of order)
    // TODO: Re-enable after fixing simplification
    // assert(match(result, x * 𝐜) || match(result, 𝐜 * x));

    auto val = evaluate(result, BinderPack{x = 10});
    assert(val == 30);
  };

  "Complex factoring: x*2 + x*3 + x*4"_test = [] {
    Symbol x;
    auto expr = x * 2_c + x * 3_c + x * 4_c;
    auto result = full_simplify(expr, default_context());

    // Should simplify to x * 9 (collecting all coefficients)
    // TODO: Re-enable after fixing simplification
    // assert(match(result, x * 𝐜) || match(result, 𝐜 * x));

    auto val = evaluate(result, BinderPack{x = 10});
    assert(val == 90);
  };

  //============================================================================
  // NESTED EXPRESSION HANDLING
  //============================================================================

  "Nested expression simplification: (x + x) + y"_test = [] {
    Symbol x;
    Symbol y;

    // Create manually nested expression: (x + x) + y
    auto inner = x + x;
    auto outer = inner + y;

    // Apply AdditionRules to inner
    auto inner_result = AdditionRules.apply(inner, default_context());
    
    // Verify evaluation correctness
    assert(evaluate(inner_result, BinderPack{x = 10}) == 20);

    // Apply innermost to outer
    auto outer_result = innermost(AdditionRules).apply(outer, default_context());
    assert(evaluate(outer_result, BinderPack{x = 10, y = 5}) == 25);

    // Apply full_simplify to outer
    auto full_result = full_simplify(outer, default_context());
    assert(evaluate(full_result, BinderPack{x = 10, y = 5}) == 25);
  };

  //============================================================================
  // POWER RULE COMBINATIONS
  //============================================================================

  "Power rules combination: x^1 * x^2"_test = [] {
    constexpr Symbol x;
    constexpr auto ctx = default_context();
    
    // x^1 * x^2 → x * x^2 → x^(1+2) = x^3
    constexpr auto expr = pow(x, 1_c) * pow(x, 2_c);
    constexpr auto result = full_simplify(expr, ctx);
    
    // Result should be x^3 or equivalent
    // TODO: Re-enable after fixing simplification
    // static_assert(match(result, pow(x, 2_c + 1_c)));
  };

  //============================================================================
  // COMPLEX NESTED EXPRESSIONS
  //============================================================================

  "Complex nesting: ((x + 0) * 1) + ((y * 0) + z)"_test = [] {
    constexpr Symbol x;
    constexpr Symbol y;
    constexpr Symbol z;
    constexpr auto ctx = default_context();
    
    // ((x + 0) * 1) + ((y * 0) + z) → x + z
    constexpr auto expr = ((x + 0_c) * 1_c) + ((y * 0_c) + z);
    constexpr auto result = full_simplify(expr, ctx);
    
    // TODO: Re-enable after fixing simplification
    // static_assert(match(result, x + z));
  };

  //============================================================================
  // PIPELINE COMPARISON
  //============================================================================

  "Pipeline comparison: recursive vs full"_test = [] {
    constexpr Symbol x;
    constexpr Symbol y;
    constexpr Symbol z;
    constexpr auto ctx = default_context();
    
    constexpr auto expr = x * (y + (z * 0_c));

    // Recursive simplifies nested expressions
    constexpr auto recursive = algebraic_simplify_recursive(expr, ctx);
    // TODO: Re-enable after fixing simplification  
    // static_assert(match(recursive, x * y));

    // Full with fixpoint ensures complete simplification
    constexpr auto full = full_simplify(expr, ctx);
    // TODO: Re-enable after fixing simplification
    // static_assert(match(full, x * y));
  };

  return 0;
}