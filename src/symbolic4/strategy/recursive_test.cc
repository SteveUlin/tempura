// Tests for recursive rewrite rules
#include "symbolic4/constants.h"
#include "symbolic4/strategy/recursive.h"
#include "symbolic4/strategy/rule.h"
#include "symbolic4/interpreter/to_string.h"
#include "unit.h"

using namespace tempura;
using namespace tempura::symbolic4;

auto main() -> int {
  // =========================================================================
  // BASIC REC MARKER
  // =========================================================================

  "rec creates Rec marker"_test = [] {
    auto r = rec(x_);
    static_assert(is_rec_v<decltype(r)>);
  };

  "Rec participates in expressions"_test = [] {
    auto expr = rec(x_) + rec(y_);
    static_assert(is_expression_v<decltype(expr)>);
  };

  // =========================================================================
  // SIMPLE RECURSIVE RULE
  // =========================================================================

  "recursive rule applies once"_test = [] {
    Symbol<struct X> x;

    // Rule: x + 0 -> x (non-recursive, just to test infrastructure)
    auto simplify = recursive(
        rrule(x_ + 0_c, x_)
    );

    auto expr = x + 0_c;
    auto result = simplify.apply(expr);

    static_assert(isSame<decltype(result), decltype(x)>);
  };

  "recursive rule with actual recursion"_test = [] {
    Symbol<struct X> x;

    // Silly rule: f(x) -> x + rec(x), where rec means "apply rules to x"
    // Base case: x -> x (identity for non-f expressions)
    // This will expand f(f(x)) -> f(x) + (x + x) = f(x) + x + x
    // Actually let's do something simpler first

    // Rule: double(x) -> x + x
    // Rule: x -> x (base case, catches everything else)
    auto expand = recursive(
        rrule(x_ + x_, rec(x_) + rec(x_))  // Recurse into both sides
      | rule(x_, x_)  // Base case: anything else unchanged
    );

    // For a symbol, x + x -> x + x (base rule returns x unchanged, no infinite loop)
    auto result = expand.apply(x + x);
    static_assert(is_expression_v<decltype(result)>);
  };

  // =========================================================================
  // DIFFERENTIATION - The real test
  // =========================================================================

  "differentiation of sum"_test = [] {
    Symbol<struct X> x;
    Symbol<struct Y> y;

    // d/dx (x + y) = d/dx(x) + d/dx(y) = 1 + 0 = 1
    // But we'll just verify the structure, not simplify

    auto diff = recursive(
        rrule(x_ + y_, rec(x_) + rec(y_))   // sum rule
      | rule(x, 1_c)                         // d/dx(x) = 1
      | rule(y, 0_c)                         // d/dx(y) = 0 (y is constant wrt x)
    );

    auto expr = x + y;
    auto result = diff.apply(expr);

    // Result should be 1 + 0
    static_assert(is_expression_v<decltype(result)>);
    static_assert(isSame<get_op_t<decltype(result)>, AddOp>);

    auto lhs = result.template arg<0>();
    auto rhs = result.template arg<1>();
    static_assert(is_constant_v<decltype(lhs)>);
    static_assert(is_constant_v<decltype(rhs)>);
    static_assert(decltype(lhs)::value == 1);
    static_assert(decltype(rhs)::value == 0);
  };

  "differentiation of product"_test = [] {
    Symbol<struct X> x;
    Symbol<struct Y> y;

    // d/dx (x * y) = d/dx(x) * y + x * d/dx(y) = 1*y + x*0 = y + 0

    auto diff = recursive(
        rrule(x_ + y_, rec(x_) + rec(y_))           // sum rule
      | rrule(x_ * y_, rec(x_) * y_ + x_ * rec(y_)) // product rule
      | rule(x, 1_c)                                 // d/dx(x) = 1
      | rule(y, 0_c)                                 // d/dx(constant) = 0
    );

    auto expr = x * y;
    auto result = diff.apply(expr);

    // Result should be (1 * y) + (x * 0)
    static_assert(is_expression_v<decltype(result)>);
    static_assert(isSame<get_op_t<decltype(result)>, AddOp>);

    // Check structure: (1 * y) + (x * 0)
    auto lhs = result.template arg<0>();  // 1 * y
    auto rhs = result.template arg<1>();  // x * 0

    static_assert(isSame<get_op_t<decltype(lhs)>, MulOp>);
    static_assert(isSame<get_op_t<decltype(rhs)>, MulOp>);

    // lhs = 1 * y
    static_assert(decltype(lhs.template arg<0>())::value == 1);
    static_assert(isSame<decltype(lhs.template arg<1>()), decltype(y)>);

    // rhs = x * 0
    static_assert(isSame<decltype(rhs.template arg<0>()), decltype(x)>);
    static_assert(decltype(rhs.template arg<1>())::value == 0);
  };

  "differentiation of x^2 (x*x)"_test = [] {
    Symbol<struct X> x;

    auto diff = recursive(
        rrule(x_ + y_, rec(x_) + rec(y_))
      | rrule(x_ * y_, rec(x_) * y_ + x_ * rec(y_))
      | rule(x, 1_c)
    );

    // d/dx(x * x) = d/dx(x) * x + x * d/dx(x) = 1*x + x*1
    auto expr = x * x;
    auto result = diff.apply(expr);

    // Result should be (1 * x) + (x * 1)
    static_assert(is_expression_v<decltype(result)>);
    static_assert(isSame<get_op_t<decltype(result)>, AddOp>);

    auto lhs = result.template arg<0>();  // 1 * x
    auto rhs = result.template arg<1>();  // x * 1

    // Verify: 1 * x
    static_assert(decltype(lhs.template arg<0>())::value == 1);
    static_assert(isSame<decltype(lhs.template arg<1>()), decltype(x)>);

    // Verify: x * 1
    static_assert(isSame<decltype(rhs.template arg<0>()), decltype(x)>);
    static_assert(decltype(rhs.template arg<1>())::value == 1);
  };

  "differentiation with nested expressions"_test = [] {
    Symbol<struct X> x;

    auto diff = recursive(
        rrule(x_ + y_, rec(x_) + rec(y_))
      | rrule(x_ * y_, rec(x_) * y_ + x_ * rec(y_))
      | rule(x, 1_c)
      | rule(n_, 0_c)  // any constant/other symbol -> 0
    );

    // d/dx((x + 1) * x)
    // = d/dx(x+1) * x + (x+1) * d/dx(x)
    // = (1 + 0) * x + (x + 1) * 1
    auto expr = (x + 1_c) * x;
    auto result = diff.apply(expr);

    // Just verify it's an expression (full structure check would be verbose)
    static_assert(is_expression_v<decltype(result)>);
    static_assert(isSame<get_op_t<decltype(result)>, AddOp>);
  };

  "differentiation with sin/cos"_test = [] {
    Symbol<struct X> x;

    auto diff = recursive(
        rrule(x_ + y_, rec(x_) + rec(y_))
      | rrule(x_ * y_, rec(x_) * y_ + x_ * rec(y_))
      | rrule(sin(x_), cos(x_) * rec(x_))  // chain rule
      | rrule(cos(x_), -sin(x_) * rec(x_)) // chain rule
      | rule(x, 1_c)
      | rule(n_, 0_c)
    );

    // d/dx(sin(x)) = cos(x) * 1
    auto expr = sin(x);
    auto result = diff.apply(expr);

    static_assert(is_expression_v<decltype(result)>);
    static_assert(isSame<get_op_t<decltype(result)>, MulOp>);

    // cos(x) * 1
    auto lhs = result.template arg<0>();  // cos(x)
    auto rhs = result.template arg<1>();  // 1

    static_assert(isSame<get_op_t<decltype(lhs)>, CosOp>);
    static_assert(decltype(rhs)::value == 1);
  };

  "differentiation with chain rule: sin(x*x)"_test = [] {
    Symbol<struct X> x;

    auto diff = recursive(
        rrule(x_ + y_, rec(x_) + rec(y_))
      | rrule(x_ * y_, rec(x_) * y_ + x_ * rec(y_))
      | rrule(sin(x_), cos(x_) * rec(x_))
      | rule(x, 1_c)
    );

    // d/dx(sin(x*x)) = cos(x*x) * d/dx(x*x)
    //                = cos(x*x) * (1*x + x*1)
    auto expr = sin(x * x);
    auto result = diff.apply(expr);

    // Should be: cos(x*x) * ((1*x) + (x*1))
    static_assert(is_expression_v<decltype(result)>);
    static_assert(isSame<get_op_t<decltype(result)>, MulOp>);

    auto lhs = result.template arg<0>();  // cos(x*x)
    static_assert(isSame<get_op_t<decltype(lhs)>, CosOp>);

    auto inner = lhs.template arg<0>();  // x*x
    static_assert(isSame<get_op_t<decltype(inner)>, MulOp>);
  };

  // =========================================================================
  // MIXING RECURSIVE AND NON-RECURSIVE RULES
  // =========================================================================

  "can mix rrule and rule"_test = [] {
    Symbol<struct X> x;

    auto system = recursive(
        rrule(x_ + y_, rec(x_) + rec(y_))  // recursive
      | rule(x_ * 0_c, 0_c)                 // non-recursive simplification
      | rule(x, 1_c)                        // base case
    );

    // Apply to x + (y * 0) where y is another symbol
    // Should become 1 + 0 (after rule applies, x->1 and y*0->0)
    Symbol<struct Y> y;
    auto expr = x + (y * 0_c);
    auto result = system.apply(expr);

    // 1 + 0
    static_assert(is_expression_v<decltype(result)>);
    auto lhs = result.template arg<0>();
    auto rhs = result.template arg<1>();
    static_assert(decltype(lhs)::value == 1);
    static_assert(decltype(rhs)::value == 0);
  };

  return TestRegistry::result();
}
