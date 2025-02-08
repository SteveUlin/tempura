#include "quadature/newton_cotes.h"
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

  for (size_t i = 0; i < 1'000; ++i) {
    auto [func, a, b, ans] = genPolynomial(rng);
    Integrator integrator{args..., std::move(func), a, b};
    double result = integrator.result();
    for (size_t j = 0; j < 20; ++j) {
      integrator.refine();
      const double next = integrator.result();
      const double diff = std::abs((next - result) / next);
      result = next;
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
  for (int64_t i = 0; i < 30; ++i) {
    ++calls;
    integrator.refine();
    const auto next = integrator.result();
    const auto diff = std::abs((next - result) / next);
    result = next;
    if (diff < 1e-10) {
      std::println("{}", result - 8.1533641198111650205387451810911930652);
      break;
    }
  }
  return calls;
}

auto main() -> int {
  "fuzz trap"_test = [] {
    fuzzTest<TrapazoidalIntegrator<double, std::function<double(double)>>>();
  };

  "fuzz simpson"_test = [] {
    fuzzTest<SimpsonIntegrator<double, std::function<double(double)>>>();
  };

  "fuzz romberg"_test = [] {
    fuzzTest<RombergIntegrator<double, std::function<double(double)>>>(5);
  };

  "convergence"_test = [] {
    auto func = [](double x) {
      if (x == 0.0) {
        return 0.0;
      }
      return std::pow(x, 4) * std::log(x + std::sqrt((x * x) + 1));
    };
    std::println("Trapazoidal: {}", 1 + testConvergence(TrapazoidalIntegrator{func, 0.0, 2.0}));
    std::println("Simpson: {}", 2 + testConvergence(SimpsonIntegrator{func, 0.0, 2.0}));
    std::println("Romberg: {}", 5 + testConvergence(RombergIntegrator{5, func, 0.0, 2.0}));
    std::println("Midpoint: {}", 1 + testConvergence(MidpointIntegrator{func, 0.0, 2.0}));
    std::println("RombergMidpoint: {}", 5 + testConvergence(RombergMidpointIntegrator{3, func, 0.0, 2.0}));
    std::println("TanhRule: {}", 1 + testConvergence(TanhRuleIntegrator{func, 0.0, 2.0}));
    std::println("TanhSinhRule: {}", 1 + testConvergence(TanhSinhRuleIntegrator{func, 0.0, 2.0}));
  };
  return TestRegistry::result();
}
