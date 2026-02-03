// Tests for partial evaluation
#include "symbolic4/constants.h"
#include "symbolic4/interpreter/partial_eval.h"
#include "unit.h"

#include <cmath>

using namespace tempura;
using namespace tempura::symbolic4;

auto main() -> int {
  Symbol<struct X> x;
  Symbol<struct Y> y;

  // =========================================================================
  // IsGround tests
  // =========================================================================

  "constants are ground"_test = [] {
    static_assert(is_ground_v<Constant<5>>);
    static_assert(is_ground_v<decltype(1_c)>);
    static_assert(is_ground_v<decltype(0_c)>);
  };

  "literals are ground"_test = [] {
    auto l = lit(3.14);
    static_assert(is_ground_v<decltype(l)>);
  };

  "symbols are NOT ground"_test = [&] {
    static_assert(!is_ground_v<decltype(x)>);
    static_assert(!is_ground_v<decltype(y)>);
  };

  "constant expressions are ground"_test = [] {
    static_assert(is_ground_v<decltype(1_c + 2_c)>);
    static_assert(is_ground_v<decltype(sin(1_c))>);
    static_assert(is_ground_v<decltype(sin(1_c + 2_c) * cos(3_c))>);
  };

  "expressions with symbols are NOT ground"_test = [&] {
    static_assert(!is_ground_v<decltype(x + 1_c)>);
    static_assert(!is_ground_v<decltype(sin(x))>);
    static_assert(!is_ground_v<decltype(1_c + x)>);
  };

  "mixed expressions: ground parts don't make whole ground"_test = [&] {
    static_assert(!is_ground_v<decltype((1_c + 2_c) * x)>);
    static_assert(!is_ground_v<decltype(sin(1_c) + x)>);
  };

  "nullary constants (pi, e) are ground"_test = [] {
    static_assert(is_ground_v<decltype(pi)>);
    static_assert(is_ground_v<decltype(e)>);
  };

  // =========================================================================
  // HasLiteral tests
  // =========================================================================

  "constants don't have literals"_test = [] {
    static_assert(!has_literal_v<decltype(1_c)>);
    static_assert(!has_literal_v<decltype(1_c + 2_c)>);
  };

  "literals have literals"_test = [] {
    auto l = lit(3.14);
    static_assert(has_literal_v<decltype(l)>);
  };

  "mixed constant/literal expressions have literals"_test = [] {
    auto l = lit(3.14);
    static_assert(has_literal_v<decltype(l + 1_c)>);
    static_assert(has_literal_v<decltype(1_c + l)>);
    static_assert(has_literal_v<decltype(sin(l))>);
  };

  // =========================================================================
  // EvalIfGround tests
  // =========================================================================

  "EvalIfGround: constant arithmetic -> constant"_test = [] {
    auto expr = 1_c + 2_c;
    auto result = EvalIfGround{}.apply(expr);

    // Should be Constant<3.0>
    static_assert(is_constant_v<decltype(result)>);
    static_assert(decltype(result)::value == 3.0);
  };

  "EvalIfGround: nested constant expr -> constant"_test = [] {
    auto expr = (1_c + 2_c) * 4_c;
    auto result = EvalIfGround{}.apply(expr);

    static_assert(is_constant_v<decltype(result)>);
    static_assert(decltype(result)::value == 12.0);
  };

  "EvalIfGround: expression with literal -> literal"_test = [] {
    auto l = lit(2.0);
    auto expr = l + 3_c;
    auto result = EvalIfGround{}.apply(expr);

    static_assert(is_literal_v<decltype(result)>);
    expectNear(result.effect_.value, 5.0);
  };

  "EvalIfGround: trig of constant -> constant"_test = [] {
    auto expr = sin(0_c);
    auto result = EvalIfGround{}.apply(expr);

    static_assert(is_constant_v<decltype(result)>);
    expectNear(decltype(result)::value, 0.0, 1e-10);
  };

  "EvalIfGround: expression with symbol -> unchanged"_test = [&] {
    auto expr = x + 1_c;
    auto result = EvalIfGround{}.apply(expr);

    // Should be unchanged (same type)
    static_assert(isSame<decltype(result), decltype(expr)>);
  };

  // =========================================================================
  // partialEval (bottom-up application)
  // =========================================================================

  "partialEval: evaluates ground subexpressions"_test = [&] {
    // (1 + 2) * x should become 3 * x
    auto expr = (1_c + 2_c) * x;
    auto result = partialEval(expr);

    // Result should be Constant<3> * x
    static_assert(is_expression_v<decltype(result)>);
    static_assert(isSame<get_op_t<decltype(result)>, MulOp>);

    auto lhs = result.template arg<0>();
    static_assert(is_constant_v<decltype(lhs)>);
    static_assert(decltype(lhs)::value == 3.0);

    auto rhs = result.template arg<1>();
    static_assert(isSame<decltype(rhs), decltype(x)>);
  };

  "partialEval: nested ground terms"_test = [&] {
    // sin(0) * x + cos(0) * y
    // sin(0) = 0, cos(0) = 1
    // -> 0 * x + 1 * y
    auto expr = sin(0_c) * x + cos(0_c) * y;
    auto result = partialEval(expr);

    // Check structure: Constant<0> * x + Constant<1> * y
    static_assert(is_expression_v<decltype(result)>);
    static_assert(isSame<get_op_t<decltype(result)>, AddOp>);
  };

  "partialEval: fully ground expression"_test = [] {
    auto expr = sin(1_c + 2_c) * cos(0_c);
    auto result = partialEval(expr);

    // sin(3) * cos(0) = sin(3) * 1 = sin(3)
    static_assert(is_constant_v<decltype(result)>);
    expectNear(decltype(result)::value, std::sin(3.0), 1e-10);
  };

  "partialEval: preserves symbolic structure"_test = [&] {
    // x + y should be unchanged
    auto expr = x + y;
    auto result = partialEval(expr);

    static_assert(isSame<decltype(result), decltype(expr)>);
  };

  "partialEval: pi and e evaluate"_test = [] {
    auto expr = pi + e;
    auto result = partialEval(expr);

    static_assert(is_constant_v<decltype(result)>);
    expectNear(decltype(result)::value, M_PI + M_E, 1e-10);
  };

  return TestRegistry::result();
}
