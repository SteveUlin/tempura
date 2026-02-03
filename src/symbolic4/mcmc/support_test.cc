#include "symbolic4/mcmc/support.h"

#include <cmath>

#include "symbolic4/distributions/joint.h"
#include "unit.h"

using namespace tempura;
using namespace tempura::symbolic4;

auto main() -> int {
  // =========================================================================
  // Support inference
  // =========================================================================

  "HalfNormal has PositiveSupport"_test = [] {
    using Lit = Literal<double>;
    using Dist = HalfNormalDist<Lit>;
    using Support = distribution_support_t<Dist>;
    static_assert(std::is_same_v<Support, PositiveSupport>);
  };

  "Beta has UnitIntervalSupport"_test = [] {
    using Lit = Literal<double>;
    using Dist = BetaDist<Lit, Lit>;
    using Support = distribution_support_t<Dist>;
    static_assert(std::is_same_v<Support, UnitIntervalSupport>);
  };

  "Gamma has PositiveSupport"_test = [] {
    using Lit = Literal<double>;
    using Dist = GammaDist<Lit, Lit>;
    using Support = distribution_support_t<Dist>;
    static_assert(std::is_same_v<Support, PositiveSupport>);
  };

  "Normal has RealSupport"_test = [] {
    using Lit = Literal<double>;
    using Dist = NormalDist<Lit, Lit>;
    using Support = distribution_support_t<Dist>;
    static_assert(std::is_same_v<Support, RealSupport>);
  };

  "Exponential has PositiveSupport"_test = [] {
    using Lit = Literal<double>;
    using Dist = ExponentialDist<Lit>;
    using Support = distribution_support_t<Dist>;
    static_assert(std::is_same_v<Support, PositiveSupport>);
  };

  // =========================================================================
  // autoTransform inference
  // =========================================================================

  "autoTransform halfNormal → positive"_test = [] {
    auto sigma = halfNormal(2.0);
    auto t = autoTransform(sigma);

    static_assert(is_positive_v<decltype(t)>);
  };

  "autoTransform beta → unitInterval"_test = [] {
    auto theta = beta(2.0, 3.0);
    auto t = autoTransform(theta);

    static_assert(is_unit_interval_v<decltype(t)>);
  };

  "autoTransform gamma → positive"_test = [] {
    auto alpha = gamma(2.0, 1.0);
    auto t = autoTransform(alpha);

    static_assert(is_positive_v<decltype(t)>);
  };

  "autoTransform normal → unconstrained"_test = [] {
    auto mu = normal(0.0, 1.0);
    auto t = autoTransform(mu);

    static_assert(is_unconstrained_v<decltype(t)>);
  };

  "autoTransform preserves explicit transform"_test = [] {
    auto sigma = halfNormal(2.0);
    // User can override with explicit transform
    auto t = autoTransform(positive(sigma));

    static_assert(is_positive_v<decltype(t)>);
  };

  // =========================================================================
  // makeAutoTransformedPosterior
  // =========================================================================

  "makeAutoTransformedPosterior basic"_test = [] {
    auto mu = normal(0.0, 10.0);
    auto sigma = halfNormal(5.0);

    auto joint = logProb(mu, sigma);
    auto posterior = makeAutoTransformedPosterior(joint, mu, sigma).build();

    // sigma gets positive transform, mu gets unconstrained
    // Test with z values (unconstrained)
    std::array<double, 2> z = {1.0, 0.5};  // mu_z, sigma_z

    double lp = posterior.logProb(z);
    expectTrue(std::isfinite(lp));

    auto grad = posterior.gradient(z);
    expectTrue(std::isfinite(grad[0]));
    expectTrue(std::isfinite(grad[1]));
  };

  "makeAutoTransformedPosterior with observed"_test = [] {
    auto mu = normal(0.0, 10.0);
    auto sigma = halfNormal(5.0);
    auto y = normal(mu, sigma);

    auto joint = logProb(mu, sigma, y);
    auto posterior =
        makeAutoTransformedPosterior(joint, mu, sigma).observe(y = 3.5);

    std::array<double, 2> z = {0.0, 0.0};  // mu_z=0, sigma_z=0 → sigma=1

    double lp = posterior.logProb(z);
    expectTrue(std::isfinite(lp));
  };

  "makeAutoTransformedPosterior beta-binomial"_test = [] {
    auto theta = beta(2.0, 2.0);

    auto joint = logProb(theta);
    auto posterior = makeAutoTransformedPosterior(joint, theta).build();

    // theta gets unitInterval transform
    std::array<double, 1> z = {0.0};  // z=0 → theta=0.5

    double lp = posterior.logProb(z);
    expectTrue(std::isfinite(lp));

    // Transform back to constrained space
    double theta_val = posterior.transformOne<0>(z[0]);
    expectNear(theta_val, 0.5, 1e-10);
  };

  "Comparison: manual vs auto transforms"_test = [] {
    auto sigma = halfNormal(5.0);
    auto mu = normal(0.0, sigma);

    auto joint = logProb(sigma, mu);

    // Manual specification
    auto manual =
        makeTransformedPosterior(joint, positive(sigma), unconstrained(mu))
            .build();

    // Auto inference
    auto automatic = makeAutoTransformedPosterior(joint, sigma, mu).build();

    // Both should give same results
    std::array<double, 2> z = {0.5, 1.0};

    double lp_manual = manual.logProb(z);
    double lp_auto = automatic.logProb(z);

    expectNear(lp_manual, lp_auto, 1e-10);
  };

  return TestRegistry::result();
}
