// Tests for symbolic4/distributions/indexed_node.h
#include "symbolic4/distributions/indexed_node.h"
#include "symbolic4/distributions/wrappers.h"
#include "symbolic4/indexed/indexed_eval.h"
#include "unit.h"

#include <cmath>
#include <vector>

using namespace tempura;
using namespace tempura::symbolic4;

auto main() -> int {
  // ===========================================================================
  // IndexedStochasticNode type traits
  // ===========================================================================

  "plate creates IndexedStochasticNode"_test = [] {
    struct Obs {};
    auto theta = plate<Obs>(beta(lit(2.0), lit(3.0)));

    static_assert(is_indexed_random_var_v<decltype(theta)>);
    static_assert(IsIndexedRandomVar<decltype(theta)>);
  };

  "IndexedStochasticNode has correct types"_test = [] {
    struct Obs {};
    auto theta = plate<Obs>(normal(lit(0.0), lit(1.0)));

    using Node = decltype(theta);
    static_assert(std::is_same_v<typename Node::dims_list, TypeList<Obs>>);
  };

  // ===========================================================================
  // IndexedStochasticNode::sym()
  // ===========================================================================

  "IndexedStochasticNode::sym returns IndexedSymbol"_test = [] {
    struct Obs {};
    auto theta = plate<Obs>(beta(lit(2.0), lit(3.0)));
    auto sym = theta.sym();

    static_assert(is_indexed_symbol_v<decltype(sym)>);
    static_assert(decltype(sym)::ndims == 1);
    static_assert(decltype(sym)::has_dim<Obs>);
  };

  // ===========================================================================
  // IndexedStochasticNode::logProb()
  // ===========================================================================

  "IndexedStochasticNode::logProb returns SumOver"_test = [] {
    struct Obs {};
    auto theta = plate<Obs>(beta(lit(2.0), lit(3.0)));
    auto lp = theta.logProb();

    static_assert(is_sum_over_v<decltype(lp)>);
  };

  "IndexedStochasticNode::logProb evaluates correctly"_test = [] {
    struct Obs {};
    auto theta = plate<Obs>(beta(lit(2.0), lit(3.0)));
    auto lp = theta.logProb();

    std::vector<double> theta_vals = {0.3, 0.5, 0.7};
    double result = evaluateIndexed(lp, theta = indexed(theta_vals));

    // Unnormalized log Beta: (alpha-1)*log(x) + (beta-1)*log(1-x) = log(x) + 2*log(1-x)
    auto expected_at = [](double x) {
      return std::log(x) + 2.0 * std::log(1.0 - x);
    };
    double expected = expected_at(0.3) + expected_at(0.5) + expected_at(0.7);
    expectNear(expected, result, 1e-10);
  };

  // ===========================================================================
  // IndexedStochasticNode with scalar parameters
  // ===========================================================================

  "plate with scalar parameter"_test = [] {
    struct Obs {};
    auto alpha = halfNormal(5.0);
    auto theta = plate<Obs>(beta(alpha, lit(3.0)));

    static_assert(IsIndexedRandomVar<decltype(theta)>);

    auto lp = theta.logProb();

    std::vector<double> theta_vals = {0.3, 0.5, 0.7};
    double result = evaluateIndexed(lp, alpha = 2.0, theta = indexed(theta_vals));

    // alpha=2, beta=3: log(x) + 2*log(1-x)
    auto expected_at = [](double x) {
      return std::log(x) + 2.0 * std::log(1.0 - x);
    };
    double expected = expected_at(0.3) + expected_at(0.5) + expected_at(0.7);
    expectNear(expected, result, 1e-10);
  };

  // ===========================================================================
  // Nested plates
  // ===========================================================================

  "nested plate creates 2D indexed node"_test = [] {
    struct Countries {};
    struct Years {};

    auto theta = plate<Years>(plate<Countries>(normal(lit(0.0), lit(1.0))));

    using NodeType = decltype(theta);
    using DimsType = typename NodeType::dims_list;

    static_assert(is_indexed_random_var_v<NodeType>);
    static_assert(std::is_same_v<DimsType, TypeList<Countries, Years>>);

    auto sym = theta.sym();
    static_assert(sym.ndims == 2);
    static_assert(sym.has_dim<Countries>);
    static_assert(sym.has_dim<Years>);
  };

  "nested plate logProb returns nested SumOver"_test = [] {
    struct Countries {};
    struct Years {};

    auto theta = plate<Years>(plate<Countries>(normal(lit(0.0), lit(1.0))));
    auto lp = theta.logProb();

    // Should be SumOver<Countries, SumOver<Years, ...>>
    static_assert(is_sum_over_v<decltype(lp)>);
  };

  // ===========================================================================
  // Binding syntax
  // ===========================================================================

  "IndexedStochasticNode binding with indexed()"_test = [] {
    struct Obs {};
    auto theta = plate<Obs>(beta(lit(2.0), lit(3.0)));

    std::vector<double> data = {0.3, 0.5};
    auto binding = theta = indexed(data);

    static_assert(is_indexed_binding_v<decltype(binding)>);
    expectEq(binding.size(), 2UL);
  };

  // ===========================================================================
  // toSymbolic and argToSymbolic
  // ===========================================================================

  "toSymbolic returns IndexedSymbol"_test = [] {
    struct Obs {};
    auto theta = plate<Obs>(normal(lit(0.0), lit(1.0)));
    auto sym = toSymbolic(theta);

    static_assert(is_indexed_symbol_v<decltype(sym)>);
  };

  "toSymbolic works for IndexedRandomVar"_test = [] {
    struct Obs {};
    auto theta = plate<Obs>(normal(lit(0.0), lit(1.0)));
    auto sym = toSymbolic(theta);

    static_assert(is_indexed_symbol_v<decltype(sym)>);
  };

  // ===========================================================================
  // Different distributions
  // ===========================================================================

  "plate with normal distribution"_test = [] {
    struct Obs {};
    auto y = plate<Obs>(normal(lit(0.0), lit(1.0)));
    auto lp = y.logProb();

    std::vector<double> y_vals = {-1.0, 0.0, 1.0};
    double result = evaluateIndexed(lp, y = indexed(y_vals));

    expectTrue(std::isfinite(result));
  };

  "plate with gamma distribution"_test = [] {
    struct Obs {};
    auto x = plate<Obs>(gamma(lit(2.0), lit(0.5)));
    auto lp = x.logProb();

    std::vector<double> x_vals = {1.0, 2.0, 3.0};
    double result = evaluateIndexed(lp, x = indexed(x_vals));

    expectTrue(std::isfinite(result));
  };

  return TestRegistry::result();
}
