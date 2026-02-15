// Tests for the value-based dim/plate/data/reduce API
#include "symbolic4/distributions/indexed_node.h"
#include "symbolic4/distributions/wrappers.h"
#include "symbolic4/indexed/data.h"
#include "symbolic4/indexed/indexed_eval.h"
#include "unit.h"

#include <vector>

using namespace tempura;
using namespace tempura::symbolic4;

auto main() -> int {
  // ===========================================================================
  // dim() basics
  // ===========================================================================

  "dim() produces unique types"_test = [] {
    auto d1 = dim();
    auto d2 = dim();
    static_assert(!std::is_same_v<decltype(d1), decltype(d2)>);
  };

  "dim() satisfies IsDim"_test = [] {
    auto d = dim();
    static_assert(IsDim<decltype(d)>);
    static_assert(is_dim_v<decltype(d)>);
  };

  "bare struct is not IsDim"_test = [] {
    struct Obs {};
    static_assert(!IsDim<Obs>);
    static_assert(!is_dim_v<Obs>);
  };

  "dim<N>() carries compile-time size"_test = [] {
    auto d = dim<8>();
    static_assert(dim_size_v<decltype(d)> == 8);
  };

  "dim() has kDynamic size"_test = [] {
    auto d = dim();
    static_assert(dim_size_v<decltype(d)> == kDynamic);
  };

  "bare struct has kDynamic size"_test = [] {
    struct Obs {};
    static_assert(dim_size_v<Obs> == kDynamic);
  };

  // ===========================================================================
  // plate(dist, dim) — value-based
  // ===========================================================================

  "plate(dist, dim) creates IndexedRandomVar"_test = [] {
    auto obs = dim();
    auto theta = plate(beta(2.0_c, 3.0_c), obs);

    static_assert(is_indexed_random_var_v<decltype(theta)>);
    static_assert(decltype(theta)::symbol_type::ndims == 1);
  };

  "plate(dist, dim1, dim2) creates 2D IndexedRandomVar"_test = [] {
    auto countries = dim();
    auto years = dim();
    auto y = plate(normal(0.0_c, 1.0_c), countries, years);

    using Node = decltype(y);
    static_assert(is_indexed_random_var_v<Node>);
    static_assert(Node::symbol_type::ndims == 2);
  };

  "plate(dist, dim) with bare struct backward compat"_test = [] {
    struct Obs {};
    auto theta = plate(normal(0.0_c, 1.0_c), Obs{});

    using Node = decltype(theta);
    static_assert(is_indexed_random_var_v<Node>);
    // The dim type in the symbol should be Obs
    static_assert(Node::symbol_type::template has_dim<Obs>);
  };

  "plate(indexed_rv, dim) appends dimension"_test = [] {
    auto countries = dim();
    auto years = dim();
    auto inner = plate(normal(0.0_c, 1.0_c), countries);
    auto y = plate(inner, years);

    using Node = decltype(y);
    static_assert(Node::symbol_type::ndims == 2);
  };

  "plate(dist, dim) logProb builds SumOver"_test = [] {
    auto obs = dim();
    auto theta = plate(normal(0.0_c, 1.0_c), obs);

    auto lp = theta.logProb();
    // Should be a ReduceOver (SumOver) wrapping the log-prob
    static_assert(is_reduce_over_v<decltype(lp)>);
  };

  // ===========================================================================
  // data(dim) — value-based
  // ===========================================================================

  "data(dim) creates IndexedData"_test = [] {
    auto obs = dim();
    auto n = data(obs);

    static_assert(is_indexed_data_v<decltype(n)>);
    static_assert(decltype(n)::ndims == 1);
  };

  "data(dim1, dim2) creates 2D IndexedData"_test = [] {
    auto obs = dim();
    auto features = dim();
    auto X = data(obs, features);

    static_assert(is_indexed_data_v<decltype(X)>);
    static_assert(decltype(X)::ndims == 2);
  };

  "data(dim) binding syntax"_test = [] {
    auto obs = dim();
    auto n = data(obs);

    std::vector<double> vals = {10.0, 20.0, 30.0};
    auto binding = n = indexed(vals);
    static_assert(is_indexed_binding_v<decltype(binding)>);
    expectEq(3UL, binding.size());
  };

  // ===========================================================================
  // reduce(expr, dim, op) — value-based
  // ===========================================================================

  "reduce(expr, dim, sum) creates SumReduce ReduceOver"_test = [] {
    auto obs = dim();
    struct Id {};
    IndexedSymbol<Id, decltype(obs)> x;

    auto r = reduce(x, obs, sum);
    static_assert(is_reduce_over_v<decltype(r)>);
    static_assert(is_sum_over_v<decltype(r)>);
  };

  "reduce(expr, dim, prod) creates ProdReduce ReduceOver"_test = [] {
    auto obs = dim();
    struct Id {};
    IndexedSymbol<Id, decltype(obs)> x;

    auto r = reduce(x, obs, prod);
    static_assert(is_reduce_over_v<decltype(r)>);
    static_assert(std::is_same_v<typename decltype(r)::reduce_op, ProdReduce>);
  };

  "sumOver(expr, dim) convenience"_test = [] {
    auto obs = dim();
    struct Id {};
    IndexedSymbol<Id, decltype(obs)> x;

    auto r = sumOver(x, obs);
    static_assert(is_sum_over_v<decltype(r)>);
  };

  // ===========================================================================
  // Evaluation: plate(dist, dim) + eval
  // ===========================================================================

  "plate(dist, dim) evaluateIndexed computes sum"_test = [] {
    auto obs = dim();
    struct XId {};
    using XSym = IndexedSymbol<XId, decltype(obs)>;
    XSym x;

    auto expr = sumOver(x * x, obs);

    std::vector<double> data = {1.0, 2.0, 3.0};
    IndexedBinding<XSym> x_bind{std::span<const double>(data)};
    auto pack = BinderPack{x_bind};

    double result = evaluateIndexed<BaseTerminals>(expr, pack);
    // 1² + 2² + 3² = 14
    expectNear(14.0, result, 1e-10);
  };

  // ===========================================================================
  // Cross-plate gather with dim objects
  // ===========================================================================

  "gather with dim-based plate"_test = [] {
    auto groups = dim();
    auto obs = dim();

    auto z = plate(normal(0.0_c, 1.0_c), groups);
    auto idx = data(obs);

    // z[idx] should create a Gather expression
    auto g = z[idx];
    static_assert(is_gather_v<decltype(g)>);
  };

  return TestRegistry::result();
}
