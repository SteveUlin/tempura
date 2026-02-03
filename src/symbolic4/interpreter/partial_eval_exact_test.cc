// Tests for exact partial evaluation
#include "symbolic4/interpreter/partial_eval_exact.h"

#include "symbolic4/constants.h"
#include "symbolic4/fraction_ops.h"
#include "symbolic4/interpreter/eval.h"
#include "symbolic4/operators.h"
#include "unit.h"

using namespace tempura;
using namespace tempura::symbolic4;

auto main() -> int {
  Symbol<struct X> x;
  Symbol<struct Y> y;

  // =========================================================================
  // is_exact_v tests
  // =========================================================================

  "Fractions are exact"_test = [] {
    static_assert(is_exact_v<Fraction<1, 2>>);
    static_assert(is_exact_v<Fraction<3, 4>>);
    static_assert(is_exact_v<Fraction<0, 1>>);
  };

  "Integer Constants are exact"_test = [] {
    static_assert(is_exact_v<Constant<5>>);
    static_assert(is_exact_v<Constant<0>>);
    static_assert(is_exact_v<Constant<-3>>);
  };

  "Double Constants are NOT exact"_test = [] {
    static_assert(!is_exact_v<Constant<3.14>>);
    static_assert(!is_exact_v<Constant<0.5>>);
  };

  "Symbols are NOT exact"_test = [&] {
    static_assert(!is_exact_v<decltype(x)>);
  };

  "Exact operations preserve exactness"_test = [] {
    // Addition of fractions
    auto add_expr = Expression<AddOp, Fraction<1, 2>, Fraction<1, 3>>{};
    static_assert(is_exact_v<decltype(add_expr)>);

    // Subtraction
    auto sub_expr = Expression<SubOp, Fraction<3, 4>, Fraction<1, 4>>{};
    static_assert(is_exact_v<decltype(sub_expr)>);

    // Multiplication
    auto mul_expr = Expression<MulOp, Fraction<2, 3>, Fraction<3, 4>>{};
    static_assert(is_exact_v<decltype(mul_expr)>);

    // Division
    auto div_expr = Expression<DivOp, Fraction<1, 2>, Fraction<2, 3>>{};
    static_assert(is_exact_v<decltype(div_expr)>);

    // Negation
    auto neg_expr = Expression<NegOp, Fraction<1, 2>>{};
    static_assert(is_exact_v<decltype(neg_expr)>);
  };

  "Transcendental operations are NOT exact"_test = [] {
    auto sin_expr = Expression<SinOp, Fraction<0, 1>>{};
    static_assert(!is_exact_v<decltype(sin_expr)>);

    auto log_expr = Expression<LogOp, Fraction<1, 1>>{};
    static_assert(!is_exact_v<decltype(log_expr)>);

    auto exp_expr = Expression<ExpOp, Fraction<0, 1>>{};
    static_assert(!is_exact_v<decltype(exp_expr)>);
  };

  "Mixed exact and symbolic is NOT exact"_test = [&] {
    auto expr = Expression<AddOp, Fraction<1, 2>, decltype(x)>{};
    static_assert(!is_exact_v<decltype(expr)>);
  };

  // =========================================================================
  // exactEval tests
  // =========================================================================

  "exactEval: Fraction terminal unchanged"_test = [] {
    auto result = exactEval(Fraction<3, 4>{});
    static_assert(is_fraction_v<decltype(result)>);
    static_assert(decltype(result)::numerator == 3);
    static_assert(decltype(result)::denominator == 4);
  };

  "exactEval: Integer Constant becomes Fraction"_test = [] {
    auto result = exactEval(Constant<5>{});
    static_assert(is_fraction_v<decltype(result)>);
    static_assert(decltype(result)::numerator == 5);
    static_assert(decltype(result)::denominator == 1);
  };

  "exactEval: Addition"_test = [] {
    auto expr = Expression<AddOp, Fraction<1, 2>, Fraction<1, 3>>{};
    auto result = exactEval(expr);
    // 1/2 + 1/3 = 5/6
    static_assert(is_fraction_v<decltype(result)>);
    static_assert(decltype(result)::numerator == 5);
    static_assert(decltype(result)::denominator == 6);
  };

  "exactEval: Subtraction"_test = [] {
    auto expr = Expression<SubOp, Fraction<3, 4>, Fraction<1, 4>>{};
    auto result = exactEval(expr);
    // 3/4 - 1/4 = 1/2
    static_assert(decltype(result)::numerator == 1);
    static_assert(decltype(result)::denominator == 2);
  };

  "exactEval: Multiplication"_test = [] {
    auto expr = Expression<MulOp, Fraction<2, 3>, Fraction<3, 4>>{};
    auto result = exactEval(expr);
    // 2/3 * 3/4 = 6/12 = 1/2
    static_assert(decltype(result)::numerator == 1);
    static_assert(decltype(result)::denominator == 2);
  };

  "exactEval: Division"_test = [] {
    auto expr = Expression<DivOp, Fraction<1, 2>, Fraction<2, 3>>{};
    auto result = exactEval(expr);
    // (1/2) / (2/3) = 3/4
    static_assert(decltype(result)::numerator == 3);
    static_assert(decltype(result)::denominator == 4);
  };

  "exactEval: Negation"_test = [] {
    auto expr = Expression<NegOp, Fraction<2, 3>>{};
    auto result = exactEval(expr);
    static_assert(decltype(result)::numerator == -2);
    static_assert(decltype(result)::denominator == 3);
  };

  "exactEval: Nested expressions"_test = [] {
    // (1/2 + 1/3) * 2/5 = 5/6 * 2/5 = 10/30 = 1/3
    auto inner = Expression<AddOp, Fraction<1, 2>, Fraction<1, 3>>{};
    auto expr = Expression<MulOp, decltype(inner), Fraction<2, 5>>{};
    auto result = exactEval(expr);
    static_assert(decltype(result)::numerator == 1);
    static_assert(decltype(result)::denominator == 3);
  };

  // =========================================================================
  // partialEvalExact tests
  // =========================================================================

  "partialEvalExact: Preserves symbolic structure"_test = [&] {
    auto expr = x + y;
    auto result = partialEvalExact(expr);
    // Should be unchanged (contains symbols)
    static_assert(isSame<decltype(result), decltype(expr)>);
  };

  "partialEvalExact: Evaluates ground exact subexpressions"_test = [&] {
    // Expression<AddOp, Fraction<1,2>, Fraction<1,3>> * x
    // The inner Expression should be collapsed to Fraction<5,6>
    auto inner = Expression<AddOp, Fraction<1, 2>, Fraction<1, 3>>{};
    auto expr = Expression<MulOp, decltype(inner), decltype(x)>{};
    auto result = partialEvalExact(expr);

    // Result should be Fraction<5,6> * x
    static_assert(is_expression_v<decltype(result)>);
    static_assert(isSame<get_op_t<decltype(result)>, MulOp>);

    auto lhs = result.template arg<0>();
    static_assert(is_fraction_v<decltype(lhs)>);
    static_assert(decltype(lhs)::numerator == 5);
    static_assert(decltype(lhs)::denominator == 6);
  };

  "partialEvalExact: Leaves non-exact expressions unchanged"_test = [&] {
    // sin(0) is ground but NOT exact (sin is not a rational operation)
    auto expr = Expression<SinOp, Fraction<0, 1>>{};
    auto result = partialEvalExact(expr);
    // Should be unchanged
    static_assert(isSame<decltype(result), decltype(expr)>);
  };

  "partialEvalExact: Mixed expression - evaluates exact parts only"_test = [&] {
    // (1/2 + 1/3) * sin(x)
    // The (1/2 + 1/3) is ground and exact → Fraction<5,6>
    // sin(x) is not ground, stays unchanged
    auto frac_expr = Expression<AddOp, Fraction<1, 2>, Fraction<1, 3>>{};
    auto sin_expr = Expression<SinOp, decltype(x)>{};
    auto expr = Expression<MulOp, decltype(frac_expr), decltype(sin_expr)>{};
    auto result = partialEvalExact(expr);

    // Result should be Fraction<5,6> * sin(x)
    static_assert(is_expression_v<decltype(result)>);
    auto lhs = result.template arg<0>();
    static_assert(is_fraction_v<decltype(lhs)>);
    static_assert(decltype(lhs)::numerator == 5);
    static_assert(decltype(lhs)::denominator == 6);
  };

  // =========================================================================
  // Integration with normal operators
  // =========================================================================

  "Direct Fraction arithmetic already evaluates"_test = [] {
    // With our closed Fraction operators, this is already Fraction<5,6>
    auto result = Fraction<1, 2>{} + Fraction<1, 3>{};
    static_assert(is_fraction_v<decltype(result)>);
    static_assert(decltype(result)::numerator == 5);
    static_assert(decltype(result)::denominator == 6);
  };

  "Integer division to Fraction already works"_test = [] {
    // This uses the IntegralConstant operator/ → Fraction
    auto result = Constant<2>{} / Constant<3>{};
    static_assert(is_fraction_v<decltype(result)>);
    static_assert(decltype(result)::numerator == 2);
    static_assert(decltype(result)::denominator == 3);
  };

  "_z suffix enables exact chains"_test = [&] {
    // 2_z / 3_z creates Fraction<2,1> / Fraction<3,1> = Fraction<2,3>
    auto frac = 2_z / 3_z;
    static_assert(is_fraction_v<decltype(frac)>);
    static_assert(decltype(frac)::numerator == 2);
    static_assert(decltype(frac)::denominator == 3);

    // Using in expression keeps the Fraction
    auto expr = frac * x;
    auto result = partialEvalExact(expr);
    // frac is already Fraction<2,3>, so result is Fraction<2,3> * x
    auto lhs = result.template arg<0>();
    static_assert(is_fraction_v<decltype(lhs)>);
    static_assert(decltype(lhs)::numerator == 2);
    static_assert(decltype(lhs)::denominator == 3);
  };

  return TestRegistry::result();
}
