#pragma once

#include <cmath>

namespace tempura::special {

/**
 * @brief Computes the value of the Rosenbrock function.
 *
 * f(x, y) = (a - x)² + b * (y - x²)²
 *
 * The Rosenbrock function is a non-convex function used as a performance test
 * problem for optimization algorithms. It is also known as the "Banana
 * function" due to the shape of its contours.
 *
 * The function reaches a minimum of 0 at (a, a²). With the default settings
 * this is 1 at (1, 1).
 *
 * @param x The x-coordinate input to the function.
 * @param y The y-coordinate input to the function.
 * @param options Optional parameters for the function, encapsulated in a
 * `BanannaOptions` struct. Default values are `a = 1` and `b = 100`.
 * @return The computed value of the Rosenbrock function.
 */
template <typename T = double, typename U = double>
struct BanannaOptions {
  T a = 1.;
  U b = 100.;
};

template <typename T = double, typename U = double,
          typename Options = BanannaOptions<double, double>>
auto rosnbrockFn(T x, T y, Options options = {}) {
  auto& [a, b] = options;
  using std::pow;
  return pow(a - x, 2) + b * pow(y - x * x, 2);
}

}  // namespace tempura::special
