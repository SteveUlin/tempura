// Tests for symbolic4/indexed/indexed_eval.h
#include "symbolic4/indexed/indexed_eval.h"
#include "symbolic4/indexed/dim.h"
#include "symbolic4/indexed/sum_over.h"
#include "symbolic4/core.h"
#include "unit.h"

#include <vector>

using namespace tempura;
using namespace tempura::symbolic4;

auto main() -> int {
  // ===========================================================================
  // DimIndexMap
  // ===========================================================================

  "DimIndexMap stores and retrieves indices"_test = [] {
    struct Obs {};
    DimIndexMap map;

    map.set<Obs>(5, 10);

    expectEq(5UL, map.get<Obs>());
    expectEq(10UL, map.getSize<Obs>());
    expectTrue(map.has<Obs>());
  };

  "DimIndexMap returns 0 for missing dimension"_test = [] {
    struct Obs {};
    struct Other {};
    DimIndexMap map;

    map.set<Obs>(5, 10);

    expectEq(0UL, map.get<Other>());
    expectFalse(map.has<Other>());
  };

  "DimIndexMap handles multiple dimensions"_test = [] {
    struct Countries {};
    struct Years {};

    DimIndexMap map;
    map.set<Countries>(2, 10);
    map.set<Years>(7, 20);

    expectEq(2UL, map.get<Countries>());
    expectEq(7UL, map.get<Years>());
    expectEq(10UL, map.getSize<Countries>());
    expectEq(20UL, map.getSize<Years>());
  };

  // ===========================================================================
  // evaluateIndexed with 1D IndexedSymbol
  // ===========================================================================

  "evaluateIndexed with simple 1D sum"_test = [] {
    struct Obs {};
    struct Id {};
    using Sym = IndexedSymbol<Id, Obs>;
    Sym x;

    // Sum of x[i] for i in [0, 3)
    auto sum_expr = sumOver<Obs>(x);

    std::vector<double> data = {1.0, 2.0, 3.0};
    IndexedBinding<Sym, 1> binding{std::span<const double>(data)};

    double result = evaluateIndexed(sum_expr, binding);
    expectNear(6.0, result, 1e-10);  // 1 + 2 + 3
  };

  "evaluateIndexed with scalar + indexed"_test = [] {
    struct Obs {};
    struct XId {};
    using XSym = IndexedSymbol<XId, Obs>;
    XSym x;
    Symbol<struct A> a;

    // Σᵢ (x[i] + a)
    auto sum_expr = sumOver<Obs>(x + a);

    std::vector<double> data = {1.0, 2.0, 3.0};
    IndexedBinding<XSym, 1> x_bind{std::span<const double>(data)};

    double result = evaluateIndexed(sum_expr, x_bind, a = 10.0);
    expectNear(36.0, result, 1e-10);  // (1+10) + (2+10) + (3+10) = 36
  };

  "evaluateIndexed with multiplication"_test = [] {
    struct Obs {};
    struct XId {};
    using XSym = IndexedSymbol<XId, Obs>;
    XSym x;
    Symbol<struct Scale> scale;

    // Σᵢ (x[i] * scale)
    auto sum_expr = sumOver<Obs>(x * scale);

    std::vector<double> data = {1.0, 2.0, 3.0};
    IndexedBinding<XSym, 1> x_bind{std::span<const double>(data)};

    double result = evaluateIndexed(sum_expr, x_bind, scale = 2.0);
    expectNear(12.0, result, 1e-10);  // (1*2) + (2*2) + (3*2) = 12
  };

  // ===========================================================================
  // evaluateIndexed with 2D IndexedSymbol
  // ===========================================================================

  "evaluateIndexed with 2D indexed symbol"_test = [] {
    struct Countries {};
    struct Years {};
    struct YId {};
    using YSym = IndexedSymbol<YId, Countries, Years>;
    YSym y;

    // Nested sum: Σ_c Σ_y y[c,y]
    auto sum_expr = sumOver<Countries>(sumOver<Years>(y));

    // 2 countries x 3 years
    std::vector<double> data = {1.0, 2.0, 3.0, 4.0, 5.0, 6.0};
    IndexedBinding<YSym, 2> binding{std::span<const double>(data), {2, 3}};

    double result = evaluateIndexed(sum_expr, binding);
    expectNear(21.0, result, 1e-10);  // 1+2+3+4+5+6
  };

  // ===========================================================================
  // evaluateIndexed with literals
  // ===========================================================================

  "evaluateIndexed with literal in expression"_test = [] {
    struct Obs {};
    struct XId {};
    using XSym = IndexedSymbol<XId, Obs>;
    XSym x;

    // Σᵢ (x[i] - 1.0)
    auto sum_expr = sumOver<Obs>(x - lit(1.0));

    std::vector<double> data = {2.0, 3.0, 4.0};
    IndexedBinding<XSym, 1> x_bind{std::span<const double>(data)};

    double result = evaluateIndexed(sum_expr, x_bind);
    expectNear(6.0, result, 1e-10);  // (2-1) + (3-1) + (4-1) = 6
  };

  // ===========================================================================
  // getDimensionSize
  // ===========================================================================

  "getDimensionSize extracts size from bindings"_test = [] {
    struct Obs {};
    struct XId {};
    using XSym = IndexedSymbol<XId, Obs>;

    std::vector<double> data = {1.0, 2.0, 3.0, 4.0, 5.0};
    IndexedBinding<XSym, 1> binding{std::span<const double>(data)};

    auto pack = BinderPack{binding};
    SizeT size = getDimensionSize<Obs>(pack);
    expectEq(5UL, size);
  };

  "getDimensionSize with mixed bindings"_test = [] {
    struct Obs {};
    struct XId {};
    using XSym = IndexedSymbol<XId, Obs>;
    Symbol<struct A> a;

    std::vector<double> data = {1.0, 2.0, 3.0};
    IndexedBinding<XSym, 1> x_bind{std::span<const double>(data)};

    auto pack = BinderPack{a = 5.0, x_bind};
    SizeT size = getDimensionSize<Obs>(pack);
    expectEq(3UL, size);
  };

  // ===========================================================================
  // Complex expressions
  // ===========================================================================

  "evaluateIndexed with squared terms"_test = [] {
    struct Obs {};
    struct XId {};
    using XSym = IndexedSymbol<XId, Obs>;
    XSym x;

    // Σᵢ x[i]²
    auto sum_expr = sumOver<Obs>(x * x);

    std::vector<double> data = {1.0, 2.0, 3.0};
    IndexedBinding<XSym, 1> x_bind{std::span<const double>(data)};

    double result = evaluateIndexed(sum_expr, x_bind);
    expectNear(14.0, result, 1e-10);  // 1 + 4 + 9 = 14
  };

  "evaluateIndexed with division"_test = [] {
    struct Obs {};
    struct XId {};
    using XSym = IndexedSymbol<XId, Obs>;
    XSym x;
    Symbol<struct Sigma> sigma;

    // Σᵢ x[i] / sigma
    auto sum_expr = sumOver<Obs>(x / sigma);

    std::vector<double> data = {2.0, 4.0, 6.0};
    IndexedBinding<XSym, 1> x_bind{std::span<const double>(data)};

    double result = evaluateIndexed(sum_expr, x_bind, sigma = 2.0);
    expectNear(6.0, result, 1e-10);  // (2/2) + (4/2) + (6/2) = 6
  };

  return TestRegistry::result();
}
