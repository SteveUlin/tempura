#include "quadature/gaussian.h"

#include <print>
#include <random>
#include <utility>

#include "unit.h"

using namespace tempura;
auto main() -> int {

  "Gauss Legendre"_test = [] {
    const auto weights = quadature::gaussLegendre(0.0, 2.0, 5);
    double result = 0.0;
    for (const auto& [abscissa, weight] : weights) {
      result += weight;
    }
    expectNear(2.0, result);
  };

  "Gauss Legendre -- x²"_test = []  {
    const auto weights = quadature::gaussLegendre(0.0, 2.0, 8);
    double result = 0.0;
    for (const auto& [abscissa, weight] : weights) {
      result += weight * abscissa * abscissa;
    }
    expectNear(8.0 / 3.0, result);
  };

  return 0;
}
