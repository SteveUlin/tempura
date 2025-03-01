#pragma once

#include <cmath>
#include <cstdint>
#include <vector>

namespace tempura::quadature {

// Gaussian Quadrature
//
// Instead of spacing out sample points evenly, Gaussian quadrature
// uses the sample locations to approximate the integral with even higher order
// polynomials than could normally be achieved with the same number of sample
// points.
//
// Gaussian quadrature requires that your integrand function has the form:
// g(x) = W(x) f(x)
//
// Where W(x) is a weight function that could be used to define an inner
// product over the space of functions. The sample points and weight depend
// of the choice of W(x). However, you don't need to explicitly factor W(x)
// out of your integrand function. You just need to select one that is
// mostly appropriate for your problem.
//
// If f(x) is a polynomial of degree up to 2n-1, then Gaussian quadrature
// is exact.
//
// Gaussian quadrature constructs a set of abscissas and weights and
// approximates the integral as:
//
// ∫ f(x) dx ≈ ∑ w_i f(x_i)

// Abscissas and weights for Gaussian quadrature

template <typename T = double>
struct GaussianWeight {
  T abscissa = 0.0;
  T weight = 0.0;
};

// Gauss-Legendre Quadrature
// ⌠a
// ⎮ f(x) dx ≈ ∑ w_i f(x_i)
// ⌡b
template <typename T = double>
constexpr auto gaussLegendre(const T a, const T b, const int64_t N,
                             const T ϵ = 1e-14)
    -> std::vector<GaussianWeight<T>> {
  std::vector<GaussianWeight<T>> weights(N);

  // Solutions are symmetric around the midpoint so we only need to find half
  using std::abs;
  T prev;
  T deriv;
  for (int64_t i = 0; i < (N + 1) / 2; ++i) {
    using std::cos;
    constexpr auto π = std::numbers::pi_v<T>;
    auto z = cos(π * (static_cast<T>(i) + 0.75) / (static_cast<T>(N) + 0.5));
    do {
      T p0 = 0.0;
      T p1 = 1.0;
      // Recurrence relation:
      // (j + 1) * Pⱼ₊₁  = (2j + 1) * z * Pⱼ - j * Pⱼ₋₁
      for (int64_t j = 0; j < N; ++j) {
        std::tie(p0, p1) =
            std::pair{p1, ((2.0 * static_cast<T>(j) + 1.0) * z * p1 -
                           static_cast<T>(j) * p0) /
                              (static_cast<T>(j) + 1.0)};
      }
      // Derivative of Legendre polynomial
      deriv = N * (z * p1 - p0) / (z * z - 1.0);
      // Newton's method
      prev = z;
      z -= p1 / deriv;
    } while (abs(z - prev) > ϵ);

    const T xm = 0.5 * (b + a);
    const T xl = 0.5 * (b - a);
    // Special case weight calculation 2 / (1 - z^2) * (Pₙ'(z))^2
    weights[i] = {.abscissa = xm - (xl * z),
                  .weight = 2.0 * xl / ((1.0 - z * z) * deriv * deriv)};
    weights[N - i - 1] = {.abscissa = xm + (xl * z),
                          .weight = weights[i].weight};
  }

  return weights;
}

}  // namespace tempura::quadature
