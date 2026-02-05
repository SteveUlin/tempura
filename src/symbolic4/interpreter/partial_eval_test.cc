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
  // EvalIfGround tests
  // =========================================================================
  // After deleting Embedded<T>, EvalIfGround returns ground expressions
  // unchanged (can't construct Constant<V> at runtime). The info-domain
  // simplifier handles actual constant folding. These tests verify the
  // passthrough behavior.

  "EvalIfGround: constants pass through"_test = [] {
    auto expr = 1_c + 2_c;
    auto result = EvalIfGround{}.apply(expr);

    // Ground but not a leaf constant — returned unchanged
    static_assert(is_ground_v<decltype(result)>);
    static_assert(is_expression_v<decltype(result)>);
  };

  "EvalIfGround: leaf constants stay as-is"_test = [] {
    auto result = EvalIfGround{}.apply(5_c);

    static_assert(is_constant_v<decltype(result)>);
  };

  "EvalIfGround: expression with symbol → unchanged"_test = [&] {
    auto expr = x + 1_c;
    auto result = EvalIfGround{}.apply(expr);

    static_assert(isSame<decltype(result), decltype(expr)>);
  };

  // =========================================================================
  // partialEval (bottom-up application)
  // =========================================================================

  "partialEval: preserves symbolic structure"_test = [&] {
    // x + y should be unchanged
    auto expr = x + y;
    auto result = partialEval(expr);

    static_assert(isSame<decltype(result), decltype(expr)>);
  };

  "partialEval: mixed expression preserves shape"_test = [&] {
    auto expr = (1_c + 2_c) * x;
    auto result = partialEval(expr);

    // Structure preserved: still a MulOp expression
    static_assert(is_expression_v<decltype(result)>);
    static_assert(isSame<get_op_t<decltype(result)>, MulOp>);

    // RHS is still x
    auto rhs = result.template arg<1>();
    static_assert(isSame<decltype(rhs), decltype(x)>);
  };

  "partialEval: nested ground terms maintain structure"_test = [&] {
    auto expr = sin(0_c) * x + cos(0_c) * y;
    auto result = partialEval(expr);

    static_assert(is_expression_v<decltype(result)>);
    static_assert(isSame<get_op_t<decltype(result)>, AddOp>);
  };

  return TestRegistry::result();
}
