// Logistic Regression with Bayesian Inference using symbolic4 plate API
//
// Demonstrates:
// - Defining a logistic regression model with symbolic priors AND likelihood
// - Using plate<Obs>() for vectorized observations
// - Using data<Obs>() for covariates
// - Automatic gradients through the entire model
//
// Model:
//   alpha ~ Normal(0, 5)       # intercept prior
//   beta ~ Normal(0, 2.5)      # slope prior
//   p[i] = sigmoid(alpha + beta * x[i])
//   y[i] ~ Bernoulli(p[i])
//

#include <cmath>
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

// Symbolic logistic (sigmoid) function: 1 / (1 + exp(-z))
template <Symbolic Z>
constexpr auto logistic(Z z) {
  return 1_c / (1_c + exp(-z));
}

// Numerically stable sigmoid for final predictions
auto sigmoid(double x) -> double {
  if (x >= 0) {
    return 1.0 / (1.0 + std::exp(-x));
  }
  double exp_x = std::exp(x);
  return exp_x / (1.0 + exp_x);
}

auto main() -> int {
  std::cout << "=================================================\n";
  std::cout << "Bayesian Logistic Regression with symbolic4 plates\n";
  std::cout << "=================================================\n\n";

  // --------------------------------------------------------------------------
  // Generate synthetic data: binary classification
  // --------------------------------------------------------------------------
  constexpr double kTrueAlpha = -1.0;
  constexpr double kTrueBeta = 3.0;
  constexpr std::size_t kNumPoints = 100;

  std::mt19937_64 rng{42};
  std::uniform_real_distribution<double> uniform_dist{0.0, 1.0};

  std::vector<double> x_data(kNumPoints);
  std::vector<double> y_data(kNumPoints);

  for (std::size_t i = 0; i < kNumPoints; ++i) {
    // x uniformly distributed in [-2, 2]
    x_data[i] = 4.0 * (static_cast<double>(i) / static_cast<double>(kNumPoints - 1)) - 2.0;

    // y ~ Bernoulli(sigmoid(alpha + beta * x))
    double p = sigmoid(kTrueAlpha + kTrueBeta * x_data[i]);
    y_data[i] = (uniform_dist(rng) < p) ? 1.0 : 0.0;
  }

  // Count outcomes
  std::size_t num_ones = 0;
  for (double y : y_data) {
    if (y > 0.5) ++num_ones;
  }

  std::cout << "True parameters:\n";
  std::cout << std::format("  alpha = {:.2f}\n", kTrueAlpha);
  std::cout << std::format("  beta  = {:.2f}\n", kTrueBeta);
  std::cout << std::format("  N     = {} ({} ones, {} zeros)\n\n",
                           kNumPoints, num_ones, kNumPoints - num_ones);

  // --------------------------------------------------------------------------
  // Define the model using symbolic4 with plates
  // --------------------------------------------------------------------------
  struct Obs {};  // Dimension tag for observations

  // Priors (both unconstrained, so transform is identity)
  auto alpha = normal(0.0, 5.0);
  auto beta = normal(0.0, 2.5);

  // Covariate (external data, not a parameter)
  auto x = data<Obs>();

  // Likelihood - plate over observations
  // alpha, beta, x are automatically converted to their symbolic forms
  // logistic(eta) = 1 / (1 + exp(-eta))
  auto y = plate<Obs>(bernoulli(logistic(alpha + beta * x)));

  // Build posterior with automatic transforms and gradients
  // .bind() accepts both data (x) and observations (y)
  auto posterior = infer(alpha, beta, y)
      .bind(x = indexed(x_data), y = indexed(y_data));

  std::cout << "Model defined using plate API.\n";
  std::cout << "  Priors + likelihood handled by infer() with automatic transforms.\n";
  std::cout << "  Gradients computed automatically via symbolic differentiation.\n\n";

  // --------------------------------------------------------------------------
  // Run HMC sampling with binding-centric API
  // --------------------------------------------------------------------------
  std::cout << "Running HMC sampling...\n";

  auto samples = posterior.sample(
      HmcConfig{.epsilon = 0.05, .steps = 15, .warmup = 500, .draws = 1000},
      BinderPack{alpha = 0.0, beta = 0.0},  // init in unconstrained space
      rng);

  std::cout << std::format("Collected {} samples\n\n", samples.size());

  // --------------------------------------------------------------------------
  // Compute posterior summaries using symbol-based access
  // --------------------------------------------------------------------------
  double mean_alpha = mean(samples[alpha]);
  double mean_beta = mean(samples[beta]);
  double sd_alpha = sd(samples[alpha]);
  double sd_beta = sd(samples[beta]);

  std::cout << "Posterior estimates:\n";
  std::cout << std::format("  alpha: {:.3f} +/- {:.3f}  (true: {:.2f})\n",
                           mean_alpha, sd_alpha, kTrueAlpha);
  std::cout << std::format("  beta:  {:.3f} +/- {:.3f}  (true: {:.2f})\n",
                           mean_beta, sd_beta, kTrueBeta);

  // --------------------------------------------------------------------------
  // Posterior predictive: decision boundary
  // --------------------------------------------------------------------------
  // Decision boundary: p = 0.5, so alpha + beta * x = 0, x = -alpha/beta
  double boundary = -mean_alpha / mean_beta;
  double true_boundary = -kTrueAlpha / kTrueBeta;

  std::cout << std::format("\nDecision boundary (x where p=0.5):\n");
  std::cout << std::format("  Estimated: {:.3f}\n", boundary);
  std::cout << std::format("  True:      {:.3f}\n", true_boundary);

  // --------------------------------------------------------------------------
  // Classification accuracy on training data
  // --------------------------------------------------------------------------
  std::size_t correct = 0;
  for (std::size_t i = 0; i < kNumPoints; ++i) {
    double p = sigmoid(mean_alpha + mean_beta * x_data[i]);
    double predicted = (p >= 0.5) ? 1.0 : 0.0;
    if (predicted == y_data[i]) ++correct;
  }

  double accuracy = 100.0 * static_cast<double>(correct) / static_cast<double>(kNumPoints);
  std::cout << std::format("\nClassification accuracy: {:.1f}%\n", accuracy);

  std::cout << "\n=================================================\n";
  std::cout << "Logistic regression example complete.\n";
  std::cout << "=================================================\n";

  return 0;
}
