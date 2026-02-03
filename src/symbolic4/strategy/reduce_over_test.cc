// Tests for ReduceOver integration with strategy system
#include "symbolic4/indexed/reduce_over.h"
#include "symbolic4/strategy/combinator.h"
#include "symbolic4/strategy/diff.h"
#include "symbolic4/strategy/recursive.h"
#include "symbolic4/strategy/rule.h"
#include "unit.h"

using namespace tempura;
using namespace tempura::symbolic4;

auto main() -> int {
  constexpr Symbol<struct X> x;
  constexpr Symbol<struct Y> y;
  struct Obs {};
  struct Groups {};

  // ===========================================================================
  // ReduceOver type traits
  // ===========================================================================

  "is_reduce_over_v recognizes all reduction types"_test = [&] {
    static_assert(is_reduce_over_v<ReduceOver<SumReduce, Obs, decltype(x)>>);
    static_assert(is_reduce_over_v<ReduceOver<ProdReduce, Obs, decltype(x)>>);
    static_assert(is_reduce_over_v<ReduceOver<MaxReduce, Obs, decltype(x)>>);
    static_assert(is_reduce_over_v<ReduceOver<LogSumExpReduce, Obs, decltype(x)>>);
    static_assert(!is_reduce_over_v<decltype(x)>);
    static_assert(!is_reduce_over_v<Constant<1>>);
  };

  "SumOver alias is backwards compatible"_test = [&] {
    auto sum = sumOver<Obs>(x);
    static_assert(is_sum_over_v<decltype(sum)>);
    static_assert(is_reduce_over_v<decltype(sum)>);
    static_assert(Symbolic<decltype(sum)>);

    // Type trait accessors work
    static_assert(isSame<sum_over_dim_tag_t<decltype(sum)>, Obs>);
    static_assert(isSame<reduce_over_dim_tag_t<decltype(sum)>, Obs>);
    static_assert(isSame<reduce_over_op_t<decltype(sum)>, SumReduce>);
  };

  "ProdReduce is not SumOver"_test = [&] {
    auto prod = prodOver<Obs>(x);
    static_assert(is_reduce_over_v<decltype(prod)>);
    static_assert(!is_sum_over_v<decltype(prod)>);
  };

  // ===========================================================================
  // ReduceOp identities and combine
  // ===========================================================================

  "SumReduce monoid"_test = [] {
    static_assert(SumReduce::identity() == 0.0);
    static_assert(SumReduce::combine(3.0, 4.0) == 7.0);
  };

  "ProdReduce monoid"_test = [] {
    static_assert(ProdReduce::identity() == 1.0);
    static_assert(ProdReduce::combine(3.0, 4.0) == 12.0);
  };

  "MaxReduce monoid"_test = [] {
    expectEq(MaxReduce::identity(), -std::numeric_limits<double>::infinity());
    expectEq(MaxReduce::combine(3.0, 7.0), 7.0);
    expectEq(MaxReduce::combine(-1.0, -5.0), -1.0);
  };

  "LogSumExpReduce monoid"_test = [] {
    // log(e^a + e^b) for a=0, b=0 → log(2) ≈ 0.693
    expectNear(LogSumExpReduce::combine(0.0, 0.0), std::log(2.0), 1e-10);
    // When one arg dominates: log(e^100 + e^0) ≈ 100
    expectNear(LogSumExpReduce::combine(100.0, 0.0), 100.0, 1e-10);
  };

  // ===========================================================================
  // Factory functions
  // ===========================================================================

  "factory functions create correct types"_test = [&] {
    auto s = sumOver<Obs>(x);
    auto p = prodOver<Obs>(x);
    auto m = maxOver<Obs>(x);
    auto l = logSumExpOver<Obs>(x);

    static_assert(isSame<reduce_over_op_t<decltype(s)>, SumReduce>);
    static_assert(isSame<reduce_over_op_t<decltype(p)>, ProdReduce>);
    static_assert(isSame<reduce_over_op_t<decltype(m)>, MaxReduce>);
    static_assert(isSame<reduce_over_op_t<decltype(l)>, LogSumExpReduce>);
  };

  // ===========================================================================
  // rebuild()
  // ===========================================================================

  "rebuild preserves ReduceOp and DimTag"_test = [&] {
    auto sum = sumOver<Obs>(x);
    auto rebuilt = sum.rebuild(y);

    static_assert(is_sum_over_v<decltype(rebuilt)>);
    static_assert(isSame<reduce_over_dim_tag_t<decltype(rebuilt)>, Obs>);
    // Inner expression changed from x to y
    static_assert(isSame<typename decltype(rebuilt)::expr_type, Symbol<struct Y>>);
  };

  // ===========================================================================
  // Operator overloads (all four ops + negation)
  // ===========================================================================

  "ReduceOver arithmetic operators"_test = [&] {
    auto sum = sumOver<Obs>(x);

    // ReduceOver op Symbolic
    auto a = sum + y;
    auto b = sum - y;
    auto c = sum * y;
    auto d = sum / y;
    static_assert(is_expression_v<decltype(a)>);
    static_assert(is_expression_v<decltype(b)>);
    static_assert(is_expression_v<decltype(c)>);
    static_assert(is_expression_v<decltype(d)>);

    // Symbolic op ReduceOver
    auto e = y + sum;
    auto f = y - sum;
    auto g = y * sum;
    auto h = y / sum;
    static_assert(is_expression_v<decltype(e)>);
    static_assert(is_expression_v<decltype(f)>);
    static_assert(is_expression_v<decltype(g)>);
    static_assert(is_expression_v<decltype(h)>);

    // Unary negation
    auto neg = -sum;
    static_assert(is_expression_v<decltype(neg)>);
  };

  "ReduceOver + ReduceOver"_test = [&] {
    auto s1 = sumOver<Obs>(x);
    auto s2 = prodOver<Groups>(y);

    auto a = s1 + s2;
    auto b = s1 - s2;
    auto c = s1 * s2;
    auto d = s1 / s2;
    static_assert(is_expression_v<decltype(a)>);
    static_assert(is_expression_v<decltype(b)>);
    static_assert(is_expression_v<decltype(c)>);
    static_assert(is_expression_v<decltype(d)>);
  };

  // ===========================================================================
  // All<S> traverses into ReduceOver
  // ===========================================================================

  "All applies strategy inside ReduceOver"_test = [&] {
    // A rule that replaces x with 0
    auto zero_x = rule(x, 0_c);
    auto sum = sumOver<Obs>(x);

    auto result = All<decltype(zero_x)>{zero_x}.apply(sum);
    // Should have applied rule to inner expr: SumOver<Obs, Constant<0>>
    static_assert(is_sum_over_v<decltype(result)>);
    static_assert(is_constant_v<typename decltype(result)::expr_type>);
  };

  "All applies strategy inside ProdOver"_test = [&] {
    auto zero_x = rule(x, 0_c);
    auto prod = prodOver<Obs>(x);

    auto result = All<decltype(zero_x)>{zero_x}.apply(prod);
    static_assert(is_reduce_over_v<decltype(result)>);
    static_assert(isSame<reduce_over_op_t<decltype(result)>, ProdReduce>);
  };

  // ===========================================================================
  // Innermost simplifies inside ReduceOver
  // ===========================================================================

  "Innermost simplifies inside ReduceOver"_test = [&] {
    // x + 0 → x rule
    auto zero_rule = rule(x + 0_c, x);
    auto strat = innermost(zero_rule);

    // SumOver<Obs, x + 0> should simplify to SumOver<Obs, x>
    auto sum = sumOver<Obs>(x + 0_c);
    auto result = strat.apply(sum);

    static_assert(is_sum_over_v<decltype(result)>);
    static_assert(is_atom_v<typename decltype(result)::expr_type>);
  };

  // ===========================================================================
  // Recursive recurses into ReduceOver
  // ===========================================================================

  "Recursive applies rules inside ReduceOver"_test = [&] {
    constexpr auto f_ = PatternVar<struct F_>{};
    constexpr auto g_ = PatternVar<struct G_>{};

    // Simple "double everything" recursive rule
    auto doubler = recursive(
        rrule(f_ + g_, rec(f_) + rec(g_))
      | rule(x, x + x)
      | rule(AnySymbol{}, 0_c)
      | rule(AnyConstant{}, 0_c)
    );

    // Apply to SumOver<Obs, x>
    auto sum = sumOver<Obs>(x);
    auto result = doubler.apply(sum);

    // Should get SumOver<Obs, x + x>
    static_assert(is_sum_over_v<decltype(result)>);
    static_assert(is_expression_v<typename decltype(result)::expr_type>);
  };

  "Recursive applies to ProdOver"_test = [&] {
    auto constant_rule = recursive(
        rule(x, 42_c)
      | rule(AnySymbol{}, 0_c)
    );

    auto prod = prodOver<Groups>(x);
    auto result = constant_rule.apply(prod);

    static_assert(is_reduce_over_v<decltype(result)>);
    static_assert(isSame<reduce_over_op_t<decltype(result)>, ProdReduce>);
    static_assert(is_constant_v<typename decltype(result)::expr_type>);
  };

  // ===========================================================================
  // differentiate() works through ReduceOver via Recursive
  // ===========================================================================

  "differentiate distributes through SumOver"_test = [&] {
    auto sum = sumOver<Obs>(x * x);
    auto result = differentiate(sum, x);

    // d/dx[Σ(x²)] = Σ(2x) = SumOver<Obs, x + x>
    static_assert(is_sum_over_v<decltype(result)>);
    // Inner expression should be the derivative of x*x = x + x
    static_assert(is_expression_v<typename decltype(result)::expr_type>);
  };

  return TestRegistry::result();
}
