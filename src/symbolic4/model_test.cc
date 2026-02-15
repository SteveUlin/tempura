// Tests for symbolic4/model.h - Unified model API
#include "symbolic4/model.h"

#include <array>
#include <cmath>
#include <vector>

#include "symbolic4/distributions/distributions.h"
#include "symbolic4/distributions/indexed_node.h"
#include "symbolic4/indexed/indexed.h"
#include "symbolic4/interpreter/eval.h"
#include "symbolic4/interpreter/to_string.h"
#include "unit.h"

using namespace tempura;
using namespace tempura::symbolic4;

auto main() -> int {
  "Model basic construction"_test = [] {
    auto mu = normal(0.0_c, 10.0_c);
    auto sigma = halfNormal(5.0_c);
    auto m = model(mu, sigma);

    // Check we can access RVs
    static_assert(decltype(m)::NumRVs == 2);

    // Check jointLogProb compiles and evaluates
    auto lp = m.jointLogProb();
    double val = evaluate(lp, mu = 1.0, sigma = 2.0);

    // Should be finite
    expectTrue(std::isfinite(val));
  };

  "Model with hierarchical structure"_test = [] {
    auto mu = normal(0.0_c, 10.0_c);
    auto sigma = halfNormal(5.0_c);
    auto y = normal(mu, sigma);
    auto m = model(mu, sigma, y);

    // Joint log-prob includes all three RVs
    auto lp = m.jointLogProb();
    double val = evaluate(lp, mu = 1.0, sigma = 2.0, y = 1.5);
    expectTrue(std::isfinite(val));
  };

  "Model posterior builder - no observations"_test = [] {
    auto mu = normal(0.0_c, 10.0_c);
    auto sigma = halfNormal(5.0_c);
    auto m = model(mu, sigma);

    auto posterior = m.posterior().build();

    // Symbolic lookup: pass constrained values, posterior handles transforms
    double lp = posterior.logProbAt(BinderPack{mu = 1.0, sigma = 2.0});
    expectTrue(std::isfinite(lp));

    // Gradient via flat state (HMC internal representation)
    std::vector<double> z = {1.0, 0.5};
    auto grad = posterior.gradient(z);
    expectTrue(std::isfinite(grad[0]));
    expectTrue(std::isfinite(grad[1]));
  };

  "Model posterior builder - with observation"_test = [] {
    auto mu = normal(0.0_c, 10.0_c);
    auto sigma = halfNormal(5.0_c);
    auto y = normal(mu, sigma);
    auto m = model(mu, sigma, y);

    auto posterior = m.posterior().observe(y = 3.5).build();

    // After observing y=3.5, only mu and sigma are sampled
    double lp = posterior.logProbAt(BinderPack{mu = 1.0, sigma = 2.0});
    expectTrue(std::isfinite(lp));
  };

  "Model params extraction"_test = [] {
    auto mu = normal(0.0_c, 10.0_c);
    auto sigma = halfNormal(5.0_c);
    auto m = model(mu, sigma);

    auto [mu_sym, sigma_sym] = m.params();

    // Symbols should be usable for evaluation
    auto expr = mu_sym + sigma_sym;
    double val = evaluate(expr, mu = 1.0, sigma = 2.0);
    expectNear(val, 3.0, 1e-10);
  };

  "Model factory function"_test = [] {
    auto mu = normal(0.0_c, 1.0_c);
    auto sigma = halfNormal(1.0_c);

    // Both syntaxes should work
    auto m1 = Model{mu, sigma};
    auto m2 = model(mu, sigma);

    auto lp1 = m1.jointLogProb();
    auto lp2 = m2.jointLogProb();

    double v1 = evaluate(lp1, mu = 0.5, sigma = 1.0);
    double v2 = evaluate(lp2, mu = 0.5, sigma = 1.0);
    expectNear(v1, v2, 1e-15);
  };

  "Model transform inference"_test = [] {
    // Different distributions should get different transforms:
    // - Normal -> unconstrained
    // - HalfNormal -> positive (log/exp)
    // - Beta -> unit interval (logit/sigmoid)

    auto mu = normal(0.0_c, 10.0_c);
    auto sigma = halfNormal(5.0_c);
    auto p = beta(2.0_c, 2.0_c);
    auto m = model(mu, sigma, p);

    auto posterior = m.posterior().build();

    // All transforms should be correctly inferred
    // z = [0, 0, 0] -> x = [0, 1, 0.5] (identity, exp, sigmoid)
    std::array<double, 3> z = {0.0, 0.0, 0.0};
    auto x = posterior.transform(z);

    expectNear(x[0], 0.0, 1e-10);   // mu: identity
    expectNear(x[1], 1.0, 1e-10);   // sigma: exp(0) = 1
    expectNear(x[2], 0.5, 1e-10);   // p: sigmoid(0) = 0.5
  };

  // ===========================================================================
  // Plate support tests
  // ===========================================================================

  "Model detects plates"_test = [] {
    struct Countries {};

    auto alpha = gamma(2.0_c, 0.1_c);
    auto theta = plate(beta(alpha, 3.0_c), Countries{});

    // Scalar-only model
    auto m1 = model(alpha);
    static_assert(!decltype(m1)::HasPlates);

    // Model with plate
    auto m2 = model(alpha, theta);
    static_assert(decltype(m2)::HasPlates);
  };

  "Model with plate - posterior builder"_test = [] {
    struct Countries {};

    auto alpha = gamma(2.0_c, 0.1_c);
    auto beta_param = gamma(2.0_c, 0.1_c);
    auto theta = plate(beta(alpha, beta_param), Countries{});

    auto m = model(alpha, beta_param, theta);

    // Build posterior with dimension
    auto posterior = m.posterior()
        .withDimension(Countries{}, 5)
        .build();

    // State: [z_alpha, z_beta, z_theta[0], ..., z_theta[4]]
    std::vector<double> z(7, 0.0);
    z[0] = std::log(3.0);   // alpha
    z[1] = std::log(10.0);  // beta
    for (std::size_t i = 0; i < 5; ++i) {
      z[2 + i] = 0.0;  // theta[i] = sigmoid(0) = 0.5
    }

    double lp = posterior.logProb(z);
    expectTrue(std::isfinite(lp));

    auto grad = posterior.gradient(z);
    expectEq(grad.size(), 7u);
    for (auto g : grad) {
      expectTrue(std::isfinite(g));
    }
  };

  "Model with plate - transform"_test = [] {
    struct Countries {};

    auto alpha = gamma(2.0_c, 0.1_c);
    auto theta = plate(beta(alpha, 3.0_c), Countries{});

    auto m = model(alpha, theta);
    auto posterior = m.posterior()
        .withDimension(Countries{}, 3)
        .build();

    // z = [log(2), 0, 0, 0] -> x = [2, 0.5, 0.5, 0.5]
    std::vector<double> z = {std::log(2.0), 0.0, 0.0, 0.0};
    auto x = posterior.transform(z);

    expectNear(x[0], 2.0, 1e-10);   // alpha: exp(log(2)) = 2
    expectNear(x[1], 0.5, 1e-10);   // theta[0]: sigmoid(0) = 0.5
    expectNear(x[2], 0.5, 1e-10);   // theta[1]: sigmoid(0) = 0.5
    expectNear(x[3], 0.5, 1e-10);   // theta[2]: sigmoid(0) = 0.5
  };

  return TestRegistry::result();
}
