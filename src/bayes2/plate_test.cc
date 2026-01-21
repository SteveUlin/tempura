#include "bayes2/plate.h"

#include <cmath>
#include <numbers>
#include <vector>

#include "bayes2/bayes2.h"
#include "prob/log_prob.h"
#include "unit.h"

using namespace tempura;
using namespace tempura::bayes2;
using namespace tempura::symbolic3;

auto main() -> int {
  // ===========================================================================
  // IndexedBinding Tests
  // ===========================================================================

  "indexed binding creation"_test = [] {
    struct TestId {};
    Symbol<TestId> sym;

    std::vector<double> values = {1.0, 2.0, 3.0, 4.0, 5.0};
    auto binding = indexed(sym, values);

    expectEq(1.0, binding.at(0));
    expectEq(3.0, binding.at(2));
    expectEq(5.0, binding.at(4));
  };

  "indexed binding from GraphNode"_test = [] {
    auto theta = halfNormal(5.0);

    std::vector<double> values = {0.1, 0.2, 0.3};
    auto binding = indexed(theta, values);

    expectEq(0.1, binding.at(0));
    expectEq(0.2, binding.at(1));
    expectEq(0.3, binding.at(2));
  };

  // ===========================================================================
  // evaluatePlate Tests
  // ===========================================================================

  "evaluate plate - simple sum"_test = [] {
    // Test: evaluate x over 3 instances where x = [1, 2, 3]
    // Sum should be 1 + 2 + 3 = 6
    struct XId {};
    Symbol<XId> x;

    std::vector<double> x_values = {1.0, 2.0, 3.0};
    auto result = evaluatePlate(x, 3, indexed(x, x_values));

    expectEq(6.0, result);
  };

  "evaluate plate - expression with scalar and indexed"_test = [] {
    // Test: evaluate (a * x) over 3 instances
    // where a = 2.0 (scalar) and x = [1, 2, 3] (indexed)
    // Sum should be 2*1 + 2*2 + 2*3 = 2 + 4 + 6 = 12
    struct AId {};
    struct XId {};
    Symbol<AId> a;
    Symbol<XId> x;

    std::vector<double> x_values = {1.0, 2.0, 3.0};
    auto expr = a * x;

    auto result = evaluatePlate(expr, 3, a = 2.0, indexed(x, x_values));

    expectEq(12.0, result);
  };

  "evaluate plate - log prob sum"_test = [] {
    // Test: sum of log p(x | μ, σ) for normal distribution
    // where μ = 0 (scalar), σ = 1 (scalar), x = [-1, 0, 1] (indexed)
    struct MuId {};
    struct SigmaId {};
    struct XId {};
    Symbol<MuId> mu;
    Symbol<SigmaId> sigma;
    Symbol<XId> x;

    auto lp_expr = prob::logNormal(x, mu, sigma);

    std::vector<double> x_values = {-1.0, 0.0, 1.0};

    auto total_lp = evaluatePlate(lp_expr, 3, mu = 0.0, sigma = 1.0,
                                  indexed(x, x_values));

    // log p(x=-1 | 0,1) + log p(x=0 | 0,1) + log p(x=1 | 0,1)
    // = -0.5*1 - 0.5*ln(2π) + -0.5*0 - 0.5*ln(2π) + -0.5*1 - 0.5*ln(2π)
    // = -1 - 1.5*ln(2π) ≈ -3.756
    double expected =
        3 * (-0.5 * std::log(2 * std::numbers::pi)) + (-0.5 - 0.0 - 0.5);
    expectNear(expected, total_lp, 1e-10);
  };

  // ===========================================================================
  // evaluatePlateGradient Tests
  // ===========================================================================

  "evaluate plate gradient - simple"_test = [] {
    // Test: gradient of sum(a * x_i) w.r.t. a
    // d/da Σᵢ(a * xᵢ) = Σᵢ xᵢ = 1 + 2 + 3 = 6
    struct AId {};
    struct XId {};
    Symbol<AId> a;
    Symbol<XId> x;

    auto expr = a * x;
    std::vector<double> x_values = {1.0, 2.0, 3.0};

    // Gradient of a*x w.r.t. a is x (but unsimplified, may still contain a)
    auto grad_expr = diff(expr, a);
    // Must provide binding for 'a' even though it's multiplied by 0 in the
    // derivative (1*x + a*0) because the expression isn't fully simplified
    auto total_grad = evaluatePlate(grad_expr, 3, a = 1.0, indexed(x, x_values));

    expectEq(6.0, total_grad);
  };

  // ===========================================================================
  // Plate Struct Tests
  // ===========================================================================

  "plate factory"_test = [] {
    auto alpha = halfNormal(5.0);
    auto theta = bayes2::beta(alpha, alpha);

    auto p = plate(theta);

    // instanceLogProb should give us the joint log-prob for one theta
    [[maybe_unused]] auto lp = p.instanceLogProb();

    // Plate's direct nodeLogProb is 0 (sum happens elsewhere)
    auto direct_lp = p.nodeLogProb();
    expectEq(0.0, evaluate(direct_lp, BinderPack{}));
  };

  // ===========================================================================
  // Integration Test - Hierarchical Model Pattern
  // ===========================================================================

  "hierarchical model with plate - beta-binomial"_test = [] {
    // Model:
    //   α ~ HalfNormal(5)
    //   β ~ HalfNormal(5)
    //   for i in 1..N:
    //     θ_i ~ Beta(α, β)
    //
    // We want: Σᵢ log p(θᵢ | α, β)

    auto alpha = halfNormal(5.0);
    auto beta_param = halfNormal(5.0);

    // "Template" for one country's theta
    auto theta = bayes2::beta(alpha, beta_param);

    // Log-prob for ONE instance of theta given alpha, beta
    auto instance_lp = theta.nodeLogProb();

    // Test data: 3 countries with different theta values
    std::vector<double> theta_values = {0.3, 0.5, 0.7};

    // Evaluate: Σᵢ log p(θᵢ | α=2, β=3)
    double total_lp = evaluatePlate(instance_lp, 3,
                                    alpha = 2.0,
                                    beta_param = 3.0,
                                    indexed(theta, theta_values));

    // Verify against direct calculation
    double expected = 0.0;
    for (double t : theta_values) {
      expected += prob::logBeta(t, 2.0, 3.0);
    }
    expectNear(expected, total_lp, 1e-10);
  };

  return TestRegistry::result();
}
