// Linear Regression with Bayesian Inference using symbolic4 plate API
//
// Demonstrates:
// - Defining a linear regression model with symbolic priors AND likelihood
// - Using plate<Obs>() for vectorized observations
// - Using data<Obs>() for covariates
// - Automatic gradients through the entire model
//
// Model:
//   alpha ~ Normal(0, 10)      # intercept prior
//   beta ~ Normal(0, 5)        # slope prior
//   sigma ~ HalfNormal(2)      # noise prior
//   y[i] ~ Normal(alpha + beta * x[i], sigma)
//

#include <format>
#include <iostream>
#include <random>
#include <vector>

#include "symbolic4/indexed/data.h"
#include "symbolic4/indexed/indexed.h"
#include "symbolic4/infer.h"
#include "symbolic4/mcmc/samples.h"
#include "symbolic4/mcmc/sampler.h"

using namespace tempura;
using namespace tempura::symbolic4;

auto main() -> int {
  std::cout << "=================================================\n";
  std::cout << "Bayesian Linear Regression with symbolic4 plates\n";
  std::cout << "=================================================\n\n";

  // --------------------------------------------------------------------------
  // Generate synthetic data: y = 2 + 3*x + noise
  // --------------------------------------------------------------------------
  constexpr double kTrueAlpha = 2.0;
  constexpr double kTrueBeta = 3.0;
  constexpr double kTrueSigma = 0.5;
  constexpr std::size_t kNumPoints = 50;

  std::mt19937_64 rng{42};
  std::normal_distribution<double> noise{0.0, kTrueSigma};

  std::vector<double> x_data(kNumPoints);
  std::vector<double> y_data(kNumPoints);

  for (std::size_t i = 0; i < kNumPoints; ++i) {
    x_data[i] = static_cast<double>(i) / static_cast<double>(kNumPoints - 1);
    y_data[i] = kTrueAlpha + kTrueBeta * x_data[i] + noise(rng);
  }

  std::cout << "True parameters:\n";
  std::cout << std::format("  alpha = {:.2f}\n", kTrueAlpha);
  std::cout << std::format("  beta  = {:.2f}\n", kTrueBeta);
  std::cout << std::format("  sigma = {:.2f}\n", kTrueSigma);
  std::cout << std::format("  N     = {}\n\n", kNumPoints);

  // --------------------------------------------------------------------------
  // Define the model using symbolic4 with plates
  // --------------------------------------------------------------------------
  struct Obs {};  // Dimension tag for observations

  // Priors
  auto alpha = normal(0.0, 10.0);
  auto beta = normal(0.0, 5.0);
  auto sigma = halfNormal(2.0);

  // Covariate (external data, not a parameter)
  auto x = data<Obs>();

  // Likelihood - plate over observations
  // alpha, beta, sigma, x are automatically converted to their symbolic forms
  // For sigma (HalfNormal), this is exp(z); for Normal params, it's just z
  auto y = plate<Obs>(normal(alpha + beta * x, sigma));

  // Build posterior with automatic transforms and gradients
  // infer(y) automatically discovers alpha, beta, sigma from y's expression tree
  auto posterior = infer(y)
      .bind(x = indexed(x_data), y = indexed(y_data));

  std::cout << "Model defined using plate API.\n";
  std::cout << "  Priors + likelihood handled by infer() with automatic transforms.\n";
  std::cout << "  Gradients computed automatically via symbolic differentiation.\n\n";

  // --------------------------------------------------------------------------
  // Run HMC sampling with new binding-centric API
  // --------------------------------------------------------------------------
  std::cout << "Running HMC sampling...\n";

  auto samples = posterior.sample(
      HmcConfig{.epsilon = 0.01, .steps = 20, .warmup = 500, .draws = 1000},
      BinderPack{alpha = 0.0, beta = 0.0, sigma = 0.0},  // init in unconstrained space
      rng);

  std::cout << std::format("Collected {} samples\n\n", samples.size());

  // --------------------------------------------------------------------------
  // Compute posterior summaries using symbol-based access
  // --------------------------------------------------------------------------
  std::cout << "Posterior estimates:\n";
  std::cout << std::format("  alpha: {:.3f} +/- {:.3f}  (true: {:.2f})\n",
                           mean(samples[alpha]), sd(samples[alpha]), kTrueAlpha);
  std::cout << std::format("  beta:  {:.3f} +/- {:.3f}  (true: {:.2f})\n",
                           mean(samples[beta]), sd(samples[beta]), kTrueBeta);
  std::cout << std::format("  sigma: {:.3f} +/- {:.3f}  (true: {:.2f})\n",
                           mean(samples[sigma]), sd(samples[sigma]), kTrueSigma);

  std::cout << "\n=================================================\n";
  std::cout << "Linear regression example complete.\n";
  std::cout << "=================================================\n";

  return 0;
}
