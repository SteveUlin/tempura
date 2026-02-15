// Minimal test: gather with infer
#include "symbolic4/infer.h"
#include "symbolic4/indexed/data.h"
#include "unit.h"

#include <vector>

using namespace tempura;
using namespace tempura::symbolic4;

auto main() -> int {
  struct Groups {};
  struct Obs {};

  // Simple model: a_bar + sigma * z[idx]
  auto a_bar = normal(0_c, 1_c);
  auto sigma = exponential(1_c);
  auto z = plate(normal(0.0_c, 1.0_c), Groups{});
  auto idx = data(Obs{});

  auto group_effect = a_bar + sigma * z[idx];  // Natural bracket syntax!

  auto p = 1_c / (1_c + exp(-group_effect));
  auto y = plate(bernoulli(p), Obs{});

  auto posterior = inferExplicit(a_bar, sigma, z, y);

  // Bind data: 2 groups (0 and 1), 4 observations
  std::vector<double> idx_data = {0.0, 1.0, 0.0, 1.0};
  std::vector<double> y_data = {1.0, 0.0, 1.0, 1.0};

  auto bound_posterior = posterior
      .withDimension(Groups{}, 2)  // Must set Groups dimension explicitly
      .bind(idx = indexed(idx_data), y = indexed(y_data));

  // Verify state dimension: 2 scalar + 2 indexed
  expectEq(bound_posterior.stateDim(), std::size_t{4});

  // Try evaluating log-prob
  std::vector<double> state(bound_posterior.stateDim(), 0.0);
  state[1] = -0.69;  // log(0.5) for sigma
  double lp = bound_posterior.logProb(state);
  expectTrue(std::isfinite(lp));

  return TestRegistry::result();
}
