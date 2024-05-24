#include "symbolic.h"

#include "unit.h"

using namespace tempura;
using namespace tempura::symbolic;
using namespace std::string_view_literals;

auto main() -> int {

  "Symbols are not the same type"_test = [] {
    Symbol a;
    Symbol b;
    static_assert(!std::is_same_v<decltype(a), decltype(b)>);
  };

  "Constants"_test = [] {
    Constant<3> a;
    static_assert(std::is_same_v<decltype(a), Constant<3>>);
    static_assert(a.value == 3);

    Constant<2> b;
    static_assert(!std::is_same_v<decltype(a), decltype(b)>);
  };

  "Addition substitution"_test = [] {
    Symbol a;
    Symbol b;
    Symbol c;
    auto f = a + b + c;
    expectEq(6.F, f(Substitution{a = 1.F, b = 2.F, c = 3.F}));

    static_assert(6.F == f(Substitution{a = 1.F, b = 2.F, c = 3.F}));
  };

  "Subtraction substitution"_test = [] {
    Symbol a;
    Symbol b;
    Symbol c;
    auto f = a - b - c;
    expectEq(4.F, f(Substitution{a = 10.F, b = 5.F, c = 1.F}));

    static_assert(4.F == f(Substitution{a = 10.F, b = 5.F, c = 1.F}));
  };

  "Multiplies substitution"_test = [] {
    Symbol a;
    Symbol b;
    Symbol c;
    auto f = a * b * c;
    expectEq(12.F, f(Substitution{a = 2.F, b = 2.F, c = 3.F}));

    static_assert(12.F == f(Substitution{a = 2.F, b = 2.F, c = 3.F}));
  };

  "Divides substitution"_test = [] {
    Symbol a;
    Symbol b;
    Symbol c;
    auto f = a / b / c;
    expectEq(1.F, f(Substitution{a = 10.F, b = 5.F, c = 2.F}));

    static_assert(1.F == f(Substitution{a = 10.F, b = 5.F, c = 2.F}));
  };

  "Match Symbol"_test = [] {
    Symbol a;
    Symbol b;
    Symbol c;
    static_assert(match(a + b + c, a + b + c));
    // Order matters
    static_assert(!match(a + b + c, a + c + b));
    // Complex symbol
    auto f = sin(a * b + c);
    auto g = sin(a * b + c);
    static_assert(match(f, g));
  };

  "Match Constant"_test = [] {
    Constant<1> a;
    Constant<2> b;
    Constant<3> c;
    static_assert(match(a + b + c, a + b + c));
    // Order matters
    static_assert(!match(a + b + c, a + c + b));
    // Complex symbol
    auto f = sin(a * b + c);
    auto g = sin(a * b + c);
    static_assert(match(f, g));
  };

  "Match Any"_test = [] {
    Symbol a;
    Symbol b;
    Symbol c;
    static_assert(match(Any{}, Any{}));
    static_assert(match(a + b + c, Any{}));
    // SubSymbol Match
    static_assert(match(a + b + c, a + b + Any{}));
    // Structure matters
    static_assert(!match(a + b + c, a + Any{}));
    static_assert(!match(a + b + c, a + Any{} + b));
    static_assert(match(a + (b + c), a + Any{}));
    // Matcher can be first
    static_assert(match(Any{}, a + b + c));
    // Match Constant
    static_assert(match(Any{}, Constant<3>{}));
  };

  "Match AnyNTerms"_test = [] {
    Symbol a;
    Symbol b;
    Symbol c;
    // AnyNTerms only matches inside SymbolicExpression
    static_assert(!match(AnyNTerms{}, AnyNTerms{}));
    static_assert(!match(a + b + c, AnyNTerms{}));
    // Works inside SymbolicExpression
    static_assert(match(a + b + c, SymbolicExpression<Plus, AnyNTerms>{}));
    static_assert(match(SymbolicExpression<Plus, AnyNTerms>{}, a + b + c));
    static_assert(
        match(SymbolicExpression<Plus, AnyNTerms>{}, SymbolicExpression<Plus, AnyNTerms>{}));
  };

  "Match AnyNTerms"_test = [] {
    Symbol a;
    Symbol b;
    Symbol c;
    // AnyNTerms only matches inside SymbolicExpression
    static_assert(!match(AnyNTerms{}, AnyNTerms{}));
    static_assert(!match(a + b + c, AnyNTerms{}));
    // Works inside SymbolicExpression
    static_assert(match(a + b + c, SymbolicExpression<Plus, AnyNTerms>{}));
    static_assert(match(SymbolicExpression<Plus, AnyNTerms>{}, a + b + c));
    static_assert(
        match(SymbolicExpression<Plus, AnyNTerms>{}, SymbolicExpression<Plus, AnyNTerms>{}));
  };

  "Match AnyConstant"_test = [] {
    Symbol a;
    Constant<3> b;
    static_assert(match(AnyConstant{}, Constant<3>{}));
    static_assert(match(Constant<3>{}, AnyConstant{}));

    static_assert(match(a + b, a + AnyConstant{}));
    static_assert(!match(a + b, AnyConstant{} + a));
  };

  "Match AnySymbol"_test = [] {
    Symbol a;
    Constant<3> b;
    static_assert(match(AnySymbol{}, Symbol{}));
    static_assert(match(a, AnySymbol{}));

    static_assert(!match(AnySymbol{}, Constant<3>{}));
    static_assert(match(a + b, AnySymbol{} + b));
    static_assert(!match(a + b, b + AnySymbol{}));
  };

  "Match AnySymbolicExpression"_test = [] {
    Symbol a;
    Constant<3> b;
    static_assert(!match(AnySymbolicExpression{}, a));
    static_assert(!match(AnySymbolicExpression{}, b));
    static_assert(match(AnySymbolicExpression{}, a + b));
    static_assert(match(a + b, AnySymbolicExpression{}));
    static_assert(match((a + b) + b, AnySymbolicExpression{} + b));

    static_assert(!match(AnySymbol{}, Constant<3>{}));
    static_assert(match(a + b, AnySymbol{} + b));
    static_assert(!match(a + b, b + AnySymbol{}));
  };

  Symbol a;
  Symbol ω;
  Symbol t;
  Symbol ϕ;

  auto f = a * sin(ω * t + ϕ);
  std::print("{}\n", f(Substitution{a = 1.f, ω = 2.f, t = 3.f, ϕ = 4.f}));

  auto g = a * sin(ω * t + ϕ);

  static_assert(match(f, g));
  static_assert(match(f, Any{}));

  auto h = a + ω + t + ϕ;

  // std::print("{}\n", type_info<decltype(flatten<Plus>(h))>());

  // std::print("{}\n", type_info<decltype(f)>());
  // show(h, Substitution{a = "a", ω = "ω", t = "t", ϕ = "ϕ"});
  // std::print("\n");
  // show(flatten(h), Substitution{a = "a", ω = "ω", t = "t", ϕ = "ϕ"});
  // std::print("\n");
  // show(mkSymbolicExpression(Plus{}, sort(cmpTypeId{}, flatten(h).terms())),
  //      Substitution{a = "a", ω = "ω", t = "t", ϕ = "ϕ"});
  // std::print("\n");
  // show(f, Substitution{a = "a", ω = "ω", t = "t", ϕ = "ϕ"});
  // std::print("\n");
  return 0;
}
