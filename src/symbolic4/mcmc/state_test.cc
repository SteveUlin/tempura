#include "symbolic4/mcmc/state.h"

#include <random>

#include "symbolic4/distributions/random_var.h"
#include "symbolic4/indexed/data.h"
#include "symbolic4/indexed/indexed.h"
#include "symbolic4/infer.h"
#include "symbolic4/mcmc/samples.h"
#include "symbolic4/mcmc/sampler.h"
#include "unit.h"

using namespace tempura;
using namespace tempura::symbolic4;

auto main() -> int {
  "ParameterState basic construction"_test = [] {
    auto alpha = normal(0.0, 10.0);
    auto beta = normal(0.0, 5.0);

    // Construct from bindings
    auto state = ParameterState<decltype(alpha), decltype(beta)>{
        BinderPack{alpha = 1.5, beta = 2.5}};

    expectNear(1.5, state[alpha], 1e-10);
    expectNear(2.5, state[beta], 1e-10);
  };

  "ParameterState array access"_test = [] {
    auto alpha = normal(0.0, 10.0);
    auto beta = normal(0.0, 5.0);

    auto state = ParameterState<decltype(alpha), decltype(beta)>{
        BinderPack{alpha = 1.0, beta = 2.0}};

    // Array access
    auto& arr = state.array();
    expectNear(1.0, arr[0], 1e-10);
    expectNear(2.0, arr[1], 1e-10);

    // Modify via symbol
    state[alpha] = 3.0;
    expectNear(3.0, arr[0], 1e-10);
  };

  "ParameterState from array"_test = [] {
    auto alpha = normal(0.0, 10.0);
    auto beta = normal(0.0, 5.0);

    std::array<double, 2> arr{5.0, 6.0};
    auto state = ParameterState<decltype(alpha), decltype(beta)>{arr};

    expectNear(5.0, state[alpha], 1e-10);
    expectNear(6.0, state[beta], 1e-10);
  };

  "Samples container basic"_test = [] {
    auto alpha = normal(0.0, 10.0);
    auto beta = normal(0.0, 5.0);

    Samples<decltype(alpha), decltype(beta)> samples;
    samples.push_back({1.0, 2.0});
    samples.push_back({1.1, 2.1});
    samples.push_back({0.9, 1.9});

    expectEq(3u, samples.size());

    // Access by parameter
    auto alpha_draws = samples[alpha];
    expectEq(3u, alpha_draws.size());
    expectNear(1.0, alpha_draws[0], 1e-10);
    expectNear(1.1, alpha_draws[1], 1e-10);
    expectNear(0.9, alpha_draws[2], 1e-10);

    auto beta_draws = samples[beta];
    expectNear(2.0, beta_draws[0], 1e-10);
    expectNear(2.1, beta_draws[1], 1e-10);
    expectNear(1.9, beta_draws[2], 1e-10);
  };

  "Samples single draw access"_test = [] {
    auto alpha = normal(0.0, 10.0);
    auto beta = normal(0.0, 5.0);

    Samples<decltype(alpha), decltype(beta)> samples;
    samples.push_back({1.0, 2.0});
    samples.push_back({3.0, 4.0});

    auto draw0 = samples[0];
    expectNear(1.0, draw0[alpha], 1e-10);
    expectNear(2.0, draw0[beta], 1e-10);

    auto draw1 = samples[1];
    expectNear(3.0, draw1[alpha], 1e-10);
    expectNear(4.0, draw1[beta], 1e-10);
  };

  "Samples iteration"_test = [] {
    auto alpha = normal(0.0, 10.0);
    auto beta = normal(0.0, 5.0);

    Samples<decltype(alpha), decltype(beta)> samples;
    samples.push_back({1.0, 10.0});
    samples.push_back({2.0, 20.0});
    samples.push_back({3.0, 30.0});

    double sum_alpha = 0.0;
    double sum_beta = 0.0;
    for (const auto& draw : samples) {
      sum_alpha += draw[alpha];
      sum_beta += draw[beta];
    }

    expectNear(6.0, sum_alpha, 1e-10);
    expectNear(60.0, sum_beta, 1e-10);
  };

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

  "posterior.sample() with linear model"_test = [] {
    struct Obs {};

    // Simple model: y ~ Normal(alpha, sigma)
    auto alpha = normal(0.0, 10.0);
    auto sigma = halfNormal(2.0);
    auto y = plate<Obs>(normal(alpha, sigma));

    // Data: just a few observations around 5.0
    std::vector<double> y_data{4.8, 5.0, 5.2, 4.9, 5.1};

    auto posterior = infer(alpha, sigma, y)
        .bind(y = indexed(y_data));

    std::mt19937_64 rng{42};

    // Run sampling
    auto samples = posterior.sample(
        HmcConfig{.epsilon = 0.05, .steps = 10, .warmup = 100, .draws = 200},
        BinderPack{alpha = 0.0, sigma = 0.0},  // init in unconstrained space
        rng);

    expectEq(200u, samples.size());

    // Check that posterior mean for alpha is close to 5.0 (the data mean)
    auto alpha_draws = samples[alpha];
    double alpha_mean = mean(alpha_draws);
    // With limited samples, we allow some tolerance
    expectTrue(alpha_mean > 4.0 && alpha_mean < 6.0);

    // Sigma should be positive and reasonable
    auto sigma_draws = samples[sigma];
    double sigma_mean = mean(sigma_draws);
    expectTrue(sigma_mean > 0.0 && sigma_mean < 2.0);
  };

  "logProbAt with bindings"_test = [] {
    struct Obs {};

    auto alpha = normal(0.0, 10.0);
    auto sigma = halfNormal(2.0);
    auto y = plate<Obs>(normal(alpha, sigma));

    std::vector<double> y_data{4.8, 5.0, 5.2, 4.9, 5.1};

    auto posterior = infer(alpha, sigma, y)
        .bind(y = indexed(y_data));

    // Evaluate log-prob using binding syntax
    double lp1 = posterior.logProbAt(BinderPack{alpha = 5.0, sigma = 0.0});

    // Compare with array-based evaluation
    std::array<double, 2> z{5.0, 0.0};
    double lp2 = posterior.logProb(z);

    expectNear(lp1, lp2, 1e-10);
  };

  "gradientAt with bindings"_test = [] {
    struct Obs {};

    auto alpha = normal(0.0, 10.0);
    auto sigma = halfNormal(2.0);
    auto y = plate<Obs>(normal(alpha, sigma));

    std::vector<double> y_data{4.8, 5.0, 5.2, 4.9, 5.1};

    auto posterior = infer(alpha, sigma, y)
        .bind(y = indexed(y_data));

    // Evaluate gradient using binding syntax
    auto grad = posterior.gradientAt(BinderPack{alpha = 5.0, sigma = 0.0});

    // Query by symbol
    double d_alpha = grad[alpha];
    double d_sigma = grad[sigma];

    // Compare with array-based evaluation
    std::array<double, 2> z{5.0, 0.0};
    auto grad_arr = posterior.gradient(z);

    expectNear(d_alpha, grad_arr[0], 1e-10);
    expectNear(d_sigma, grad_arr[1], 1e-10);
  };

  return TestRegistry::result();
}
