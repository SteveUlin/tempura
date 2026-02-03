// Layer 1 simplification tests
#include "symbolic4/constants.h"
#include "symbolic4/core.h"
#include "symbolic4/interpreter/eval.h"
#include "symbolic4/interpreter/simplify.h"
#include "symbolic4/operators.h"
#include "unit.h"

using namespace tempura;
using namespace tempura::symbolic4;

auto main() -> int {
  // ============================================================================
  // Constant folding tests (require simplifyFull for partial evaluation)
  // ============================================================================

  "constant folding: addition"_test = [] {
    auto expr = Constant<2>{} + Constant<3>{};
    auto result = simplifyFull(expr);
    static_assert(is_constant_v<decltype(result)>);
    static_assert(decltype(result)::value == 5);
  };

  "constant folding: multiplication"_test = [] {
    auto expr = Constant<2>{} * Constant<3>{};
    auto result = simplifyFull(expr);
    static_assert(is_constant_v<decltype(result)>);
    static_assert(decltype(result)::value == 6);
  };

  "constant folding: subtraction"_test = [] {
    auto expr = Constant<5>{} - Constant<3>{};
    auto result = simplifyFull(expr);
    static_assert(is_constant_v<decltype(result)>);
    static_assert(decltype(result)::value == 2);
  };

  "constant folding: division"_test = [] {
    auto expr = Constant<6>{} / Constant<2>{};
    auto result = simplifyFull(expr);
    static_assert(is_constant_v<decltype(result)>);
    static_assert(decltype(result)::value == 3);
  };

  // ============================================================================
  // Identity elimination tests
  // ============================================================================

  "identity: x + 0 -> x"_test = [] {
    Symbol<struct X> x;
    auto expr = x + 0_c;
    auto result = simplify(expr);
    static_assert(std::is_same_v<decltype(result), Symbol<struct X>>);
  };

  "identity: 0 + x -> x"_test = [] {
    Symbol<struct X> x;
    auto expr = 0_c + x;
    auto result = simplify(expr);
    static_assert(std::is_same_v<decltype(result), Symbol<struct X>>);
  };

  "identity: x * 1 -> x"_test = [] {
    Symbol<struct X> x;
    auto expr = x * 1_c;
    auto result = simplify(expr);
    static_assert(std::is_same_v<decltype(result), Symbol<struct X>>);
  };

  "identity: 1 * x -> x"_test = [] {
    Symbol<struct X> x;
    auto expr = 1_c * x;
    auto result = simplify(expr);
    static_assert(std::is_same_v<decltype(result), Symbol<struct X>>);
  };

  "identity: x - 0 -> x"_test = [] {
    Symbol<struct X> x;
    auto expr = x - 0_c;
    auto result = simplify(expr);
    static_assert(std::is_same_v<decltype(result), Symbol<struct X>>);
  };

  "identity: x / 1 -> x"_test = [] {
    Symbol<struct X> x;
    auto expr = x / 1_c;
    auto result = simplify(expr);
    static_assert(std::is_same_v<decltype(result), Symbol<struct X>>);
  };

  "identity: x ^ 1 -> x"_test = [] {
    Symbol<struct X> x;
    auto expr = pow(x, 1_c);
    auto result = simplify(expr);
    static_assert(std::is_same_v<decltype(result), Symbol<struct X>>);
  };

  "identity: x ^ 0 -> 1"_test = [] {
    Symbol<struct X> x;
    auto expr = pow(x, 0_c);
    auto result = simplify(expr);
    static_assert(std::is_same_v<decltype(result), Constant<1.>>);
  };

  // ============================================================================
  // Zero annihilation tests
  // ============================================================================

  "0_c: x * 0 -> 0"_test = [] {
    Symbol<struct X> x;
    auto expr = x * 0_c;
    auto result = simplify(expr);
    static_assert(std::is_same_v<decltype(result), Constant<0.>>);
  };

  "0_c: 0 * x -> 0"_test = [] {
    Symbol<struct X> x;
    auto expr = 0_c * x;
    auto result = simplify(expr);
    static_assert(std::is_same_v<decltype(result), Constant<0.>>);
  };

  "0_c: 0 / x -> 0"_test = [] {
    Symbol<struct X> x;
    auto expr = 0_c / x;
    auto result = simplify(expr);
    static_assert(std::is_same_v<decltype(result), Constant<0.>>);
  };

  // ============================================================================
  // Self operation tests
  // ============================================================================

  "self: x - x -> 0"_test = [] {
    Symbol<struct X> x;
    auto expr = x - x;
    auto result = simplify(expr);
    static_assert(std::is_same_v<decltype(result), Constant<0.>>);
  };

  "self: x / x -> 1"_test = [] {
    Symbol<struct X> x;
    auto expr = x / x;
    auto result = simplify(expr);
    static_assert(std::is_same_v<decltype(result), Constant<1.>>);
  };

  // ============================================================================
  // Double negation test
  // ============================================================================

  "double negation: -(-x) -> x"_test = [] {
    Symbol<struct X> x;
    auto expr = -(-x);
    auto result = simplify(expr);
    static_assert(std::is_same_v<decltype(result), Symbol<struct X>>);
  };

  // ============================================================================
  // Inverse cancellation tests
  // ============================================================================

  "inverse: exp(log(x)) -> x"_test = [] {
    Symbol<struct X> x;
    auto expr = exp(log(x));
    auto result = simplify(expr);
    static_assert(std::is_same_v<decltype(result), Symbol<struct X>>);
  };

  "inverse: log(exp(x)) -> x"_test = [] {
    Symbol<struct X> x;
    auto expr = log(exp(x));
    auto result = simplify(expr);
    static_assert(std::is_same_v<decltype(result), Symbol<struct X>>);
  };

  // ============================================================================
  // Nested simplification tests
  // ============================================================================

  "nested: (x + 0) * 1 -> x"_test = [] {
    Symbol<struct X> x;
    auto expr = (x + 0_c) * 1_c;
    auto result = simplify(expr);
    static_assert(std::is_same_v<decltype(result), Symbol<struct X>>);
  };

  "nested: 1 * (0 + x) -> x"_test = [] {
    Symbol<struct X> x;
    auto expr = 1_c * (0_c + x);
    auto result = simplify(expr);
    static_assert(std::is_same_v<decltype(result), Symbol<struct X>>);
  };

  "nested: (2 + 3) * x -> 5 * x"_test = [] {
    Symbol<struct X> x;
    auto expr = (Constant<2>{} + Constant<3>{}) * x;
    auto result = simplifyFull(expr);
    static_assert(is_expression_v<decltype(result)>);
    static_assert(std::is_same_v<get_op_t<decltype(result)>, MulOp>);
    // Verify 5 * x structure
    auto lhs = result.template arg<0>();
    static_assert(is_constant_v<decltype(lhs)>);
    static_assert(decltype(lhs)::value == 5);
  };

  "nested: x * (0 * y) -> 0"_test = [] {
    Symbol<struct X> x;
    Symbol<struct Y> y;
    auto expr = x * (0_c * y);
    auto result = simplify(expr);
    static_assert(std::is_same_v<decltype(result), Constant<0.>>);
  };

  "nested: -(-(-x)) -> -x"_test = [] {
    Symbol<struct X> x;
    auto expr = -(-(-x));
    auto result = simplify(expr);
    static_assert(is_expression_v<decltype(result)>);
    static_assert(std::is_same_v<get_op_t<decltype(result)>, NegOp>);
  };

  // ============================================================================
  // Pass-through tests (expressions that shouldn't simplify)
  // ============================================================================

  "passthrough: x + y unchanged"_test = [] {
    Symbol<struct X> x;
    Symbol<struct Y> y;
    auto expr = x + y;
    auto result = simplify(expr);
    static_assert(is_expression_v<decltype(result)>);
    static_assert(std::is_same_v<get_op_t<decltype(result)>, AddOp>);
  };

  "passthrough: sin(x) unchanged"_test = [] {
    Symbol<struct X> x;
    auto expr = sin(x);
    auto result = simplify(expr);
    static_assert(is_expression_v<decltype(result)>);
    static_assert(std::is_same_v<get_op_t<decltype(result)>, SinOp>);
  };

  "passthrough: terminals unchanged"_test = [] {
    Symbol<struct X> x;
    auto sym_result = simplify(x);
    static_assert(std::is_same_v<decltype(sym_result), Symbol<struct X>>);

    auto const_result = simplify(Constant<42>{});
    static_assert(std::is_same_v<decltype(const_result), Constant<42>>);

    auto pi_result = simplify(pi);
    static_assert(std::is_same_v<decltype(pi_result), Expression<PiOp>>);
  };

  // ============================================================================
  // Evaluation consistency tests
  // ============================================================================

  "eval consistency: simplify preserves value"_test = [] {
    Symbol<struct X> x;

    auto expr1 = x + 0_c;
    auto simp1 = simplify(expr1);
    expectNear(evaluate(expr1, x = 5.0), evaluate(simp1, x = 5.0));

    auto expr2 = x * 1_c;
    auto simp2 = simplify(expr2);
    expectNear(evaluate(expr2, x = 7.0), evaluate(simp2, x = 7.0));

    auto expr3 = (x + 0_c) * 1_c;
    auto simp3 = simplify(expr3);
    expectNear(evaluate(expr3, x = 3.14), evaluate(simp3, x = 3.14));
  };

  // ============================================================================
  // Fraction handling tests
  // ============================================================================

  "fraction: Fraction<0,1> treated as 0_c"_test = [] {
    Symbol<struct X> x;
    auto expr = x + Fraction<0, 1>{};
    auto result = simplify(expr);
    static_assert(std::is_same_v<decltype(result), Symbol<struct X>>);
  };

  "fraction: Fraction<1,1> treated as 1_c"_test = [] {
    Symbol<struct X> x;
    auto expr = x * Fraction<1, 1>{};
    auto result = simplify(expr);
    static_assert(std::is_same_v<decltype(result), Symbol<struct X>>);
  };

  "fraction: Fraction<2,2> treated as 1_c"_test = [] {
    Symbol<struct X> x;
    auto expr = x * Fraction<2, 2>{};
    auto result = simplify(expr);
    static_assert(std::is_same_v<decltype(result), Symbol<struct X>>);
  };

  return TestRegistry::result();
}
