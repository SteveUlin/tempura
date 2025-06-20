#include "math/cos.h"

#include <random>

#include "benchmark.h"
#include "unit.h"

using namespace tempura;

int64_t ulp_distance(double a, double b) {
  // Ensure we handle cases where a and b have different signs correctly.
  // If signs are different, the ULP distance can be very large,
  // but the simple integer difference is still a meaningful metric.
  if (std::signbit(a) != std::signbit(b)) {
    // For a simple test, you might return a max value or handle as a special
    // case. However, for small numbers around zero, this will still work.
  }

  int64_t a_int = std::bit_cast<int64_t>(a);
  int64_t b_int = std::bit_cast<int64_t>(b);

  // The integer representation of negative doubles decreases as their magnitude
  // increases. The following logic correctly handles positive and negative
  // numbers.
  if (a_int < 0) a_int = INT64_MIN - a_int;
  if (b_int < 0) b_int = INT64_MIN - b_int;

  return std::abs(a_int - b_int);
}

auto main() -> int {
  constexpr double π = std::numbers::pi;

  "cos function"_test = [] {
    expectNear(std::cos(0.0), tempura::cos(0.0));
    expectNear(std::cos(0.5), tempura::cos(0.5));
    expectNear(std::cos(π / 2.), tempura::cos(π / 2.));
    expectNear(std::cos(3 * π / 2.), tempura::cos(3 * π / 2.));

    expectNear(std::cos(1.0), tempura::cos(1.0));
    expectNear(std::cos(1.5), tempura::cos(1.5));
    expectNear(std::cos(2.0), tempura::cos(2.0));
    expectNear(std::cos(3.0), tempura::cos(3.0));

    expectNear(std::cos(100.0), tempura::cos(100.0));
    expectNear(std::cos(-100.0), tempura::cos(-100.0));
  };

  "fuzz"_test = [] {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<double> dist(-100.0, 100.0);

    for (int i = 0; i < 10'000'000; ++i) {
      double x = dist(gen);
      expectNear<1e-8>(std::cos(x), tempura::cos(x));
    }
  };

  volatile double out;
  // Ops per sec: 150'913'782
  "std cos benchmark"_bench.ops(10'000) = [&out] {
    double x = 0.5;
    for (int i = 0; i < 10'000; ++i) {
      out = std::cos(x);
      x += 0.01;
    }
  };

  // Ops per sec: 455'021'158
  "tempura cos benchmark"_bench.ops(10'000) = [&out] {
    double x = 0.5;
    for (int i = 0; i < 10'000; ++i) {
      out = tempura::cos(x);
      x += 0.01;
    }
  };

  // A vector of test values around zero
  std::vector<double> test_values = {
      0.0,  1e-15, -1e-15, 1e-9, -1e-9, 1e-6,  -1e-6,  0.01, -0.01,
      0.1,  -0.1,  0.5,    -0.5, 1.0,   -1.0,  1.5,    -1.5, 2.0,
      -2.0, 3.0,   -3.0,   4.0,  -4.0,  100.0, -100.0,
  };

  std::cout << std::fixed << std::setprecision(18);
  std::cout << "Testing cos() approximation ULP error around zero..."
            << std::endl;
  std::cout << "---------------------------------------------------------------"
               "-----------"
            << std::endl;
  std::cout << std::left << std::setw(22) << "Input Value (x)" << std::setw(25)
            << "my_tempura::cos(x)" << std::setw(25) << "std::cos(x)"
            << "ULP Error" << std::endl;
  std::cout << "---------------------------------------------------------------"
               "-----------"
            << std::endl;

  for (double x : test_values) {
    double my_result = tempura::cos(x);
    double std_result = std::cos(x);
    int64_t ulp_err = ulp_distance(my_result, std_result);

    std::cout << std::left << std::setw(22) << x << std::setw(25) << my_result
              << std::setw(25) << std_result << ulp_err << std::endl;
  }
  std::cout << "---------------------------------------------------------------"
               "-----------"
            << std::endl;

  return TestRegistry::result();
}

