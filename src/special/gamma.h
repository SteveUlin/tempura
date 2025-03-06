#include <array>
#include <cassert>
#include <cmath>
#include <limits>

#include "sequence.h"

namespace tempura::special {

// Lanczos approximation to the gamma function
//
// Copied from Numerical Recipes 3rd Edition 6.1

constexpr auto logGamma(const double x) -> double {
  assert(x > 0.0);
  static constexpr std::array coefficients = {
      57.1562356658629235,     -59.5979603554754912,    14.1360979747417471,
      -0.491913816097620199,   .339946499848118887e-4,  .465236289270485756e-4,
      -.983744753048795646e-4, .158088703224912494e-3,  -.210264441724104883e-3,
      .217439618115212643e-3,  -.164318106536763890e-3, .844182239838527433e-4,
      -.261908384015814087e-4, .368991826595316234e-5};
  double tmp = x + 5.2421875;
  tmp = (x + 0.5) * std::log(tmp) - tmp;
  double ser = 0.999999999999997092;
  for (size_t j = 0; j < coefficients.size(); ++j) {
    ser += coefficients[j] / (x + static_cast<double>(j + 1));
  }
  return tmp + std::log(2.5066282746310005 * ser / x);
}

constexpr auto gamma(const double x) -> double { return std::exp(logGamma(x)); }

constexpr auto factorial(const int64_t n) -> double {
  // Factorial of numbers larger than 170 overflow in double precision.
  // Factorials up to 22! are exact in double precision.
  static constexpr std::array mem = [] consteval {
    std::array<double, 171> arr = {};
    arr[0] = 1.;
    for (size_t i = 1; i < arr.size(); ++i) {
      arr[i] = static_cast<double>(i) * arr[i - 1];
    }
    return arr;
  }();
  assert(0 <= n && n <= 170);
  return mem[n];
}

constexpr auto logFactorial(const int64_t n) -> double {
  static constexpr std::array<double, 2000> mem = [] {
    std::array<double, 2000> arr = {};
    for (size_t i = 0; i < arr.size(); ++i) {
      arr[i] = logGamma(static_cast<double>(i + 1));
    }
    return arr;
  }();
  if (n < 2000) {
    return mem[n];
  }
  return logGamma(static_cast<double>(n + 1));
}

constexpr auto binomialCoefficient(const int64_t n, const int64_t k) -> double {
  assert(0 <= k && k <= n);
  if (n <= 170) {
    return std::floor(0.5 + (factorial(n) / (factorial(k) * factorial(n - k))));
  }
  return std::floor(
      0.5 + std::exp(logFactorial(n) - logFactorial(k) - logFactorial(n - k)));
}

constexpr auto beta(const double x, const double y) -> double {
  return std::exp(logGamma(x) + logGamma(y) - logGamma(x + y));
}

// Incomplete Gamma function
// P(a, x) = γ(a, x) / Γ(a)
//         =  1 ⌠∞
//          ----⎮ t^(a-1) e^(-t) dt
//          Γ(a)⌡x
// P(x, 0) = 0 and P(x, ∞) = 1
//
// The Incomplete Gamma fn is kinda like a sigmoid function:
//  - transition centers ~ a - 1
//  - the transition width is ~ sqrt(a)

namespace detail {

// The Incomplete Gamma function as a series expansion:
// Γ(a, x) = e⁻ˣxᵃ ∑ Γ(a) xⁿ / Γ(a + 1 + n)
constexpr auto incompleteGammaSeries(const double a, const double x)
    -> double {
  double ap = a;
  double sum = 1 / a;
  double term = sum;
  while (true) {
    ap += 1.0;
    term *= x / ap;
    sum += term;
    constexpr double ϵ = std::numeric_limits<double>::epsilon();
    if (std::abs(term) < std::abs(sum) * ϵ) {
      break;
    }
  }
  return std::exp(-x + (a * std::log(x)) - logGamma(a)) * sum;
}

// The Incomplete Gamma function as a continued fraction
// Γ(a, x) = e⁻ˣxᵃ / (x + 1 - a - (1 ⋅ (1 - a) / (x + 3 - a - (2 ⋅ (2 - a) / (x + 5 - a - (3 ⋅ (3 - a) / ...))
constexpr auto incompleteGammaContinuedFaction(const double a, const double x)
-> double {
  constexpr double ϵ = std::numeric_limits<double>::epsilon();
  auto value = FnGenerator{[a, x, n = 0]() -> std::pair<double, double> {
    if (n == 0) {
      return {1.0, x + 1.0 - a};
    }
    return {- n * (1 - a), x + ((2 * n + 1)) - a};
  }} | continuants() | Converges{.epsilon = ϵ};
  return std::exp(-x + (a * std::log(x)) - logGamma(a)) / value;
}

}  // namespace detail

}  // namespace tempura::special
