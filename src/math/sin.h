#include <array>
#include <cmath>
#include <cstdint>
#include <print>

#include "chebyshev.h"
#include "meta/vector_to_array.h"

namespace tempura {

template <typename T>
auto sin(T x) -> T {
  constexpr double π = std::numbers::pi;
  constexpr auto coeff = vectorToArray([] {
    // Chebyshev coefficients for sin(x) with the nearby zeros factored out
    Chebyshev chebyshev(
        [](double x) { return std::sin(x) / (x * (x - π) * (x + π)); }, -π, π);
    chebyshev.setDegree(20);
    // The approximation in terms of x instead of Chebyshev polynomials
    auto vec = toPolynomial(chebyshev);

    // Every second coefficient is really small, so we can drop them without
    // much loss of precision.
    std::vector<double> reduced;
    for (std::size_t i = 0; i < vec.size(); i += 2) {
      reduced.push_back(vec[i]);
    }
    return reduced;
  });

  using std::round;
  using std::fma;

  // Reduce the range to [-π, π]
  T q = x * T{1.0 / (2.0 * π)};
  q = round(q);
  const T x_reduced = fma(q, T{-2.0 * π}, x);
  const T x2 = x_reduced * x_reduced;

  auto poly = T{coeff[coeff.size() - 1]};
  for (std::int64_t i = coeff.size() - 2; i >= 0; i -= 1) {
    poly = fma(poly, x2, T{coeff[i]});
  }
  poly *= x_reduced * (x_reduced - T{π}) * (x_reduced + T{π});
  return poly;
}

}
