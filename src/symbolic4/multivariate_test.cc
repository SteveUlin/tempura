// Tests for symbolic4/distributions/multivariate.h - Multivariate distributions
#include "symbolic4/distributions/multivariate.h"
#include "symbolic4/interpreter/eval.h"
#include "symbolic4/operators.h"
#include "unit.h"

#include <cmath>

using namespace tempura;
using namespace tempura::symbolic4;

auto main() -> int {
  // ===========================================================================
  // VectorSymbol
  // ===========================================================================

  "VectorSymbol has correct dimension"_test = [] {
    struct XId {};
    using Vec2 = VectorSymbol<XId, 2>;
    using Vec3 = VectorSymbol<XId, 3>;

    static_assert(Vec2::dim == 2);
    static_assert(Vec3::dim == 3);
  };

  "VectorSymbol is symbolic"_test = [] {
    struct XId {};
    using Vec = VectorSymbol<XId, 3>;

    static_assert(is_vector_symbol_v<Vec>);
    static_assert(!is_vector_symbol_v<Symbol<XId>>);
  };

  "VectorSymbol components have correct indices"_test = [] {
    struct XId {};
    using Vec = VectorSymbol<XId, 3>;

    auto x0 = Vec::component<0>();
    auto x1 = Vec::component<1>();
    auto x2 = Vec::component<2>();

    static_assert(is_vector_component_v<decltype(x0)>);
    static_assert(is_vector_component_v<decltype(x1)>);
    static_assert(is_vector_component_v<decltype(x2)>);

    static_assert(decltype(x0)::index == 0);
    static_assert(decltype(x1)::index == 1);
    static_assert(decltype(x2)::index == 2);

    // All components share same id and dim
    static_assert(decltype(x0)::dim == 3);
    static_assert(decltype(x1)::dim == 3);
  };

  "VectorComponent is not VectorSymbol"_test = [] {
    struct XId {};
    using Vec = VectorSymbol<XId, 2>;
    auto x0 = Vec::component<0>();

    static_assert(!is_vector_symbol_v<decltype(x0)>);
    static_assert(is_vector_component_v<decltype(x0)>);
  };

  // ===========================================================================
  // VectorBinding
  // ===========================================================================

  "VectorBinding stores values"_test = [] {
    struct XId {};
    using Vec = VectorSymbol<XId, 3>;
    VectorBinding<Vec> binding{1.0, 2.0, 3.0};

    expectNear(1.0, binding[0], 1e-10);
    expectNear(2.0, binding[1], 1e-10);
    expectNear(3.0, binding[2], 1e-10);
  };

  "VectorBinding get<I> access"_test = [] {
    struct XId {};
    using Vec = VectorSymbol<XId, 3>;
    VectorBinding<Vec> binding{10.0, 20.0, 30.0};

    expectNear(10.0, binding.get<0>(), 1e-10);
    expectNear(20.0, binding.get<1>(), 1e-10);
    expectNear(30.0, binding.get<2>(), 1e-10);
  };

  "VectorBinding from array"_test = [] {
    struct XId {};
    using Vec = VectorSymbol<XId, 2>;
    std::array<double, 2> arr = {5.0, 6.0};
    VectorBinding<Vec> binding{arr};

    expectNear(5.0, binding[0], 1e-10);
    expectNear(6.0, binding[1], 1e-10);
  };

  "VectorBinding type traits"_test = [] {
    struct XId {};
    using Vec = VectorSymbol<XId, 2>;
    using Binding = VectorBinding<Vec>;

    static_assert(is_vector_binding_v<Binding>);
    static_assert(!is_vector_binding_v<Vec>);
    static_assert(Binding::dim == 2);
  };

  // ===========================================================================
  // DiagonalMultivariateNormalDist
  // ===========================================================================

  "DiagonalMVN creates log-prob expression"_test = [] {
    struct XId {};
    using Vec = VectorSymbol<XId, 2>;

    auto mu = std::tuple{lit(0.0), lit(0.0)};
    auto sigma = std::tuple{lit(1.0), lit(1.0)};
    auto dist = diagMvNormal(mu, sigma);

    auto lp = dist.logProbFor(Vec{});

    // Should be symbolic
    static_assert(Symbolic<decltype(lp)>);
  };

  "DiagonalMVN dimension must match"_test = [] {
    struct XId {};
    using Vec2 = VectorSymbol<XId, 2>;
    using Vec3 = VectorSymbol<XId, 3>;

    auto mu2 = std::tuple{lit(0.0), lit(0.0)};
    auto sigma2 = std::tuple{lit(1.0), lit(1.0)};
    auto dist2 = diagMvNormal(mu2, sigma2);

    // This should work
    [[maybe_unused]] auto lp2 = dist2.logProbFor(Vec2{});

    // Vec3 with dist2 would fail (3 != 2) - can't test at compile time easily
    // since it's a requires clause
  };

  "DiagonalMVN with scalar symbols"_test = [] {
    struct XId {};
    using Vec = VectorSymbol<XId, 2>;

    Symbol<struct Mu1> mu1;
    Symbol<struct Mu2> mu2;
    Symbol<struct S1> s1;
    Symbol<struct S2> s2;

    auto mu = std::tuple{mu1, mu2};
    auto sigma = std::tuple{s1, s2};
    auto dist = diagMvNormal(mu, sigma);

    auto lp = dist.logProbFor(Vec{});

    // Log-prob should depend on all symbols
    static_assert(Symbolic<decltype(lp)>);
  };

  // ===========================================================================
  // Helper factories
  // ===========================================================================

  "mvNormalMean creates tuple"_test = [] {
    Symbol<struct A> a;
    Symbol<struct B> b;

    auto mu = mvNormalMean(a, b);
    static_assert(std::tuple_size_v<decltype(mu)> == 2);
  };

  "mvNormalSigma creates tuple"_test = [] {
    auto sigma = mvNormalSigma(lit(1.0), lit(2.0), lit(3.0));
    static_assert(std::tuple_size_v<decltype(sigma)> == 3);
  };

  // ===========================================================================
  // VectorComponent in expressions
  // ===========================================================================

  "VectorComponent can be used in expressions"_test = [] {
    struct XId {};
    using Vec = VectorSymbol<XId, 2>;

    auto x0 = Vec::component<0>();
    auto x1 = Vec::component<1>();

    // Should be able to form expressions
    auto sum = x0 + x1;
    auto prod = x0 * x1;
    auto sq = x0 * x0;

    static_assert(Symbolic<decltype(sum)>);
    static_assert(Symbolic<decltype(prod)>);
    static_assert(Symbolic<decltype(sq)>);
  };

  "VectorComponent mixed with scalar symbols"_test = [] {
    struct XId {};
    using Vec = VectorSymbol<XId, 2>;
    Symbol<struct A> a;

    auto x0 = Vec::component<0>();

    // Mix vector component with scalar
    auto expr = a * x0 + lit(1.0);

    static_assert(Symbolic<decltype(expr)>);
  };

  return TestRegistry::result();
}
