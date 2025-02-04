#include "quadature/improper.h"

#include <print>
#include <random>
#include <utility>

#include "unit.h"

using namespace tempura;
using namespace tempura::quadature;

struct Integrand {
  std::function<double(double)> func;
  double a;
  double b;
  double ans;
};

auto genPolynomial(std::uniform_random_bit_generator auto& rng) -> Integrand {
  const double a = std::uniform_real_distribution{-4.0, 4.0}(rng);
  const double b = std::uniform_real_distribution{-4.0, 4.0}(rng);
  double ans = 0;

  size_t length = std::uniform_int_distribution{1, 8}(rng);
  std::vector<std::pair<double, double>> data;
  data.reserve(length);
  for (size_t i = 0; i < length; ++i) {
    const int64_t power = std::uniform_int_distribution{0, 4}(rng);
    const double coeff = std::uniform_real_distribution{-4.0, 4.0}(rng);
    data.emplace_back(coeff, power);
    ans += coeff * (std::pow(b, power + 1) - std::pow(a, power + 1)) / (power + 1);
  }

  return Integrand{
      .func =
          [data](double x) {
            double result = 0;
            for (const auto& [coeff, power] : data) {
              result += coeff * std::pow(x, power);
            }
            return result;
          },
      .a = a,
      .b = b,
      .ans = ans,
  };
}

template <typename Integrator, typename... Args>
void fuzzTest(Args&&... args) {
  std::default_random_engine rng(1337);

  for (size_t i = 0; i < 1; ++i) {
    auto [func, a, b, ans] = genPolynomial(rng);
    Integrator integrator{args..., std::move(func), a, b};
    double result = integrator.result();
    for (size_t j = 0; j < 20; ++j) {
      integrator.refine();
      const double next = integrator.result();
      const double diff = std::abs((next - result) / next);
      result = next;
      std::println("{}, diff: {}", result, diff);
      if (diff < 1e-6) {
        break;
      }
    }
    expectLessThan(std::abs((result - ans) / ans), 1e-5);
  }
};

// Returns the number of calls to .refine that it took to converges.
template <typename Integrator>
auto testConvergence(Integrator integrator) -> int64_t {
  int64_t calls = 0;
  auto result = integrator.result();
  for (int64_t i = 0; i < 20; ++i) {
    ++calls;
    integrator.refine();
    const auto next = integrator.result();
    const auto diff = std::abs((next - result) / next);
    result = next;
    std::println("{}", result);
    if (diff < 1e-4) {
      std::println("{}", result);
      break;
    }
  }
  return calls;
}

auto main() -> int {
  "fuzz midpoint"_test = [] {
    fuzzTest<MidpointIntegrator<double, std::function<double(double)>>>();
  };

  "convergence midpoint"_test = [] {
    int64_t count =
        testConvergence(MidpointIntegrator([](double x) { return std::pow(x, -0.5); }, 0.0, 1.0));

    std::println("Convergence: {}", count);
    count = testConvergence(
        RombergMidpointIntegrator(5, [](double x) { return std::pow(x, -0.5); }, 0.0, 1.0));

    std::println("Convergence: {}", count);

    count = testConvergence(MidpointInfIntegrator{[](double x) { return std::pow(x, -4); }, 1.0});
    std::println("Convergence: {}", count);

    count = testConvergence(
        MidpointSqrtIntegrator{[](double x) { return 1.0 / std::sqrt(x); }, 0.0, 1.0});
    std::println("Convergence: {}", count);
  };

  return TestRegistry::result();
}
