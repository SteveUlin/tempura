// Tests for symbolic4/distributions/observed.h - Observed data conditioning
#include "symbolic4/distributions/distributions.h"
#include "symbolic4/distributions/observed.h"
#include "symbolic4/indexed/indexed.h"
#include "unit.h"

#include <cmath>
#include <vector>

using namespace tempura;
using namespace tempura::symbolic4;

auto main() -> int {
  // ===========================================================================
  // Observed type traits
  // ===========================================================================

  "observe creates Observed wrapper"_test = [] {
    struct Obs {};
    auto theta = plate<Obs>(beta(2.0_c, 3.0_c));

    std::vector<double> data = {0.3, 0.5, 0.7};
    auto theta_obs = observe(theta, data);

    static_assert(is_observed_v<decltype(theta_obs)>);
    static_assert(!is_observed_v<decltype(theta)>);
  };

  "Observed preserves node type"_test = [] {
    struct Obs {};
    auto theta = plate<Obs>(normal(0.0_c, 1.0_c));

    std::vector<double> data = {1.0, 2.0};
    auto theta_obs = observe(theta, data);

    using NodeType = typename decltype(theta_obs)::node_type;
    static_assert(is_indexed_random_var_v<NodeType>);
  };

  // ===========================================================================
  // makeObservedBinding
  // ===========================================================================

  "makeObservedBinding creates correct 1D binding"_test = [] {
    struct Obs {};
    auto theta = plate<Obs>(beta(2.0_c, 3.0_c));

    std::vector<double> data = {0.3, 0.5, 0.7};
    auto theta_obs = observe(theta, data);
    auto binding = makeObservedBinding(theta_obs);

    expectEq(binding.size(), 3UL);
    expectNear(0.3, binding.at(0), 1e-10);
    expectNear(0.5, binding.at(1), 1e-10);
    expectNear(0.7, binding.at(2), 1e-10);
  };

  // ===========================================================================
  // observedLogProb
  // ===========================================================================

  "observedLogProb returns log-prob expression"_test = [] {
    struct Obs {};
    auto theta = plate<Obs>(beta(2.0_c, 3.0_c));

    std::vector<double> data = {0.3, 0.5, 0.7};
    auto theta_obs = observe(theta, data);

    auto lp = observedLogProb(theta_obs);
    static_assert(Symbolic<decltype(lp)>);
    static_assert(is_sum_over_v<decltype(lp)>);
  };

  "observed plate log-prob evaluation"_test = [] {
    struct Obs {};
    auto theta = plate<Obs>(beta(2.0_c, 3.0_c));

    std::vector<double> data = {0.3, 0.5, 0.7};
    auto theta_obs = observe(theta, data);

    auto lp = observedLogProb(theta_obs);
    auto binding = makeObservedBinding(theta_obs);

    double result = evaluateIndexed(lp, BinderPack{binding});

    // Full normalized log Beta(x; 2, 3) = log(x) + 2*log(1-x) - lbeta(2, 3)
    // lbeta(2,3) = lgamma(2) + lgamma(3) - lgamma(5)
    double log_normalizer = std::lgamma(2.0) + std::lgamma(3.0) - std::lgamma(5.0);
    auto expected_at = [log_normalizer](double x) {
      return std::log(x) + 2.0 * std::log(1.0 - x) - log_normalizer;
    };
    double expected = expected_at(0.3) + expected_at(0.5) + expected_at(0.7);
    expectNear(expected, result, 1e-10);
  };

  // ===========================================================================
  // Observed with scalar parameters
  // ===========================================================================

  "observed with scalar parameter"_test = [] {
    struct Obs {};
    auto alpha = halfNormal(5_c);
    auto theta = plate<Obs>(beta(alpha, 3.0_c));

    std::vector<double> data = {0.3, 0.5, 0.7};
    auto theta_obs = observe(theta, data);

    auto lp = observedLogProb(theta_obs);
    auto binding = makeObservedBinding(theta_obs);

    // Evaluate with both scalar and observed bindings
    double result = evaluateIndexed(lp, BinderPack{alpha = 2.0, binding});

    // alpha=2, beta=3: full normalized log Beta
    double log_normalizer = std::lgamma(2.0) + std::lgamma(3.0) - std::lgamma(5.0);
    auto expected_at = [log_normalizer](double x) {
      return std::log(x) + 2.0 * std::log(1.0 - x) - log_normalizer;
    };
    double expected = expected_at(0.3) + expected_at(0.5) + expected_at(0.7);
    expectNear(expected, result, 1e-10);
  };

  "observed normal distribution"_test = [] {
    struct Obs {};
    auto mu = normal(0_c, 10_c);
    auto y = plate<Obs>(normal(mu, 1.0_c));

    std::vector<double> data = {1.0, 2.0, 3.0};
    auto y_obs = observe(y, data);

    auto lp = observedLogProb(y_obs);
    auto binding = makeObservedBinding(y_obs);

    // Evaluate at mu = 2.0
    double result = evaluateIndexed(lp, BinderPack{mu = 2.0, binding});

    // Full log Normal(x; mu, sigma) = -0.5 * z^2 - log(sigma) - 0.5*log(2*pi)
    // For sigma = 1: -0.5 * (x - mu)^2 - 0 - 0.9189385332046727
    constexpr double kLogSqrt2Pi = 0.9189385332046727;
    auto expected_at = [](double x, double m) {
      return -0.5 * (x - m) * (x - m) - kLogSqrt2Pi;
    };
    double expected = expected_at(1.0, 2.0) + expected_at(2.0, 2.0) + expected_at(3.0, 2.0);
    expectNear(expected, result, 1e-10);
  };

  return TestRegistry::result();
}
