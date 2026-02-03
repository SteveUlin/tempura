// Tests for symbolic4/indexed/sum_over.h
#include "symbolic4/indexed/sum_over.h"
#include "symbolic4/core.h"
#include "unit.h"

using namespace tempura;
using namespace tempura::symbolic4;

auto main() -> int {
  // ===========================================================================
  // SumOver type traits
  // ===========================================================================

  "SumOver is recognized by type trait"_test = [] {
    struct Obs {};
    Symbol<struct X> x;
    auto sum = sumOver<Obs>(x);

    static_assert(is_sum_over_v<decltype(sum)>);
    static_assert(!is_sum_over_v<Symbol<struct X>>);
  };

  "SumOver satisfies Symbolic"_test = [] {
    struct Obs {};
    Symbol<struct X> x;
    auto sum = sumOver<Obs>(x);

    static_assert(Symbolic<decltype(sum)>);
  };

  // ===========================================================================
  // SumOver construction
  // ===========================================================================

  "sumOver() factory creates SumOver"_test = [] {
    struct Observations {};
    Symbol<struct Theta> theta;

    auto sum = sumOver<Observations>(theta);

    static_assert(is_sum_over_v<decltype(sum)>);
    static_assert(std::is_same_v<typename decltype(sum)::dim_tag, Observations>);
  };

  "SumOver stores expression"_test = [] {
    struct D {};
    Symbol<struct X> x;
    auto sum = sumOver<D>(x);
    auto expr = sum.expr();

    static_assert(std::is_same_v<decltype(expr), Symbol<struct X>>);
  };

  // ===========================================================================
  // sum_over_dim_tag_t helper
  // ===========================================================================

  "sum_over_dim_tag_t extracts dimension"_test = [] {
    struct MyDim {};
    Symbol<struct X> x;
    using SumType = SumOver<MyDim, Symbol<struct X>>;

    static_assert(std::is_same_v<sum_over_dim_tag_t<SumType>, MyDim>);
  };

  // ===========================================================================
  // Arithmetic with SumOver
  // ===========================================================================

  "SumOver + Symbolic creates Expression"_test = [] {
    struct D {};
    Symbol<struct X> x;
    Symbol<struct Y> y;

    auto sum = sumOver<D>(x);
    auto result = sum + y;

    static_assert(Symbolic<decltype(result)>);
    static_assert(is_expression_v<decltype(result)>);
  };

  "Symbolic + SumOver creates Expression"_test = [] {
    struct D {};
    Symbol<struct X> x;
    Symbol<struct Y> y;

    auto sum = sumOver<D>(x);
    auto result = y + sum;

    static_assert(Symbolic<decltype(result)>);
    static_assert(is_expression_v<decltype(result)>);
  };

  "SumOver + SumOver creates Expression"_test = [] {
    struct D1 {};
    struct D2 {};
    Symbol<struct X> x;
    Symbol<struct Y> y;

    auto sum1 = sumOver<D1>(x);
    auto sum2 = sumOver<D2>(y);
    auto result = sum1 + sum2;

    static_assert(Symbolic<decltype(result)>);
    static_assert(is_expression_v<decltype(result)>);
  };

  // ===========================================================================
  // SumOver with complex expressions
  // ===========================================================================

  "SumOver with arithmetic expression"_test = [] {
    struct D {};
    Symbol<struct X> x;
    Symbol<struct Y> y;

    auto expr = x * y;
    auto sum = sumOver<D>(expr);

    static_assert(is_sum_over_v<decltype(sum)>);
    static_assert(is_expression_v<typename decltype(sum)::expr_type>);
  };

  return TestRegistry::result();
}
