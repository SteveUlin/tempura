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
    auto mu = normal(0_c, 1_c);
    static_assert(IsRandomVar<decltype(mu)>);
  };

  "RandomVar has correct types"_test = [] {
    auto mu = normal(0_c, 1_c);
    using Node = decltype(mu);

    static_assert(
        std::is_same_v<typename Node::dist_type,
                       NormalDist<Constant<0.0>, Constant<1.0>>>);
  };

  // ===========================================================================
  // RandomVar::sym()
  // ===========================================================================

  "RandomVar::sym returns symbol"_test = [] {
    auto mu = normal(0_c, 1_c);
    auto sym = mu.sym();

    static_assert(Symbolic<decltype(sym)>);
    static_assert(is_atom_v<decltype(sym)>);
  };

  "Different RVs have different symbols"_test = [] {
    auto mu = normal(0_c, 1_c);
    auto sigma = halfNormal(5_c);

    using MuSym = decltype(mu.sym());
    using SigmaSym = decltype(sigma.sym());

    static_assert(!std::is_same_v<MuSym, SigmaSym>);
  };

  // ===========================================================================
  // RandomVar::logProb()
  // ===========================================================================

  "RandomVar::logProb returns symbolic expression"_test = [] {
    auto mu = normal(0_c, 1_c);
    auto lp = mu.logProb();

    static_assert(Symbolic<decltype(lp)>);
  };

  "RandomVar::logProb evaluates correctly"_test = [] {
    auto mu = normal(0_c, 1_c);
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
    auto mu = normal(0_c, 1_c);
    auto lp = mu.unnormalizedLogProb();
    double val = evaluate(lp, mu = 0.5);

    // -0.5 * z^2 = -0.5 * 0.25 = -0.125
    expectNear(-0.125, val, 1e-10);
  };

  // ===========================================================================
  // Implicit conversion to symbol
  // ===========================================================================

  "RandomVar implicitly converts to symbol"_test = [] {
    auto mu = normal(0_c, 10_c);
    // When mu is used in another distribution, it should convert to its symbol
    auto y = normal(mu, 1_c);

    // y's distribution should have mu's symbol as its mu parameter
    auto y_lp = y.logProb();
    double val = evaluate(y_lp, mu = 1.0, y = 1.5);
    expectTrue(std::isfinite(val));
  };

  // ===========================================================================
  // Binding syntax
  // ===========================================================================

  "RandomVar binding syntax works"_test = [] {
    auto mu = normal(0_c, 1_c);
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
    auto mu = normal(0_c, 1_c);
    static_assert(IsRandomVar<decltype(mu)>);
  };

  "halfNormal() creates RandomVar"_test = [] {
    auto sigma = halfNormal(5_c);
    static_assert(IsRandomVar<decltype(sigma)>);
  };

  "beta() creates RandomVar"_test = [] {
    auto p = beta(2_c, 5_c);
    static_assert(IsRandomVar<decltype(p)>);
  };

  "gamma() creates RandomVar"_test = [] {
    auto x = gamma(3_c, 0.5_c);
    static_assert(IsRandomVar<decltype(x)>);
  };

  "exponential() creates RandomVar"_test = [] {
    auto x = exponential(0.5_c);
    static_assert(IsRandomVar<decltype(x)>);
  };

  "uniform() creates RandomVar"_test = [] {
    auto x = uniform(0_c, 1_c);
    static_assert(IsRandomVar<decltype(x)>);
  };

  "cauchy() creates RandomVar"_test = [] {
    auto x = cauchy(0_c, 1_c);
    static_assert(IsRandomVar<decltype(x)>);
  };

  "bernoulli() creates RandomVar"_test = [] {
    auto x = bernoulli(0.5_c);
    static_assert(IsRandomVar<decltype(x)>);
  };

  "studentT() creates RandomVar"_test = [] {
    auto x = studentT(3_c, 0_c, 1_c);
    static_assert(IsRandomVar<decltype(x)>);
  };

  // ===========================================================================
  // Nested dependencies
  // ===========================================================================

  "Nested RandomVars work"_test = [] {
    auto mu = normal(0_c, 10_c);
    auto sigma = halfNormal(5_c);
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

  "RandomVar::sym returns plain Free symbol"_test = [] {
    auto mu = normal(0_c, 1_c);
    auto sym = mu.sym();

    // sym() now returns Symbol<Id> (Atom<Id, Free>) — no Sample effect
    static_assert(std::is_same_v<get_effect_t<decltype(sym)>, Free>);
    static_assert(!is_random_var_atom_v<decltype(sym)>);
  };

  "freeSym and sym return the same type"_test = [] {
    auto mu = normal(0_c, 1_c);
    static_assert(std::is_same_v<decltype(mu.sym()), decltype(mu.freeSym())>);
  };

  "RandomVar preserves distribution access via dist()"_test = [] {
    auto mu = normal(5_c, 2_c);

    // Distribution is accessible via dist(), not via the symbol
    auto& dist = mu.dist();
    static_assert(dist.mu().value == 5.0);
    static_assert(dist.sigma().value == 2.0);
  };

  "sym and freeSym are identical"_test = [] {
    auto mu = normal(0_c, 1_c);
    // Both return the same Symbol<Id> type
    static_assert(std::is_same_v<decltype(mu.sym()), decltype(mu.freeSym())>);
    static_assert(std::is_same_v<get_id_t<decltype(mu.sym())>, get_id_t<decltype(mu.freeSym())>>);
  };

  "Expression with RandomVar symbols evaluates directly"_test = [] {
    auto mu = normal(0_c, 1_c);
    auto sigma = halfNormal(5_c);

    // Build expression using Free symbols from RandomVars
    auto expr = mu.sym() + sigma.sym();

    // Bind constrained values directly — no inverse transform needed
    double val = evaluate(expr, mu = 3.0, sigma = 2.0);
    expectNear(val, 5.0, 1e-10);
  };

  return TestRegistry::result();
}
