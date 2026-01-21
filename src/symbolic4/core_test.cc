// Tests for core.h - fundamental symbolic types
#include "symbolic4/core.h"
#include "symbolic4/operators.h"  // For AddOp, MulOp, convenience constants
#include "unit.h"

using namespace tempura;
using namespace tempura::symbolic4;

auto main() -> int {
  "Symbol is an Atom with Lookup strategy"_test = [] {
    Symbol<struct X> x;
    static_assert(is_atom_v<decltype(x)>);
    static_assert(std::is_same_v<get_strategy_t<decltype(x)>, Lookup>);
    static_assert(has_identity_v<decltype(x)>);
  };

  "different Symbols have different identities"_test = [] {
    Symbol<struct X> x;
    Symbol<struct Y> y;
    static_assert(!std::is_same_v<get_id_t<decltype(x)>, get_id_t<decltype(y)>>);
  };

  "Literal has void identity"_test = [] {
    auto l = Literal<double>{Intrinsic<double>{3.14}};
    static_assert(is_atom_v<decltype(l)>);
    static_assert(is_literal_v<decltype(l)>);
    static_assert(!has_identity_v<decltype(l)>);
  };

  "Constant is symbolic"_test = [] {
    Constant<42> c;
    static_assert(Symbolic<decltype(c)>);
    static_assert(is_constant_v<decltype(c)>);
    static_assert(c.value == 42);
  };

  "negative Constant"_test = [] {
    Constant<-7> neg;
    static_assert(is_constant_v<decltype(neg)>);
    static_assert(neg.value == -7);
  };

  "Fraction reduces to lowest terms"_test = [] {
    static_assert(Fraction<4, 8>::numerator == 1);
    static_assert(Fraction<4, 8>::denominator == 2);
    static_assert(Fraction<-6, 9>::numerator == -2);
    static_assert(Fraction<-6, 9>::denominator == 3);
    static_assert(Fraction<6, -9>::numerator == -2);
    static_assert(Fraction<6, -9>::denominator == 3);
  };

  "Fraction with denominator 1"_test = [] {
    static_assert(Fraction<5, 1>::numerator == 5);
    static_assert(Fraction<5, 1>::denominator == 1);
  };

  "convenience constants"_test = [] {
    static_assert(zero.value == 0);
    static_assert(one.value == 1);
    static_assert(two.value == 2);
    static_assert(neg_one.value == -1);
    static_assert(c<42>.value == 42);
    static_assert(c<-100>.value == -100);
  };

  "lit helper creates Literal"_test = [] {
    auto l = lit(3.14);
    static_assert(is_literal_v<decltype(l)>);
    expectNear(l.strategy_.value, 3.14);
  };

  "Expression stores operator and args"_test = [] {
    Symbol<struct X> x;
    Symbol<struct Y> y;
    auto expr = x + y;  // Uses operator overload to create Expression

    static_assert(is_expression_v<decltype(expr)>);
    static_assert(std::is_same_v<get_op_t<decltype(expr)>, AddOp>);
    static_assert(decltype(expr)::arity == 2);
  };

  "Expression arg access"_test = [] {
    Symbol<struct X> x;
    Constant<5> five;
    auto expr = x * five;  // Uses operator overload

    auto lhs = expr.arg<0>();
    auto rhs = expr.arg<1>();
    static_assert(is_atom_v<decltype(lhs)>);
    static_assert(is_constant_v<decltype(rhs)>);
  };

  "terminal classification"_test = [] {
    Symbol<struct X> x;
    Constant<1> c;
    Fraction<1, 2> f;
    auto l = Literal<int>{Intrinsic<int>{42}};

    static_assert(is_terminal_v<decltype(x)>);
    static_assert(is_terminal_v<decltype(c)>);
    static_assert(is_terminal_v<decltype(f)>);
    static_assert(is_terminal_v<decltype(l)>);
  };

  "BinderPack lookup"_test = [] {
    Symbol<struct X> x;
    Symbol<struct Y> y;

    auto bindings = BinderPack{x = 3.0, y = 5.0};
    expectNear(bindings[x], 3.0);
    expectNear(bindings[y], 5.0);
  };

  "RandomVar and DeterministicVar aliases"_test = [] {
    struct DummyDist {};
    using RV = RandomVar<struct Id1, DummyDist>;
    static_assert(is_atom_v<RV>);
    static_assert(has_identity_v<RV>);

    Symbol<struct X> x;
    auto sum = x + x;
    using DV = DeterministicVar<struct Id2, decltype(sum)>;
    static_assert(is_atom_v<DV>);
    static_assert(has_identity_v<DV>);
  };

  return TestRegistry::result();
}
