#include "bayes/integrators.h"

#include <print>

#include "bayes/random.h"
#include "broadcast_array.h"
#include "unit.h"

using namespace tempura;

auto main() -> int {
  // Find the center of mass of a cut cylinder
  // z² + (√(x² + y²)- 3)² ≤ 1
  // x ≥ 1
  // y ≥ -3
  struct XYZ {
    double x;
    double y;
    double z;
  };
  auto sample = [gen = bayes::Rand()] mutable {
    constexpr double scale = 1.0 / static_cast<double>(gen.max() - gen.min());
    return XYZ{.x = static_cast<double>(gen()) * scale * 3.0 + 1.0,
               .y = static_cast<double>(gen()) * scale * 7.0 - 3.0,
               .z = static_cast<double>(gen()) * scale * 2.0 - 1.0};
  };
  auto func = [](const XYZ& arg) {
    const auto& [x, y, z] = arg;
    double t = std::sqrt(x * x + y * y) - 3.;
    if (z * z + t * t <= 1.0) {
      return BroadcastArray<double, 3>(x, y, z);
    }
    return BroadcastArray<double, 3>(0., 0., 0.);
  };

  bayes::MonteCarloIntegrator integrator{std::move(func), std::move(sample), 3. * 7. * 2.};
  integrator.step(10'000'000);
  auto res = integrator.result();
  auto var = integrator.variance();
  std::println("result: {} {} {}", res[0], res[1], res[2]);
  std::println("variance: {} {} {}", var[0], var[1], var[2]);

  return TestRegistry::result();
}
