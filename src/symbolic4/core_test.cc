// Tests for core.h - fundamental symbolic types
#include "symbolic4/core.h"
#include "symbolic4/constants.h"  // For zero, one, two, etc.
#include "symbolic4/operators.h"  // For AddOp, MulOp
#include "unit.h"

using namespace tempura;
using namespace tempura::symbolic4;

auto main() -> int {
  "Symbol is an Atom with Free effect"_test = [] {
    Symbol<struct X> x;
    static_assert(is_atom_v<decltype(x)>);
    static_assert(std::is_same_v<get_effect_t<decltype(x)>, Free>);
    static_assert(has_identity_v<decltype(x)>);
  };

  "different Symbols have different identities"_test = [] {
    Symbol<struct X> x;
    Symbol<struct Y> y;
    static_assert(!std::is_same_v<get_id_t<decltype(x)>, get_id_t<decltype(y)>>);
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
    // c<V> creates compile-time constants via template variable
    static_assert(c<0>.value == 0);
    static_assert(c<1>.value == 1);
    static_assert(c<2>.value == 2);
    static_assert(c<-1>.value == -1);
    static_assert(c<42>.value == 42);
    static_assert(c<-100>.value == -100);
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

    static_assert(is_terminal_v<decltype(x)>);
    static_assert(is_terminal_v<decltype(c)>);
    static_assert(is_terminal_v<decltype(f)>);
  };

  "BinderPack lookup"_test = [] {
    Symbol<struct X> x;
    Symbol<struct Y> y;

    auto bindings = BinderPack{x = 3.0, y = 5.0};
    expectNear(bindings[x], 3.0);
    expectNear(bindings[y], 5.0);
  };

  "Atom effects and DeterministicVar aliases"_test = [] {
    // Sample effect for random variables
    struct DummyDist {};
    using SampleAtom = Atom<struct Id1, Sample<DummyDist>>;
    static_assert(is_atom_v<SampleAtom>);
    static_assert(has_identity_v<SampleAtom>);

    // DeterministicVar for computed values
    Symbol<struct X> x;
    auto sum = x + x;
    using DV = DeterministicVar<struct Id2, decltype(sum)>;
    static_assert(is_atom_v<DV>);
    static_assert(has_identity_v<DV>);
  };

  // ===========================================================================
  // Sample effect and random variable traits
  // ===========================================================================

  "is_sample_effect_v recognizes Sample effect"_test = [] {
    struct DummyDist {};
    static_assert(is_sample_effect_v<Sample<DummyDist>>);
    static_assert(!is_sample_effect_v<Free>);
    static_assert(!is_sample_effect_v<Compute<int>>);
  };

  "is_random_var_atom_v recognizes atoms with Sample effect"_test = [] {
    struct DummyDist {};
    using RVAtom = Atom<struct Id1, Sample<DummyDist>>;
    using FreeAtom = Atom<struct Id1, Free>;
    using ConstAtom = Constant<3.14>;

    static_assert(is_random_var_atom_v<RVAtom>);
    static_assert(!is_random_var_atom_v<FreeAtom>);
    static_assert(!is_random_var_atom_v<ConstAtom>);
  };

  "get_distribution_t extracts distribution from Sample effect"_test = [] {
    struct MyDist {
      int param;
    };
    using EffectType = Sample<MyDist>;
    static_assert(std::is_same_v<get_distribution_t<EffectType>, MyDist>);
  };

  "get_atom_distribution_t extracts distribution from atom"_test = [] {
    struct MyDist {
      double x;
    };
    using RVAtom = Atom<struct MyId, Sample<MyDist>>;
    static_assert(std::is_same_v<get_atom_distribution_t<RVAtom>, MyDist>);
  };

  "Sample effect carries distribution instance"_test = [] {
    struct MyDist {
      int value;
      constexpr MyDist(int v) : value{v} {}
    };
    constexpr Sample<MyDist> eff{MyDist{42}};
    static_assert(eff.dist_.value == 42);

    // Atom with Sample effect stores the distribution
    constexpr Atom<struct Id, Sample<MyDist>> atom{Sample<MyDist>{MyDist{99}}};
    static_assert(atom.effect_.dist_.value == 99);
  };

  return TestRegistry::result();
}
