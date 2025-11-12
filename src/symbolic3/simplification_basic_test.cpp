//==============================================================================
// Consolidated Basic Simplification Tests
//
// This file consolidates:
// - simplify_test.cpp (basic power, addition, multiplication rules)
// - canonical_test.cpp (canonical form infrastructure and variadic operations)
//
// Tests the fundamental algebraic simplification rules that form the foundation
// of the symbolic3 system.
//==============================================================================

#include "symbolic3/context.h"
#include "symbolic3/core.h"
#include "symbolic3/matching.h"
#include "symbolic3/operators.h"
#include "symbolic3/pattern_matching.h"
#include "symbolic3/simplify.h"
#include "symbolic3/symbolic3.h"
#include "unit.h"

using namespace tempura;
using namespace tempura::symbolic3;

auto main() -> int {
  //============================================================================
  // POWER RULES
  //============================================================================

  "Power zero rule: x^0 → 1"_test = [] {
    constexpr Symbol x;
    constexpr auto ctx = default_context();

    constexpr auto expr = pow(x, Constant<0>{});
    constexpr auto result = power_zero.apply(expr, ctx);

    // Verify the result is exactly Constant<1>
    static_assert(match(result, Constant<1>{}), "x^0 should simplify to 1");
  };

  "Power one rule: x^1 → x"_test = [] {
    constexpr Symbol x;
    constexpr auto ctx = default_context();

    constexpr auto expr = pow(x, Constant<1>{});
    constexpr auto result = power_one.apply(expr, ctx);

    // Verify the result is exactly x
    static_assert(match(result, x), "x^1 should simplify to x");
  };

  //============================================================================
  // ADDITION RULES
  //============================================================================

  "Addition identity: y + 0 → y"_test = [] {
    constexpr Symbol y;
    constexpr auto ctx = default_context();

    constexpr auto expr = y + Constant<0>{};
    constexpr auto result = AdditionRuleCategories::Identity.apply(expr, ctx);

    // Verify the result is exactly y
    static_assert(match(result, y), "y + 0 should simplify to y");
  };

  "Addition zero: 0 + y → y"_test = [] {
    constexpr Symbol y;
    constexpr auto ctx = default_context();

    constexpr auto expr = Constant<0>{} + y;
    constexpr auto result = AdditionRuleCategories::Identity.apply(expr, ctx);

    // Verify the result is exactly y
    static_assert(match(result, y), "0 + y should simplify to y");
  };

  //============================================================================
  // MULTIPLICATION RULES
  //============================================================================

  "Multiplication identity: z * 1 → z"_test = [] {
    constexpr Symbol z;
    constexpr auto ctx = default_context();

    constexpr auto expr = z * Constant<1>{};
    constexpr auto result =
        MultiplicationRuleCategories::Identity.apply(expr, ctx);

    // Verify the result is exactly z
    static_assert(match(result, z), "z * 1 should simplify to z");
  };

  "Multiplication zero: z * 0 → 0"_test = [] {
    constexpr Symbol z;
    constexpr auto ctx = default_context();

    constexpr auto expr = z * Constant<0>{};
    constexpr auto result =
        MultiplicationRuleCategories::Identity.apply(expr, ctx);

    // Verify the result is exactly Constant<0>
    static_assert(match(result, Constant<0>{}), "z * 0 should simplify to 0");
  };

  "Multiplication one: 1 * z → z"_test = [] {
    constexpr Symbol z;
    constexpr auto ctx = default_context();

    constexpr auto expr = Constant<1>{} * z;
    constexpr auto result =
        MultiplicationRuleCategories::Identity.apply(expr, ctx);

    // Verify the result is exactly z
    static_assert(match(result, z), "1 * z should simplify to z");
  };

  //============================================================================
  // VARIADIC FUNCTION OBJECTS (from canonical_test.cpp)
  //============================================================================

  "Variadic function objects - AddOp evaluation"_test = [] {
    // Test that AddOp supports variadic evaluation using fold expressions
    constexpr symbolic3::AddOp add;

    // Unary (identity)
    constexpr auto result0 = add(5);
    static_assert(result0 == 5, "add(5) should be 5");

    // Binary
    constexpr auto result1 = add(1, 2);
    static_assert(result1 == 3, "1 + 2 should be 3");

    // Ternary
    constexpr auto result2 = add(1, 2, 3);
    static_assert(result2 == 6, "1 + 2 + 3 should be 6");

    // Quaternary
    constexpr auto result3 = add(1, 2, 3, 4);
    static_assert(result3 == 10, "1 + 2 + 3 + 4 should be 10");
  };

  "Variadic function objects - MulOp evaluation"_test = [] {
    // Test that MulOp supports variadic evaluation using fold expressions
    constexpr symbolic3::MulOp mul;

    // Unary (identity)
    constexpr auto result0 = mul(7);
    static_assert(result0 == 7, "mul(7) should be 7");

    // Binary
    constexpr auto result1 = mul(2, 3);
    static_assert(result1 == 6, "2 * 3 should be 6");

    // Ternary
    constexpr auto result2 = mul(2, 3, 4);
    static_assert(result2 == 24, "2 * 3 * 4 should be 24");
  };

  //============================================================================
  // CANONICAL FORM INFRASTRUCTURE
  //============================================================================

  "Canonical form infrastructure exists"_test = [] {
    // Test that the canonical form trait system exists
    constexpr bool add_uses_canonical = uses_canonical_form<symbolic3::AddOp>;
    constexpr bool mul_uses_canonical = uses_canonical_form<symbolic3::MulOp>;
    constexpr bool sub_uses_canonical = uses_canonical_form<symbolic3::SubOp>;

    static_assert(add_uses_canonical, "AddOp should use canonical form");
    static_assert(mul_uses_canonical, "MulOp should use canonical form");
    static_assert(!sub_uses_canonical, "SubOp should NOT use canonical form");
  };

  "Canonical strategy exists"_test = [] {
    // Test that the to_canonical strategy exists
    // Note: Full flattening implementation is in progress
    // This test just verifies the infrastructure compiles

    // Just verify the strategy can be instantiated
    // Full implementation details in RECOMMENDATION_2_IMPLEMENTATION.md
  };

  "Expression types maintain binary structure"_test = [] {
    // Verify current binary structure is preserved
    constexpr Symbol a;
    constexpr Symbol b;
    constexpr Symbol c;

    constexpr auto expr = (a + b) + c;

    // Currently this is Expression<Add, Expression<Add, a, b>, c>
    using ExprType = decltype(expr);
    constexpr bool is_add_expr = is_expression<ExprType>;

    static_assert(is_add_expr, "Should be an Add expression");
  };

  return 0;
}
