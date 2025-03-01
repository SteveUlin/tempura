#include "quadature/gaussian.h"

#include <bits/ranges_algo.h>

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

  "Gauss Legendre -- x²"_test = [] {
    const auto weights = quadature::gaussLegendre(0.0, 2.0, 8);
    double result = 0.0;
    for (const auto& [abscissa, weight] : weights) {
      result += weight * abscissa * abscissa;
    }
    expectNear(8.0 / 3.0, result);
  };

  "Gauss Laguerre"_test = [] {
    const auto weights = quadature::gaussLaguerre<double>(2.0, 5);
    double result = 0.0;
    for (const auto& [abscissa, weight] : weights) {
      result += weight;
    }
    expectNear(2.0, result);
  };

  "Gauss Hermite"_test = [] {
    const double result = std::ranges::fold_left(
        quadature::gaussHermite<double>(5) |
            std::ranges::views::transform(
                [](const auto& val) { return val.weight; }),
        0.0, std::plus<>{});
    expectNear(1.77245, result);

    const double result2 = std::ranges::fold_left(
        quadature::gaussHermite<double>(5) |
            std::ranges::views::transform(
                [](const auto& val) { return val.weight * val.abscissa; }),
        0.0, std::plus<>{});
    expectNear(0.0, result2);
  };

  return 0;
}
