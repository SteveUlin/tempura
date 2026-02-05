// Tests for symbolic4/distributions/indexed_node.h
#include "symbolic4/distributions/indexed_node.h"
#include "symbolic4/distributions/wrappers.h"
#include "symbolic4/indexed/indexed_eval.h"
#include "unit.h"

#include <cmath>
#include <vector>

using namespace tempura;
using namespace tempura::symbolic4;

// Local helper: check if a type is in a TypeList (replaces list_contains_v)
template <typename T, typename List> struct ListContains;
template <typename T, typename... Ts>
struct ListContains<T, TypeList<Ts...>> {
  static constexpr bool value = (std::is_same_v<T, Ts> || ...);
};
template <typename T, typename List>
constexpr bool list_contains_v = ListContains<T, List>::value;

auto main() -> int {
  // ===========================================================================
  // IndexedStochasticNode type traits
  // ===========================================================================

  "plate creates IndexedStochasticNode"_test = [] {
    struct Obs {};
    auto theta = plate<Obs>(beta(2.0_c, 3.0_c));

    static_assert(is_indexed_random_var_v<decltype(theta)>);
    static_assert(IsIndexedRandomVar<decltype(theta)>);
  };

  "IndexedStochasticNode has correct types"_test = [] {
    struct Obs {};
    auto theta = plate<Obs>(normal(0.0_c, 1.0_c));

    using Node = decltype(theta);
    static_assert(std::is_same_v<typename Node::dims_list, TypeList<Obs>>);
  };

  // ===========================================================================
  // IndexedStochasticNode::sym()
  // ===========================================================================

  "IndexedStochasticNode::sym returns discoverable atom"_test = [] {
    struct Obs {};
    auto theta = plate<Obs>(beta(2.0_c, 3.0_c));
    auto sym = theta.sym();

    // sym() returns Atom<Id, IndexedSample<Dist, DimsList>> for auto-discovery
    static_assert(is_indexed_random_var_atom_v<decltype(sym)>);
    using Effect = typename decltype(sym)::effect_type;
    static_assert(Size_v<typename Effect::dims_list> == 1);
    static_assert(list_contains_v<Obs, typename Effect::dims_list>);
  };

  // ===========================================================================
  // IndexedStochasticNode::logProb()
  // ===========================================================================

  "IndexedStochasticNode::logProb returns SumOver"_test = [] {
    struct Obs {};
    auto theta = plate<Obs>(beta(2.0_c, 3.0_c));
    auto lp = theta.logProb();

    static_assert(is_sum_over_v<decltype(lp)>);
  };

  "IndexedStochasticNode::logProb evaluates correctly"_test = [] {
    struct Obs {};
    auto theta = plate<Obs>(beta(2.0_c, 3.0_c));
    auto lp = theta.logProb();

    std::vector<double> theta_vals = {0.3, 0.5, 0.7};
    double result = evaluateIndexed(lp, theta = indexed(theta_vals));

    // Full log Beta(2,3): (α-1)log(x) + (β-1)log(1-x) - logB(α,β)
    double log_normalizer = std::lgamma(2.0) + std::lgamma(3.0) - std::lgamma(5.0);
    auto expected_at = [log_normalizer](double x) {
      return std::log(x) + 2.0 * std::log(1.0 - x) - log_normalizer;
    };
    double expected = expected_at(0.3) + expected_at(0.5) + expected_at(0.7);
    expectNear(expected, result, 1e-10);
  };

  // ===========================================================================
  // IndexedStochasticNode with scalar parameters
  // ===========================================================================

  "plate with scalar parameter"_test = [] {
    struct Obs {};
    auto alpha = halfNormal(5.0_c);
    auto theta = plate<Obs>(beta(alpha, 3.0_c));

    static_assert(IsIndexedRandomVar<decltype(theta)>);

    auto lp = theta.logProb();

    std::vector<double> theta_vals = {0.3, 0.5, 0.7};
    double result = evaluateIndexed(lp, alpha = 2.0, theta = indexed(theta_vals));

    // alpha=2, beta=3: full log Beta including normalizer
    double log_normalizer = std::lgamma(2.0) + std::lgamma(3.0) - std::lgamma(5.0);
    auto expected_at = [log_normalizer](double x) {
      return std::log(x) + 2.0 * std::log(1.0 - x) - log_normalizer;
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

    auto theta = plate<Years>(plate<Countries>(normal(0.0_c, 1.0_c)));

    using NodeType = decltype(theta);
    using DimsType = typename NodeType::dims_list;

    static_assert(is_indexed_random_var_v<NodeType>);
    static_assert(std::is_same_v<DimsType, TypeList<Countries, Years>>);

    auto sym = theta.sym();
    // sym() returns Atom<Id, IndexedSample<Dist, TypeList<Countries, Years>>>
    using Effect = typename decltype(sym)::effect_type;
    static_assert(Size_v<typename Effect::dims_list> == 2);
    static_assert(list_contains_v<Countries, typename Effect::dims_list>);
    static_assert(list_contains_v<Years, typename Effect::dims_list>);
  };

  "nested plate logProb returns nested SumOver"_test = [] {
    struct Countries {};
    struct Years {};

    auto theta = plate<Years>(plate<Countries>(normal(0.0_c, 1.0_c)));
    auto lp = theta.logProb();

    // Should be SumOver<Countries, SumOver<Years, ...>>
    static_assert(is_sum_over_v<decltype(lp)>);
  };

  // ===========================================================================
  // Binding syntax
  // ===========================================================================

  "IndexedStochasticNode binding with indexed()"_test = [] {
    struct Obs {};
    auto theta = plate<Obs>(beta(2.0_c, 3.0_c));

    std::vector<double> data = {0.3, 0.5};
    auto binding = theta = indexed(data);

    static_assert(is_indexed_binding_v<decltype(binding)>);
    expectEq(binding.size(), 2UL);
  };

  // ===========================================================================
  // toSymbolic and argToSymbolic
  // ===========================================================================

  "toSymbolic returns discoverable atom"_test = [] {
    struct Obs {};
    auto theta = plate<Obs>(normal(0.0_c, 1.0_c));
    auto sym = toSymbolic(theta);

    // toSymbolic() delegates to sym(), returning Atom<Id, IndexedSample<...>>
    static_assert(is_indexed_random_var_atom_v<decltype(sym)>);
  };

  "toSymbolic works for IndexedRandomVar"_test = [] {
    struct Obs {};
    auto theta = plate<Obs>(normal(0.0_c, 1.0_c));
    auto sym = toSymbolic(theta);

    static_assert(is_indexed_random_var_atom_v<decltype(sym)>);
  };

  // ===========================================================================
  // Different distributions
  // ===========================================================================

  "plate with normal distribution"_test = [] {
    struct Obs {};
    auto y = plate<Obs>(normal(0.0_c, 1.0_c));
    auto lp = y.logProb();

    std::vector<double> y_vals = {-1.0, 0.0, 1.0};
    double result = evaluateIndexed(lp, y = indexed(y_vals));

    expectTrue(std::isfinite(result));
  };

  "plate with gamma distribution"_test = [] {
    struct Obs {};
    auto x = plate<Obs>(gamma(2.0_c, 0.5_c));
    auto lp = x.logProb();

    std::vector<double> x_vals = {1.0, 2.0, 3.0};
    double result = evaluateIndexed(lp, x = indexed(x_vals));

    expectTrue(std::isfinite(result));
  };

  return TestRegistry::result();
}
