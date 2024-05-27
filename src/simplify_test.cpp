#include "simplify.h"

#include "symbolic.h"
#include "unit.h"

using namespace tempura;
using namespace tempura::symbolic;

auto main() -> int {
  "Flatten"_test = [] {
    auto a = SYMBOL();
    auto b = SYMBOL();
    auto c = SYMBOL();
    // Starts as a tree
    static_assert(
        std::is_same_v<decltype(a + b + c),
                       decltype(SymbolicExpression{Plus{}, SymbolicExpression{Plus{}, a, b}, c})>);
    // One level down
    static_assert(
        std::is_same_v<decltype(flatten(a + b + c)),
                       decltype(SymbolicExpression{Plus{}, a, b, c})>);
    // Multi Level
    static_assert(
        std::is_same_v<decltype(flatten((a + (b + (c + c))))),
                       decltype(SymbolicExpression{Plus{}, a, b, c, c})>);
  };

  "GetVariablePart"_test = [] {
    auto a = SYMBOL();
    auto π = Constant<3.14159>{};
    static_assert(match(getVariablePart(a), a));
    static_assert(match(getVariablePart(π * a), a));
    static_assert(match(getVariablePart(SymbolicExpression{Multiply{}, π, a, a}), a * a));
    static_assert(match(getVariablePart(SymbolicExpression{Plus{}, π, a}), π + a));
    show(getVariablePart(π), {});
    std::print("\n");
  };

  "Plus Sort"_test = [] {
    auto a = SYMBOL();
    auto b = SYMBOL();
    auto c = SYMBOL();
    auto π = Constant<3.14159>{};
    auto ϕ = Constant<1.61803>{};
    static_assert(match(sort(flatten(c + b + a + c)), flatten(a + b + c + c)));

    // Keeps stable order
    static_assert(match(sort(flatten(a + (π * a))), flatten(a + (π * a))));
    static_assert(match(sort(flatten((π * c) + b + (ϕ * c) + (π * a) + c)),
                             flatten((π * a) + b + (π * c) + (ϕ * c) + c)));
  };

  "Plus Merge"_test = [] {
    auto a = SYMBOL();
    auto b = SYMBOL();
    static_assert(match(merge(a + a), Constant<2>{} * a));
    static_assert(match(merge(flatten(a + a + a)), Constant<3>{} * a));
    static_assert(match(merge(flatten(a + a + a + b + b)), flatten(Constant<3>{} * a + Constant<2>{} * b)));
  };

  "Plus FlattenSortMerge"_test = [] {
    auto a = SYMBOL();
    auto b = SYMBOL();
    auto c = SYMBOL();
    static_assert(match(flattenSortMerge(a + c + b + a + b + a), flatten(Constant<3>{} * a + Constant<2>{} * b + c)));
  };

  "Plus remove zeros"_test = [] {
    auto a = SYMBOL();
    auto b = SYMBOL();
    static_assert(match(
      collapseIdentities(flatten(a + Constant<0>{} + b + Constant<0>{})), a + b));
    static_assert(match(
      collapseIdentities(flatten(a + Constant<0>{} + b + Constant<3>{})), flatten(a + b + Constant<3>{})));

    static_assert(match(collapseIdentities(Constant<0>{} + Constant<0>{}), Constant<0>{}));
  };

  return 0;
}

