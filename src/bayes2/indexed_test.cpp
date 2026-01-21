#include "bayes2/indexed.h"

#include <cmath>
#include <vector>

#include "bayes2/traversal.h"
#include "prob/log_prob.h"
#include "unit.h"

using namespace tempura;
using namespace tempura::bayes2;
using namespace tempura::symbolic3;
using namespace tempura::prob;

// Test Dimensions
struct Countries {};
struct Observations {};

int main() {
  // ==========================================================================
  // IndexedRandomVar Basic Tests
  // ==========================================================================

  "IndexedRandomVar has indexed symbol"_test = [] {
    auto alpha = halfNormal(5.0);
    auto beta_param = halfNormal(5.0);
    auto theta = plate<Countries>(beta(alpha, beta_param));

    using SymType = decltype(theta)::symbol_type;
    static_assert(is_indexed_over<SymType, Countries>);
    static_assert(rank_of<SymType> == 1);
    return TestRegistry::result();
  };

  "IndexedRandomVar instanceLogProb is symbolic"_test = [] {
    auto alpha = halfNormal(5.0);
    auto beta_param = halfNormal(5.0);
    auto theta = plate<Countries>(beta(alpha, beta_param));

    auto lp = theta.instanceLogProb();
    static_assert(Symbolic<decltype(lp)>);
    return TestRegistry::result();
  };

  "IndexedRandomVar nodeLogProb is SumOver"_test = [] {
    auto alpha = halfNormal(5.0);
    auto beta_param = halfNormal(5.0);
    auto theta = plate<Countries>(beta(alpha, beta_param));

    auto total_lp = theta.nodeLogProb();
    static_assert(is_sum_over<decltype(total_lp)>);
    static_assert(
        std::is_same_v<reduction_dim_t<decltype(total_lp)>, Countries>);
    return TestRegistry::result();
  };

  // ==========================================================================
  // Differentiation Tests
  // ==========================================================================

  "diff of nodeLogProb w.r.t. scalar parent"_test = [] {
    auto alpha = halfNormal(5.0);
    auto beta_param = halfNormal(5.0);
    auto theta = plate<Countries>(beta(alpha, beta_param));

    auto total_lp = theta.nodeLogProb();
    auto d_alpha = diff(total_lp, alpha);

    // Result should be SumOver (diff pushes into sum)
    static_assert(is_sum_over<decltype(d_alpha)>);
    return TestRegistry::result();
  };

  "diff of instanceLogProb w.r.t. indexed symbol"_test = [] {
    auto alpha = halfNormal(5.0);
    auto beta_param = halfNormal(5.0);
    auto theta = plate<Countries>(beta(alpha, beta_param));

    auto instance_lp = theta.instanceLogProb();
    auto d_theta = diff(instance_lp, theta);

    // d/d(theta) log p(theta | alpha, beta) is a symbolic expression
    static_assert(Symbolic<decltype(d_theta)>);
    return TestRegistry::result();
  };

  // ==========================================================================
  // Evaluation Tests
  // ==========================================================================

  "evaluate instanceLogProb"_test = [] {
    auto alpha = halfNormal(5.0);
    auto beta_param = halfNormal(5.0);
    auto theta = plate<Countries>(beta(alpha, beta_param));

    auto instance_lp = theta.instanceLogProb();

    // Evaluate for specific values
    double result = evaluate(instance_lp, BinderPack{alpha = 2.0,
                                                     beta_param = 3.0,
                                                     theta.sym() = 0.5});

    // log Beta(0.5 | 2, 3) = logBeta(0.5, 2, 3)
    double expected = logBeta(0.5, 2.0, 3.0);
    expectNear(result, expected, 1e-10);
    return TestRegistry::result();
  };

  "evaluate nodeLogProb (sum over countries)"_test = [] {
    auto alpha = halfNormal(5.0);
    auto beta_param = halfNormal(5.0);
    auto theta = plate<Countries>(beta(alpha, beta_param));

    auto total_lp = theta.nodeLogProb();

    // Uniform binding syntax: symbol = indexed(values)
    std::vector<double> theta_vals = {0.3, 0.5, 0.7};
    double result = evaluate(total_lp, alpha = 2.0, beta_param = 3.0,
                             theta = indexed(theta_vals));

    // Should equal sum of individual log-probs
    double expected = 0.0;
    for (double t : theta_vals) {
      expected += logBeta(t, 2.0, 3.0);
    }
    expectNear(result, expected, 1e-10);
    return TestRegistry::result();
  };

  // ==========================================================================
  // Parent Tracking Tests
  // ==========================================================================

  "IndexedRandomVar tracks parents"_test = [] {
    auto alpha = halfNormal(5.0);
    auto beta_param = halfNormal(5.0);
    auto theta = plate<Countries>(beta(alpha, beta_param));

    // theta should have alpha and beta_param as parents
    const auto& parents = theta.parents();
    static_assert(std::tuple_size_v<std::decay_t<decltype(parents)>> == 2);
    return TestRegistry::result();
  };

  // ==========================================================================
  // Mixed Scalar and Indexed Tests
  // ==========================================================================

  "joint log-prob evaluated separately"_test = [] {
    // Hierarchical model:
    // alpha ~ HalfNormal(5)
    // beta ~ HalfNormal(5)
    // for i in Countries:
    //   theta[i] ~ Beta(alpha, beta)
    auto alpha = halfNormal(5.0);
    auto beta_param = halfNormal(5.0);
    auto theta = plate<Countries>(beta(alpha, beta_param));

    // Evaluate scalars with BinderPack, indexed with variadic
    double scalar_lp = evaluate(alpha.nodeLogProb() + beta_param.nodeLogProb(),
                                BinderPack{alpha = 2.0, beta_param = 3.0});

    std::vector<double> theta_vals = {0.3, 0.5, 0.7};
    double indexed_lp = evaluate(theta.nodeLogProb(), alpha = 2.0,
                                 beta_param = 3.0,
                                 theta = indexed(theta_vals));

    double result = scalar_lp + indexed_lp;

    double expected = logHalfNormal(2.0, 5.0) +
                      logHalfNormal(3.0, 5.0) +
                      logBeta(0.3, 2.0, 3.0) +
                      logBeta(0.5, 2.0, 3.0) +
                      logBeta(0.7, 2.0, 3.0);
    expectNear(result, expected, 1e-10);
    return TestRegistry::result();
  };

  // ==========================================================================
  // Type Trait Tests
  // ==========================================================================

  "is_indexed_random_var trait"_test = [] {
    auto alpha = halfNormal(5.0);
    auto theta = plate<Countries>(beta(alpha, halfNormal(5.0)));

    static_assert(!is_indexed_random_var<decltype(alpha)>);
    static_assert(is_indexed_random_var<decltype(theta)>);
    return TestRegistry::result();
  };

  // ==========================================================================
  // Binding Syntax Tests
  // ==========================================================================

  "IndexedRandomVar binding with operator="_test = [] {
    auto alpha = halfNormal(5.0);
    auto theta = plate<Countries>(beta(alpha, halfNormal(5.0)));

    std::vector<double> vals = {0.1, 0.2, 0.3};
    auto binding = theta = indexed(vals);

    static_assert(is_indexed_binding<decltype(binding)>);
    expectEq(binding.size(), 3UL);
    return TestRegistry::result();
  };

  return TestRegistry::result();
}
