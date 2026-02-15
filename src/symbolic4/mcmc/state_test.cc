#include "symbolic4/mcmc/samples.h"

#include <random>

#include "symbolic4/constants.h"
#include "symbolic4/distributions/collect_log_prob.h"
#include "symbolic4/distributions/random_var.h"
#include "symbolic4/indexed/indexed.h"
#include "symbolic4/mcmc/plate_transforms.h"
#include "symbolic4/mcmc/sampler.h"
#include "unit.h"

using namespace tempura;
using namespace tempura::symbolic4;

namespace {
struct TestDim {};
}  // namespace

auto main() -> int {
  "Statistics helpers"_test = [] {
    std::vector<double> values{1.0, 2.0, 3.0, 4.0, 5.0};

    expectNear(3.0, mean(values), 1e-10);
    // variance = ((1-3)^2 + (2-3)^2 + (3-3)^2 + (4-3)^2 + (5-3)^2) / 5 = 10/5 = 2
    expectNear(2.0, variance(values), 1e-10);
    expectNear(std::sqrt(2.0), sd(values), 1e-10);
    expectNear(3.0, median(values), 1e-10);
    expectNear(1.0, quantile(values, 0.0), 1e-10);
    expectNear(5.0, quantile(values, 1.0), 1e-10);
  };

  // =========================================================================
  // Samples: expression evaluation, draw access, iteration
  // =========================================================================

  "Samples[expr] evaluates expression across draws"_test = [] {
    auto a = normal(0_c, 10_c);
    auto sigma = exponential(1_c);
    auto z_b = plate(normal(0_c, 1_c), TestDim{});

    auto joint_lp = collectLogProbs(a, sigma, z_b);
    auto posterior = makePlateTransformedPosterior(joint_lp, a, sigma, z_b)
                         .withDimension(TestDim{}, 3)
                         .build();

    std::mt19937_64 rng{42};
    auto samples = posterior.sample(
        HmcConfig{.epsilon = 0.1, .steps = 10, .warmup = 10, .draws = 5},
        BinderPack{a = 0.0, sigma = 1.0, z_b = std::vector<double>{0.0, 0.0, 0.0}},
        rng);

    // Evaluate arithmetic expression: a + sigma (scalar)
    auto expr_draws = samples[a + sigma];
    expectEq(5u, expr_draws.size());

    // Each draw should equal a_val + sigma_val
    auto a_draws = samples[a];
    auto sigma_draws = samples[sigma];
    for (std::size_t i = 0; i < 5; ++i) {
      expectNear(a_draws[i] + sigma_draws[i], expr_draws[i], 1e-10);
    }
  };

  "Samples[i] returns queryable SymbolicState"_test = [] {
    auto a = normal(0_c, 10_c);
    auto sigma = exponential(1_c);
    auto z_b = plate(normal(0_c, 1_c), TestDim{});

    auto joint_lp = collectLogProbs(a, sigma, z_b);
    auto posterior = makePlateTransformedPosterior(joint_lp, a, sigma, z_b)
                         .withDimension(TestDim{}, 3)
                         .build();

    std::mt19937_64 rng{42};
    auto samples = posterior.sample(
        HmcConfig{.epsilon = 0.1, .steps = 10, .warmup = 10, .draws = 5},
        BinderPack{a = 0.0, sigma = 1.0, z_b = std::vector<double>{0.0, 0.0, 0.0}},
        rng);

    // Access draw 0 as SymbolicState
    auto draw = samples[std::size_t{0}];

    // Scalar param access
    double a_val = draw[a];
    auto a_draws = samples[a];
    expectNear(a_draws[0], a_val, 1e-10);

    // Indexed param access — returns span
    auto z_span = draw[z_b];
    expectEq(3u, z_span.size());
  };

  "Samples range-for iteration"_test = [] {
    auto a = normal(0_c, 10_c);
    auto sigma = exponential(1_c);

    auto joint_lp = collectLogProbs(a, sigma);
    auto posterior = makePlateTransformedPosterior(joint_lp, a, sigma).build();

    std::mt19937_64 rng{42};
    auto samples = posterior.sample(
        HmcConfig{.epsilon = 0.1, .steps = 10, .warmup = 10, .draws = 5},
        BinderPack{a = 0.0, sigma = 1.0},
        rng);

    double sum_a = 0.0;
    std::size_t count = 0;
    for (const auto& draw : samples) {
      sum_a += draw[a];
      ++count;
    }
    expectEq(5u, count);

    // Sum should match manual computation
    auto a_draws = samples[a];
    double expected_sum = 0.0;
    for (double v : a_draws) expected_sum += v;
    expectNear(expected_sum, sum_a, 1e-10);
  };

  return TestRegistry::result();
}
