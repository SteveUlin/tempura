#pragma once

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <limits>

#include "quadature/gaussian.h"
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

// Regularized lower incomplete gamma function: P(a, x) = γ(a, x) / Γ(a)
//
// Intuition:
//   P(a, x) is the CDF of the Gamma(a, 1) distribution evaluated at x.
//   Behaves like a sigmoid: transitions from 0 to 1 near x ≈ a-1 with
//   width proportional to √a.
//
// Properties:
//   P(a, 0) = 0, P(a, ∞) = 1
//   Integral form: P(a, x) = (1/Γ(a)) ∫₀ˣ t^(a-1) e^(-t) dt

namespace detail {

// Series expansion: γ(a, x) = e^(-x) x^a ∑_{n=0}^∞ x^n / Γ(a + n + 1)
//
// Converges rapidly for x < a + 1 (below the transition point)
template <typename T>
constexpr auto incompleteGammaSeries(T a, T x) -> T {
  using std::abs;
  using std::exp;
  using std::lgamma;
  using std::log;

  T ap = a;
  T sum = T{1} / a;
  T term = sum;

  // Iterate until convergence
  while (true) {
    ap += T{1};
    term *= x / ap;
    sum += term;

    // Converged when term is negligible relative to sum
    constexpr T ϵ = T{1e-10};
    if (abs(term) < abs(sum) * ϵ) {
      break;
    }
  }

  return exp(-x + a * log(x) - lgamma(a)) * sum;
}

// Gauss-Legendre quadrature for incomplete gamma
//
// Used for large a (≥ 100) where series/continued fraction converge slowly
// Integrates the gamma integrand directly using 18-point quadrature
constexpr auto incompleteGammaGaussianQuadature(const double a, const double x)
    -> double {
  using std::exp;
  using std::log;
  using std::max;
  using std::min;
  using std::sqrt;

  const double a1 = a - 1.0;
  const double sqrta1 = sqrt(a1);
  const double lna1 = log(a1);
  const double gln = logGamma(a);

  // Choose integration endpoint based on position relative to transition
  double xu;
  if (x > a1) {
    // Above transition: integrate from x to far right
    xu = max(a1 + 11.5 * sqrta1, x + 6.0 * sqrta1);
  } else {
    // Below transition: integrate from far left to x
    xu = max(0.0, min(a1 - 7.5 * sqrta1, x - 5.0 * sqrta1));
  }

  constexpr static auto weights = [] consteval {
    constexpr int64_t N = 18;
    auto weights = quadature::gaussLegendre(0.0, 1.0, N);
    std::array<quadature::GaussianWeight<>, N> w;
    std::ranges::copy(weights, w.begin());
    return w;
  }();

  double sum = 0.0;
  for (const auto& [t, w] : weights) {
    const double z = x + (xu - x) * t;
    sum += w * exp(-(z - a1) + a1 * (log(z) - lna1));
  }

  double ans = sum * (xu - x) * exp(a1 * (lna1 - 1.0) - gln);
  return x > a1 ? 1.0 - ans : -ans;
}

// Continued fraction: Q(a, x) = 1 - P(a, x)
//
// Uses Lentz's algorithm to evaluate the continued fraction expansion
// Converges rapidly for x > a + 1 (above the transition point)
template <typename T>
constexpr auto incompleteGammaContinuedFraction(T a, T x) -> T {
  using std::exp;
  using std::lgamma;
  using std::log;

  constexpr T ϵ = T{1e-10};
  constexpr T tiny = T{1e-30};

  T b = x + T{1} - a;
  T c = T{1} / tiny;
  T d = T{1} / b;
  T h = d;

  // Modified Lentz's algorithm for continued fraction evaluation
  for (int i = 1; i < 100; ++i) {
    T an = -static_cast<T>(i) * (static_cast<T>(i) - a);
    b += T{2};

    d = an * d + b;
    if (d == T{0}) {
      d = tiny;
    }

    c = b + an / c;
    if (c == T{0}) {
      c = tiny;
    }

    d = T{1} / d;
    T del = d * c;
    h *= del;

    // Converged when del ≈ 1
    T diff = del - T{1};
    T abs_diff = diff < T{0} ? -diff : diff;
    if (abs_diff < ϵ) {
      break;
    }
  }

  // Return complement: P(a, x) = 1 - Q(a, x)
  return T{1} - exp(-x + a * log(x) - lgamma(a)) * h;
}

}  // namespace detail

// Generic incomplete gamma for arbitrary floating-point types
template <typename T>
constexpr auto incompleteGamma(T a, T x) -> T {
  assert(a > T{0} && x >= T{0});

  if (x == T{0}) {
    return T{0};
  }

  // Choose algorithm based on position relative to transition point
  if (x < a + T{1}) {
    // Series converges rapidly below transition
    return detail::incompleteGammaSeries(a, x);
  }

  // Continued fraction converges rapidly above transition
  return detail::incompleteGammaContinuedFraction(a, x);
}

// Specialized overload for double supporting large a via quadrature
constexpr auto incompleteGamma(double a, double x) -> double {
  assert(a > 0.0 && x >= 0.0);

  if (x == 0.0) {
    return 0.0;
  }

  // For very large a, quadrature is more stable than series/fraction
  if (a >= 100.0) {
    return detail::incompleteGammaGaussianQuadature(a, x);
  }

  if (x < a + 1.0) {
    return detail::incompleteGammaSeries(a, x);
  }

  return detail::incompleteGammaContinuedFraction(a, x);
}

}  // namespace tempura::special
