// Tests for symbolic4/distributions/random_var.h
#include "symbolic4/distributions/random_var.h"
#include "symbolic4/distributions/wrappers.h"
#include "symbolic4/interpreter/eval.h"
#include "unit.h"

#include <cmath>

using namespace tempura;
using namespace tempura::symbolic4;

auto main() -> int {
  // ===========================================================================
  // RandomVar type traits
  // ===========================================================================

  "RandomVar satisfies IsRandomVar"_test = [] {
    auto mu = normal(lit(0), lit(1));
    static_assert(IsRandomVar<decltype(mu)>);
  };

  "RandomVar has correct types"_test = [] {
    auto mu = normal(lit(0.0), lit(1.0));
    using Node = decltype(mu);

    static_assert(
        std::is_same_v<typename Node::dist_type,
                       NormalDist<Literal<double>, Literal<double>>>);
  };

  // ===========================================================================
  // RandomVar::sym()
  // ===========================================================================

  "RandomVar::sym returns symbol"_test = [] {
    auto mu = normal(lit(0), lit(1));
    auto sym = mu.sym();

    static_assert(Symbolic<decltype(sym)>);
    static_assert(is_atom_v<decltype(sym)>);
  };

  "Different RVs have different symbols"_test = [] {
    auto mu = normal(lit(0), lit(1));
    auto sigma = halfNormal(lit(5));

    using MuSym = decltype(mu.sym());
    using SigmaSym = decltype(sigma.sym());

    static_assert(!std::is_same_v<MuSym, SigmaSym>);
  };

  // ===========================================================================
  // RandomVar::logProb()
  // ===========================================================================

  "RandomVar::logProb returns symbolic expression"_test = [] {
    auto mu = normal(lit(0), lit(1));
    auto lp = mu.logProb();

    static_assert(Symbolic<decltype(lp)>);
  };

  "RandomVar::logProb evaluates correctly"_test = [] {
    auto mu = normal(lit(0), lit(1));
    auto lp = mu.logProb();
    double val = evaluate(lp, mu = 0.5);

    // log N(0.5 | 0, 1) = -0.5 * 0.25 - log(1) - 0.919
    double expected = -0.5 * 0.25 - std::log(1.0) - 0.9189385332046727;
    expectNear(expected, val, 1e-10);
  };

  // ===========================================================================
  // RandomVar::unnormalizedLogProb()
  // ===========================================================================

  "RandomVar::unnormalizedLogProb evaluates correctly"_test = [] {
    auto mu = normal(lit(0), lit(1));
    auto lp = mu.unnormalizedLogProb();
    double val = evaluate(lp, mu = 0.5);

    // -0.5 * z^2 = -0.5 * 0.25 = -0.125
    expectNear(-0.125, val, 1e-10);
  };

  // ===========================================================================
  // Implicit conversion to symbol
  // ===========================================================================

  "RandomVar implicitly converts to symbol"_test = [] {
    auto mu = normal(lit(0), lit(10));
    // When mu is used in another distribution, it should convert to its symbol
    auto y = normal(mu, lit(1));

    // y's distribution should have mu's symbol as its mu parameter
    auto y_lp = y.logProb();
    double val = evaluate(y_lp, mu = 1.0, y = 1.5);
    expectTrue(std::isfinite(val));
  };

  // ===========================================================================
  // Binding syntax
  // ===========================================================================

  "RandomVar binding syntax works"_test = [] {
    auto mu = normal(lit(0), lit(1));
    auto binding = mu = 0.5;

    // Verify binding works in BinderPack
    auto lp = mu.logProb();
    double val = evaluate(lp, binding);
    expectTrue(std::isfinite(val));
  };

  // ===========================================================================
  // Factory functions
  // ===========================================================================

  "normal() creates RandomVar"_test = [] {
    auto mu = normal(lit(0), lit(1));
    static_assert(IsRandomVar<decltype(mu)>);
  };

  "halfNormal() creates RandomVar"_test = [] {
    auto sigma = halfNormal(lit(5));
    static_assert(IsRandomVar<decltype(sigma)>);
  };

  "beta() creates RandomVar"_test = [] {
    auto p = beta(lit(2), lit(5));
    static_assert(IsRandomVar<decltype(p)>);
  };

  "gamma() creates RandomVar"_test = [] {
    auto x = gamma(lit(3), lit(0.5));
    static_assert(IsRandomVar<decltype(x)>);
  };

  "exponential() creates RandomVar"_test = [] {
    auto x = exponential(lit(0.5));
    static_assert(IsRandomVar<decltype(x)>);
  };

  "uniform() creates RandomVar"_test = [] {
    auto x = uniform(lit(0), lit(1));
    static_assert(IsRandomVar<decltype(x)>);
  };

  "cauchy() creates RandomVar"_test = [] {
    auto x = cauchy(lit(0), lit(1));
    static_assert(IsRandomVar<decltype(x)>);
  };

  "bernoulli() creates RandomVar"_test = [] {
    auto x = bernoulli(lit(0.5));
    static_assert(IsRandomVar<decltype(x)>);
  };

  "studentT() creates RandomVar"_test = [] {
    auto x = studentT(lit(3), lit(0), lit(1));
    static_assert(IsRandomVar<decltype(x)>);
  };

  // ===========================================================================
  // Nested dependencies
  // ===========================================================================

  "Nested RandomVars work"_test = [] {
    auto mu = normal(lit(0), lit(10));
    auto sigma = halfNormal(lit(5));
    auto y = normal(mu, sigma);

    static_assert(IsRandomVar<decltype(y)>);

    auto lp = y.logProb();
    double val = evaluate(lp, mu = 1.0, sigma = 2.0, y = 1.5);

    double z = (1.5 - 1.0) / 2.0;
    double expected = -0.5 * z * z - std::log(2.0) - 0.9189385332046727;
    expectNear(expected, val, 1e-10);
  };

  // ===========================================================================
  // RandomVarSymbol type traits
  // ===========================================================================

  "RandomVar::sym returns RandomVarSymbol with Sample effect"_test = [] {
    auto mu = normal(lit(0), lit(1));
    auto sym = mu.sym();

    // The symbol should have Sample effect, not Free effect
    static_assert(is_random_var_atom_v<decltype(sym)>);
    static_assert(!std::is_same_v<get_effect_t<decltype(sym)>, Free>);
  };

  "RandomVarSymbol carries distribution in type"_test = [] {
    auto mu = normal(lit(0), lit(1));
    using SymType = decltype(mu.sym());

    // Can extract distribution type from the symbol
    using DistType = get_atom_distribution_t<SymType>;
    static_assert(std::is_same_v<DistType, NormalDist<Literal<int>, Literal<int>>>);
  };

  "RandomVarSymbol carries distribution instance"_test = [] {
    auto mu = normal(lit(5.0), lit(2.0));
    auto sym = mu.sym();

    // The distribution instance is stored in the symbol's effect
    auto& dist = sym.effect_.dist_;
    expectNear(dist.mu_.effect_.value, 5.0);
    expectNear(dist.sigma_.effect_.value, 2.0);
  };

  "freeSym returns plain Symbol for bindings"_test = [] {
    auto mu = normal(lit(0), lit(1));
    auto free_sym = mu.freeSym();

    // freeSym should return Atom<Id, Free> for use with bindings
    static_assert(std::is_same_v<get_effect_t<decltype(free_sym)>, Free>);
    static_assert(!is_random_var_atom_v<decltype(free_sym)>);
  };

  "RandomVarSymbol and freeSym have same Id"_test = [] {
    auto mu = normal(lit(0), lit(1));
    auto sym = mu.sym();
    auto free_sym = mu.freeSym();

    // Both should have the same identity type
    static_assert(std::is_same_v<get_id_t<decltype(sym)>, get_id_t<decltype(free_sym)>>);
  };

  "Expression with RandomVarSymbol evaluates correctly"_test = [] {
    auto mu = normal(lit(0), lit(1));
    auto sigma = halfNormal(lit(5));

    // Build expression using RandomVarSymbols
    auto expr = mu.sym() + sigma.sym();

    // Evaluate - bindings use freeSym types but evaluator maps RandomVarSymbol to it
    double val = evaluate(expr, mu = 3.0, sigma = 2.0);
    expectNear(val, 5.0, 1e-10);
  };

  return TestRegistry::result();
}
