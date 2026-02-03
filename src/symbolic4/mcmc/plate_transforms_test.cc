#include "symbolic4/mcmc/plate_transforms.h"

#include <cmath>
#include <vector>

#include "symbolic4/distributions/joint.h"
#include "unit.h"

using namespace tempura;
using namespace tempura::symbolic4;

auto main() -> int {
  // =========================================================================
  // Basic plate transform setup
  // =========================================================================

  "ScalarParamSpec basic"_test = [] {
    auto sigma = halfNormal(5.0);
    auto t = autoTransform(sigma);

    ScalarParamSpec<decltype(sigma), decltype(t)> spec{t};

    expectEq(spec.size(), 1UL);
    expectFalse(spec.is_indexed);

    // Test transform: exp(0) = 1
    expectNear(spec.transformValue(0.0), 1.0, 1e-10);
    // Jacobian for log transform: z
    expectNear(spec.logJacobian(0.5), 0.5, 1e-10);
  };

  "IndexedParamSpec basic"_test = [] {
    struct Countries {};
    auto alpha = gamma(2.0, 0.1);
    auto theta = plate<Countries>(beta(alpha, lit(3.0)));
    auto t = autoTransform(theta);

    // IndexedParamSpec would be created by the factory
    // Just test the transform values
    expectTrue(is_unit_interval_v<decltype(t)>);
  };

  // =========================================================================
  // DimSizes
  // =========================================================================

  "DimSizes single dimension"_test = [] {
    struct Countries {};
    DimSizes<Countries> dims{{38}};

    expectEq(dims.get<Countries>(), 38UL);
  };

  "DimSizes multiple dimensions"_test = [] {
    struct Countries {};
    struct Years {};
    DimSizes<Countries, Years> dims{{38, 10}};

    expectEq(dims.get<Countries>(), 38UL);
    expectEq(dims.get<Years>(), 10UL);
  };

  // =========================================================================
  // makePlateTransformedPosterior
  // =========================================================================

  "makePlateTransformedPosterior scalar only"_test = [] {
    auto alpha = gamma(2.0, 0.1);
    auto beta_param = gamma(2.0, 0.1);

    auto joint = logProb(alpha, beta_param);
    auto posterior = makePlateTransformedPosterior(joint, alpha, beta_param).build();

    expectEq(posterior.stateDim(), 2UL);

    // Test evaluation at z = [log(3), log(17)]
    std::vector<double> z = {std::log(3.0), std::log(17.0)};
    double lp = posterior.logProb(z);
    expectTrue(std::isfinite(lp));

    // Test transform
    auto x = posterior.transform(z);
    expectNear(x[0], 3.0, 1e-10);
    expectNear(x[1], 17.0, 1e-10);
  };

  "makePlateTransformedPosterior with plate"_test = [] {
    struct Countries {};

    auto alpha = gamma(2.0, 0.1);
    auto theta = plate<Countries>(beta(alpha, lit(3.0)));

    auto joint = logProb(alpha, theta);

    // Create posterior with dimension specified
    auto posterior = makePlateTransformedPosterior(joint, alpha, theta)
                         .withDimension<Countries>(5)
                         .build();

    // State: [z_alpha, z_theta[0], ..., z_theta[4]]
    expectEq(posterior.stateDim(), 6UL);

    // Create state: alpha = 3.0, theta = [0.3, 0.4, 0.5, 0.6, 0.7]
    std::vector<double> z(6);
    z[0] = std::log(3.0);  // z_alpha
    for (int i = 0; i < 5; ++i) {
      double theta_i = 0.3 + 0.1 * i;
      z[1 + i] = std::log(theta_i / (1.0 - theta_i));  // logit(theta_i)
    }

    double lp = posterior.logProb(z);
    expectTrue(std::isfinite(lp));

    // Gradient should also work
    auto grad = posterior.gradient(z);
    expectEq(grad.size(), 6UL);
    for (double g : grad) {
      expectTrue(std::isfinite(g));
    }
  };

  "makePlateTransformedPosterior hierarchical"_test = [] {
    struct Countries {};

    // Full hierarchical model:
    // alpha ~ Gamma(2, 0.1)
    // beta ~ Gamma(2, 0.1)
    // theta[i] ~ Beta(alpha, beta) for i in Countries

    auto alpha = gamma(lit(2.0), lit(0.1));
    auto beta_param = gamma(lit(2.0), lit(0.1));
    auto theta = plate<Countries>(beta(alpha, beta_param));

    auto joint = logProb(alpha, beta_param, theta);

    auto posterior = makePlateTransformedPosterior(joint, alpha, beta_param, theta)
                         .withDimension<Countries>(3)
                         .build();

    // State: [z_alpha, z_beta, z_theta[0], z_theta[1], z_theta[2]]
    expectEq(posterior.stateDim(), 5UL);

    std::vector<double> z = {
        std::log(3.0),   // alpha
        std::log(17.0),  // beta
        0.0,             // logit(0.5)
        -0.5,            // logit(~0.38)
        0.5              // logit(~0.62)
    };

    double lp = posterior.logProb(z);
    expectTrue(std::isfinite(lp));
  };

  return TestRegistry::result();
}
