#include "prob/transforms.h"

#include <cmath>
#include <numbers>

#include "prob/beta.h"
#include "prob/gamma.h"
#include "prob/normal.h"
#include "unit.h"

using namespace tempura;
using namespace tempura::prob;

auto main() -> int {
  // =========================================================================
  // Identity transform
  // =========================================================================

  "Identity forward/inverse"_test = [] {
    Identity<> t;
    expectNear(t.forward(2.5), 2.5, 1e-10);
    expectNear(t.inverse(2.5), 2.5, 1e-10);
    expectNear(t.logJacobian(2.5), 0.0, 1e-10);
  };

  // =========================================================================
  // Positive transform
  // =========================================================================

  "Positive forward/inverse"_test = [] {
    Positive<> t;

    // forward: exp(z)
    expectNear(t.forward(0.0), 1.0, 1e-10);
    expectNear(t.forward(1.0), std::exp(1.0), 1e-10);
    expectNear(t.forward(-1.0), std::exp(-1.0), 1e-10);

    // inverse: log(x)
    expectNear(t.inverse(1.0), 0.0, 1e-10);
    expectNear(t.inverse(std::exp(2.0)), 2.0, 1e-10);

    // round-trip
    for (double z : {-2.0, -1.0, 0.0, 1.0, 2.0}) {
      expectNear(t.inverse(t.forward(z)), z, 1e-10);
    }
  };

  "Positive logJacobian"_test = [] {
    Positive<> t;
    // log|dx/dz| = log(exp(z)) = z
    expectNear(t.logJacobian(0.0), 0.0, 1e-10);
    expectNear(t.logJacobian(1.5), 1.5, 1e-10);
    expectNear(t.logJacobian(-2.0), -2.0, 1e-10);
  };

  "Positive adjustedLogProb"_test = [] {
    Positive<> t;
    Gamma prior{2.0, 1.0};

    double z = 0.5;
    double x = t.forward(z);  // exp(0.5)

    double expected = prior.logProb(x) + t.logJacobian(z);
    expectNear(t.adjustedLogProb(prior, z), expected, 1e-10);
  };

  // =========================================================================
  // UnitInterval transform
  // =========================================================================

  "UnitInterval forward/inverse"_test = [] {
    UnitInterval<> t;

    // forward: sigmoid(z)
    expectNear(t.forward(0.0), 0.5, 1e-10);
    expectTrue(t.forward(5.0) > 0.99);
    expectTrue(t.forward(-5.0) < 0.01);

    // inverse: logit(x)
    expectNear(t.inverse(0.5), 0.0, 1e-10);

    // round-trip
    for (double z : {-3.0, -1.0, 0.0, 1.0, 3.0}) {
      expectNear(t.inverse(t.forward(z)), z, 1e-10);
    }
  };

  "UnitInterval logJacobian"_test = [] {
    UnitInterval<> t;

    // At z=0: x=0.5, log|J| = log(0.5) + log(0.5) = 2*log(0.5)
    double expected_at_0 = 2.0 * std::log(0.5);
    expectNear(t.logJacobian(0.0), expected_at_0, 1e-10);

    // log|J| = log(x) + log(1-x) where x = sigmoid(z)
    double z = 1.0;
    double x = t.forward(z);
    double expected = std::log(x) + std::log(1.0 - x);
    expectNear(t.logJacobian(z), expected, 1e-10);
  };

  "UnitInterval adjustedLogProb"_test = [] {
    UnitInterval<> t;
    Beta prior{2.0, 3.0};

    double z = 0.5;
    double x = t.forward(z);

    double expected = prior.logProb(x) + t.logJacobian(z);
    expectNear(t.adjustedLogProb(prior, z), expected, 1e-10);
  };

  // =========================================================================
  // LowerBounded transform
  // =========================================================================

  "LowerBounded forward/inverse"_test = [] {
    LowerBounded<> t{-5.0};  // x > -5

    // forward: -5 + exp(z)
    expectNear(t.forward(0.0), -5.0 + 1.0, 1e-10);
    expectNear(t.forward(1.0), -5.0 + std::exp(1.0), 1e-10);

    // inverse: log(x - (-5))
    expectNear(t.inverse(-4.0), 0.0, 1e-10);

    // round-trip
    for (double z : {-2.0, 0.0, 2.0}) {
      expectNear(t.inverse(t.forward(z)), z, 1e-10);
    }
  };

  // =========================================================================
  // Interval transform
  // =========================================================================

  "Interval forward/inverse"_test = [] {
    Interval<> t{-1.0, 1.0};  // x in (-1, 1)

    // At z=0: x = -1 + 2*0.5 = 0
    expectNear(t.forward(0.0), 0.0, 1e-10);

    // Extremes approach bounds
    expectTrue(t.forward(10.0) > 0.99);
    expectTrue(t.forward(-10.0) < -0.99);

    // round-trip
    for (double z : {-2.0, 0.0, 2.0}) {
      expectNear(t.inverse(t.forward(z)), z, 1e-10);
    }
  };

  // =========================================================================
  // TransformedParam convenience
  // =========================================================================

  "TransformedParam positive"_test = [] {
    auto sigma = positive(0.5);

    expectNear(sigma.value(), std::exp(0.5), 1e-10);
    expectNear(sigma.logJacobian(), 0.5, 1e-10);

    // With distribution
    Gamma prior{2.0, 1.0};
    double expected = prior.logProb(sigma.value()) + sigma.logJacobian();
    expectNear(sigma.logProb(prior), expected, 1e-10);
  };

  "TransformedParam unitInterval"_test = [] {
    auto theta = unitInterval(0.0);

    expectNear(theta.value(), 0.5, 1e-10);

    Beta prior{2.0, 2.0};
    double expected = prior.logProb(theta.value()) + theta.logJacobian();
    expectNear(theta.logProb(prior), expected, 1e-10);
  };

  "TransformedParam interval"_test = [] {
    auto x = interval(0.0, -10.0, 10.0);

    expectNear(x.value(), 0.0, 1e-10);  // midpoint at z=0
  };

  // =========================================================================
  // Array operations
  // =========================================================================

  "transformAll"_test = [] {
    Positive<> t;
    std::array<double, 3> z = {0.0, 1.0, -1.0};
    auto x = transformAll(t, z);

    expectNear(x[0], 1.0, 1e-10);
    expectNear(x[1], std::exp(1.0), 1e-10);
    expectNear(x[2], std::exp(-1.0), 1e-10);
  };

  "totalLogJacobian"_test = [] {
    Positive<> t;
    std::array<double, 3> z = {1.0, 2.0, 3.0};

    // Sum of z values for Positive transform
    expectNear(totalLogJacobian(t, z), 6.0, 1e-10);
  };

  // =========================================================================
  // Practical example: Hierarchical model
  // =========================================================================

  "Hierarchical model pattern"_test = [] {
    // Simulates the pattern from the user's code
    // alpha, beta > 0 with Gamma priors
    // theta in (0,1) with Beta(alpha, beta) prior

    std::array<double, 3> state = {0.5, 0.3, 0.0};  // log(alpha), log(beta), logit(theta)

    auto alpha = positive(state[0]);
    auto beta = positive(state[1]);
    auto theta = unitInterval(state[2]);

    // All values should be valid
    expectTrue(alpha.value() > 0);
    expectTrue(beta.value() > 0);
    expectTrue(theta.value() > 0 && theta.value() < 1);

    // Log-posterior with transforms
    double lp = 0.0;
    lp += alpha.logProb(Gamma{2.0, 0.1});
    lp += beta.logProb(Gamma{2.0, 0.1});
    lp += theta.logProb(Beta{alpha.value(), beta.value()});

    expectTrue(std::isfinite(lp));
  };

  return TestRegistry::result();
}
