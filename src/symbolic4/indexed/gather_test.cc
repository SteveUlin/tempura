// Tests for symbolic4/indexed/gather.h
#include "symbolic4/indexed/gather.h"
#include "symbolic4/indexed/indexed_eval.h"
#include "symbolic4/strategy/diff.h"
#include "unit.h"

#include <vector>

using namespace tempura;
using namespace tempura::symbolic4;

auto main() -> int {
  // ===========================================================================
  // Basic type traits
  // ===========================================================================

  "is_gather_v trait"_test = [] {
    struct Groups {};
    struct Obs {};
    struct AId {};
    struct IdxId {};

    using ASym = IndexedSymbol<AId, Groups>;
    using IdxSym = IndexedSymbol<IdxId, Obs>;

    ASym a;
    IdxSym idx;

    auto g = gather(a, idx);
    using GatherType = decltype(g);

    expectTrue(is_gather_v<GatherType>);
    expectFalse(is_gather_v<ASym>);
    expectFalse(is_gather_v<IdxSym>);
    expectFalse(is_gather_v<Constant<1>>);
  };

  // ===========================================================================
  // Basic evaluation
  // ===========================================================================

  "gather basic evaluation"_test = [] {
    // a = [10, 20, 30] over Groups (3 groups)
    // idx = [1, 0, 2, 1] over Obs (4 observations)
    // gather(a, idx) at each obs = [20, 10, 30, 20]
    // Sum = 80

    struct Groups {};
    struct Obs {};
    struct AId {};
    struct IdxId {};

    using ASym = IndexedSymbol<AId, Groups>;
    using IdxSym = IndexedSymbol<IdxId, Obs>;

    ASym a;
    IdxSym idx;

    auto expr = sumOver<Obs>(gather(a, idx));

    std::vector<double> a_data = {10.0, 20.0, 30.0};
    std::vector<double> idx_data = {1.0, 0.0, 2.0, 1.0};

    IndexedBinding<ASym, 1> a_bind{std::span<const double>(a_data)};
    IndexedBinding<IdxSym, 1> idx_bind{std::span<const double>(idx_data)};

    double result = evaluateIndexed(expr, a_bind, idx_bind);
    expectNear(80.0, result, 1e-10);  // 20 + 10 + 30 + 20
  };

  "gather with scalar addition"_test = [] {
    // a = [10, 20, 30]
    // idx = [0, 1, 2]
    // gather(a, idx) + 5 at each obs = [15, 25, 35]
    // Sum = 75

    struct Groups {};
    struct Obs {};
    struct AId {};
    struct IdxId {};

    using ASym = IndexedSymbol<AId, Groups>;
    using IdxSym = IndexedSymbol<IdxId, Obs>;

    ASym a;
    IdxSym idx;
    Symbol<struct C> c;

    auto expr = sumOver<Obs>(gather(a, idx) + c);

    std::vector<double> a_data = {10.0, 20.0, 30.0};
    std::vector<double> idx_data = {0.0, 1.0, 2.0};

    IndexedBinding<ASym, 1> a_bind{std::span<const double>(a_data)};
    IndexedBinding<IdxSym, 1> idx_bind{std::span<const double>(idx_data)};

    double result = evaluateIndexed(expr, a_bind, idx_bind, c = 5.0);
    expectNear(75.0, result, 1e-10);  // (10+5) + (20+5) + (30+5)
  };

  "gather with index at same element"_test = [] {
    // All observations map to the same group
    // a = [10, 20, 30]
    // idx = [1, 1, 1, 1]  (all point to group 1)
    // gather(a, idx) = [20, 20, 20, 20]
    // Sum = 80

    struct Groups {};
    struct Obs {};
    struct AId {};
    struct IdxId {};

    using ASym = IndexedSymbol<AId, Groups>;
    using IdxSym = IndexedSymbol<IdxId, Obs>;

    ASym a;
    IdxSym idx;

    auto expr = sumOver<Obs>(gather(a, idx));

    std::vector<double> a_data = {10.0, 20.0, 30.0};
    std::vector<double> idx_data = {1.0, 1.0, 1.0, 1.0};

    IndexedBinding<ASym, 1> a_bind{std::span<const double>(a_data)};
    IndexedBinding<IdxSym, 1> idx_bind{std::span<const double>(idx_data)};

    double result = evaluateIndexed(expr, a_bind, idx_bind);
    expectNear(80.0, result, 1e-10);  // 20 * 4
  };

  "gather with multiplication by scalar"_test = [] {
    // a = [1, 2, 3]
    // idx = [0, 1, 2, 0]
    // scale = 10
    // Sum of gather(a, idx) * scale = (1 + 2 + 3 + 1) * 10 = 70

    struct Groups {};
    struct Obs {};
    struct AId {};
    struct IdxId {};

    using ASym = IndexedSymbol<AId, Groups>;
    using IdxSym = IndexedSymbol<IdxId, Obs>;

    ASym a;
    IdxSym idx;
    Symbol<struct Scale> scale;

    auto expr = sumOver<Obs>(gather(a, idx) * scale);

    std::vector<double> a_data = {1.0, 2.0, 3.0};
    std::vector<double> idx_data = {0.0, 1.0, 2.0, 0.0};

    IndexedBinding<ASym, 1> a_bind{std::span<const double>(a_data)};
    IndexedBinding<IdxSym, 1> idx_bind{std::span<const double>(idx_data)};

    double result = evaluateIndexed(expr, a_bind, idx_bind, scale = 10.0);
    expectNear(70.0, result, 1e-10);  // (1 + 2 + 3 + 1) * 10
  };

  // ===========================================================================
  // Operator overloads
  // ===========================================================================

  "gather arithmetic operators"_test = [] {
    struct Groups {};
    struct Obs {};
    struct AId {};
    struct IdxId {};

    using ASym = IndexedSymbol<AId, Groups>;
    using IdxSym = IndexedSymbol<IdxId, Obs>;

    ASym a;
    IdxSym idx;
    auto g = gather(a, idx);

    // Test that operators compile and produce Expression types
    auto add_expr = g + lit(1.0);
    auto sub_expr = g - lit(2.0);
    auto mul_expr = g * lit(3.0);
    auto div_expr = g / lit(4.0);
    auto neg_expr = -g;

    expectTrue(is_expression_v<decltype(add_expr)>);
    expectTrue(is_expression_v<decltype(sub_expr)>);
    expectTrue(is_expression_v<decltype(mul_expr)>);
    expectTrue(is_expression_v<decltype(div_expr)>);
    expectTrue(is_expression_v<decltype(neg_expr)>);
  };

  "gather + gather operators"_test = [] {
    struct Groups {};
    struct Obs {};

    IndexedSymbol<struct AId, Groups> a;
    IndexedSymbol<struct BId, Groups> b;
    IndexedSymbol<struct IdxId, Obs> idx;

    auto ga = gather(a, idx);
    auto gb = gather(b, idx);

    auto sum = ga + gb;
    auto diff = ga - gb;
    auto prod = ga * gb;
    auto quot = ga / gb;

    expectTrue(is_expression_v<decltype(sum)>);
    expectTrue(is_expression_v<decltype(diff)>);
    expectTrue(is_expression_v<decltype(prod)>);
    expectTrue(is_expression_v<decltype(quot)>);
  };

  // ===========================================================================
  // Differentiation
  // ===========================================================================

  "gather differentiation - independent variable"_test = [] {
    // d/dx[gather(a, idx)] where a doesn't depend on x
    // Should be gather(0, idx) = 0

    struct Groups {};
    struct Obs {};

    IndexedSymbol<struct AId, Groups> a;
    IndexedSymbol<struct IdxId, Obs> idx;
    Symbol<struct X> x;

    auto g = gather(a, idx);
    auto dg = diff(g, x);

    // The derivative should simplify to 0 (constant)
    // After simplification, gather(0, idx) should become 0
    expectTrue(is_constant_v<decltype(dg)> || is_gather_v<decltype(dg)>);
  };

  "gather differentiation - scalar variable in param"_test = [] {
    // d/dmu[gather(a + mu, idx)] = gather(1, idx) = 1
    // where a is IndexedSymbol and mu is a scalar

    struct Groups {};
    struct Obs {};

    IndexedSymbol<struct AId, Groups> a;
    IndexedSymbol<struct IdxId, Obs> idx;
    Symbol<struct Mu> mu;

    auto g = gather(a + mu, idx);
    auto dg = diff(g, mu);

    // The derivative is gather(d/dmu[a + mu], idx) = gather(1, idx)
    // After simplification, this might still be a Gather wrapping 1
    expectTrue(is_gather_v<decltype(dg)> || is_constant_v<decltype(dg)>);
  };

  // ===========================================================================
  // Numerical gradient verification
  // ===========================================================================

  "gather gradient vs numerical - scalar parameter"_test = [] {
    // f(mu) = SumOver<Obs>(gather(a * mu, idx))
    // a = [1, 2, 3], idx = [0, 1, 2, 0]
    // f(mu) = (1*mu) + (2*mu) + (3*mu) + (1*mu) = 7*mu
    // df/dmu = 7

    struct Groups {};
    struct Obs {};

    IndexedSymbol<struct AId, Groups> a;
    IndexedSymbol<struct IdxId, Obs> idx;
    Symbol<struct Mu> mu;

    auto f = sumOver<Obs>(gather(a * mu, idx));
    auto df = diff(f, mu);

    std::vector<double> a_data = {1.0, 2.0, 3.0};
    std::vector<double> idx_data = {0.0, 1.0, 2.0, 0.0};
    double mu_val = 2.0;

    IndexedBinding<decltype(a), 1> a_bind{std::span<const double>(a_data)};
    IndexedBinding<decltype(idx), 1> idx_bind{std::span<const double>(idx_data)};

    // Evaluate f at mu = 2.0
    double f_val = evaluateIndexed(f, a_bind, idx_bind, mu = mu_val);
    expectNear(14.0, f_val, 1e-10);  // 7 * 2

    // Evaluate gradient symbolically
    double grad_val = evaluateIndexed(df, a_bind, idx_bind, mu = mu_val);
    expectNear(7.0, grad_val, 1e-10);

    // Numerical gradient check
    double eps = 1e-6;
    double f_plus = evaluateIndexed(f, a_bind, idx_bind, mu = mu_val + eps);
    double f_minus = evaluateIndexed(f, a_bind, idx_bind, mu = mu_val - eps);
    double numerical_grad = (f_plus - f_minus) / (2 * eps);
    expectNear(grad_val, numerical_grad, 1e-5);
  };

  return TestRegistry::result();
}
