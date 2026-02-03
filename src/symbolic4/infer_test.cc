// Tests for infer.h - simplified inference API
#include "symbolic4/infer.h"
#include "symbolic4/distributions/joint.h"
#include "symbolic4/model.h"
#include "unit.h"

#include <cmath>

using namespace tempura;
using namespace tempura::symbolic4;

auto main() -> int {
  // =========================================================================
  // Basic infer() tests - now returns HMC-ready posterior with transforms
  // =========================================================================

  "infer returns builder with observe and build"_test = [] {
    auto mu = normal(lit(0.0), lit(1.0));

    // infer() returns a builder
    auto builder = infer(mu);

    // Can call build() to get posterior
    auto posterior = builder.build();

    // Posterior operates in unconstrained space
    // For Normal, transform is identity, so z = x
    std::array<double, 1> z = {0.5};
    double lp = posterior.logProb(z);

    // log N(0.5 | 0, 1) = -0.5 * 0.25 - 0.5*log(2π)
    // No Jacobian for unconstrained (identity transform)
    double expected = -0.5 * 0.25 - 0.5 * std::log(2 * M_PI);
    expectNear(expected, lp, 1e-10);
  };

  "infer with positive parameter includes Jacobian"_test = [] {
    auto sigma = halfNormal(lit(2.0));
    auto posterior = infer(sigma).build();

    // HalfNormal has positive support → exp transform
    // z = 0 → sigma = exp(0) = 1
    std::array<double, 1> z = {0.0};
    double lp = posterior.logProb(z);
    expectTrue(std::isfinite(lp));

    // Check that transform works: z=0 → sigma=1
    auto x = posterior.transform(z);
    expectNear(1.0, x[0], 1e-10);

    // Check that Jacobian affects log-prob (compare with different z)
    std::array<double, 1> z2 = {1.0};  // sigma = e ≈ 2.718
    double lp2 = posterior.logProb(z2);
    expectTrue(std::isfinite(lp2));
    expectTrue(lp != lp2);  // Should be different
  };

  "infer gradient includes chain rule"_test = [] {
    auto sigma = halfNormal(lit(2.0));
    auto posterior = infer(sigma).build();

    // At z = 0, sigma = exp(0) = 1
    std::array<double, 1> z = {0.0};
    auto grad = posterior.gradient(z);

    // Gradient should be finite and nonzero
    expectTrue(std::isfinite(grad[0]));

    // Gradient should change with z
    std::array<double, 1> z2 = {1.0};
    auto grad2 = posterior.gradient(z2);
    expectTrue(std::isfinite(grad2[0]));
    expectTrue(grad[0] != grad2[0]);
  };

  "infer with unit interval parameter"_test = [] {
    auto p = beta(lit(2.0), lit(2.0));
    auto posterior = infer(p).build();

    // Beta has (0,1) support → sigmoid transform
    // z = 0 → p = sigmoid(0) = 0.5
    std::array<double, 1> z = {0.0};
    double lp = posterior.logProb(z);
    expectTrue(std::isfinite(lp));

    // Check that transform works: z=0 → p=0.5
    auto x = posterior.transform(z);
    expectNear(0.5, x[0], 1e-10);

    // Check inverse: p=0.5 → z=0
    std::array<double, 1> x_half = {0.5};
    auto z_back = posterior.inverse(x_half);
    expectNear(0.0, z_back[0], 1e-10);
  };

  // =========================================================================
  // Hierarchical model tests
  // =========================================================================

  "infer with hierarchical model"_test = [] {
    auto mu = normal(lit(0.0), lit(10.0));
    auto sigma = halfNormal(lit(5.0));
    auto y = normal(mu, sigma);

    auto posterior = infer(mu, sigma, y).build();

    // z = [0, 0, 0] → x = [0, 1, 0] (identity, exp, identity)
    std::array<double, 3> z = {0.0, 0.0, 0.0};
    double lp = posterior.logProb(z);
    expectTrue(std::isfinite(lp));

    auto grad = posterior.gradient(z);
    expectTrue(std::isfinite(grad[0]));
    expectTrue(std::isfinite(grad[1]));
    expectTrue(std::isfinite(grad[2]));
  };

  "infer with observation"_test = [] {
    auto mu = normal(lit(0.0), lit(10.0));
    auto sigma = halfNormal(lit(5.0));
    auto y = normal(mu, sigma);

    // Observe y = 3.5 - observe() returns final posterior, no build() needed
    auto posterior = infer(mu, sigma, y).observe(y = 3.5);

    // Still 3 parameters in state (observation is bound, not removed)
    std::array<double, 3> z = {0.0, 0.0, 0.0};
    double lp = posterior.logProb(z);
    expectTrue(std::isfinite(lp));
  };

  // =========================================================================
  // Transform and inverse tests
  // =========================================================================

  "infer transform and inverse are consistent"_test = [] {
    auto mu = normal(lit(0.0), lit(10.0));
    auto sigma = halfNormal(lit(5.0));
    auto posterior = infer(mu, sigma).build();

    // z → x → z should round-trip
    std::array<double, 2> z = {1.5, -0.5};
    auto x = posterior.transform(z);
    auto z_back = posterior.inverse(x);

    expectNear(z[0], z_back[0], 1e-10);
    expectNear(z[1], z_back[1], 1e-10);

    // Check transforms are correct
    // mu: identity, sigma: exp
    expectNear(z[0], x[0], 1e-10);           // mu unchanged
    expectNear(std::exp(z[1]), x[1], 1e-10); // sigma = exp(z)
  };

  // =========================================================================
  // inferRaw() tests - raw posterior without transforms
  // =========================================================================

  "inferRaw operates in constrained space"_test = [] {
    auto mu = normal(lit(0.0), lit(1.0));
    auto posterior = inferRaw(mu);

    // No transform, no Jacobian - just raw log-prob
    double lp = posterior.logProb(0.5);
    double expected = -0.5 * 0.25 - 0.5 * std::log(2 * M_PI);
    expectNear(expected, lp, 1e-10);
  };

  "inferRaw gradient in constrained space"_test = [] {
    auto mu = normal(lit(0.0), lit(1.0));
    auto posterior = inferRaw(mu);

    // d/dmu[-0.5 * mu²] = -mu = -0.5
    auto grad = posterior.gradient(0.5);
    expectNear(-0.5, grad[0], 1e-10);
  };

  "inferRaw with hierarchical model"_test = [] {
    auto mu = normal(lit(0.0), lit(10.0));
    auto sigma = halfNormal(lit(5.0));
    auto y = normal(mu, sigma);

    auto posterior = inferRaw(mu, sigma, y);

    // Direct evaluation in constrained space
    double lp = posterior.logProb(0.5, 2.0, 1.0);

    // Compare with manual joint log-prob
    auto manual_lp = logProb(mu, sigma, y);
    double expected = evaluate(manual_lp, mu = 0.5, sigma = 2.0, y = 1.0);
    expectNear(expected, lp, 1e-10);
  };

  // =========================================================================
  // Self-consistency tests for infer() API
  // =========================================================================

  "infer produces consistent transforms"_test = [] {
    auto mu = normal(lit(0.0), lit(10.0));
    auto sigma = halfNormal(lit(5.0));

    auto posterior = infer(mu, sigma).build();

    // Test at various points
    std::array<double, 2> z = {1.0, 0.5};

    // Log-prob should be finite
    double lp = posterior.logProb(z);
    expectTrue(std::isfinite(lp));

    // Gradient should be finite
    auto grad = posterior.gradient(z);
    expectTrue(std::isfinite(grad[0]));
    expectTrue(std::isfinite(grad[1]));

    // Transform and inverse should round-trip
    auto x = posterior.transform(z);
    auto z_back = posterior.inverse(x);
    expectNear(z[0], z_back[0], 1e-10);
    expectNear(z[1], z_back[1], 1e-10);

    // Transform should apply correct constraints
    // mu: unconstrained (identity), sigma: positive (exp)
    expectEq(z[0], x[0]);  // mu unchanged
    expectNear(std::exp(z[1]), x[1], 1e-10);  // sigma = exp(z)
  };

  "infer with three params produces correct transforms"_test = [] {
    auto mu = normal(lit(0.0), lit(10.0));
    auto sigma = halfNormal(lit(5.0));
    auto p = beta(lit(2.0), lit(2.0));

    auto posterior = infer(mu, sigma, p).build();

    // z = [0, 0, 0] → x = [0, 1, 0.5]
    std::array<double, 3> z = {0.0, 0.0, 0.0};

    auto x = posterior.transform(z);

    // mu: unconstrained → identity
    expectEq(0.0, x[0]);
    // sigma: positive → exp(0) = 1
    expectNear(1.0, x[1], 1e-10);
    // p: unit interval → sigmoid(0) = 0.5
    expectNear(0.5, x[2], 1e-10);

    // Log-prob should be finite
    double lp = posterior.logProb(z);
    expectTrue(std::isfinite(lp));
  };

  return TestRegistry::result();
}
