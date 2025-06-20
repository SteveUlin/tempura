#include <bit>
#include <cmath>
#include <cstdint>
#include <print>

#include "chebyshev.h"
#include "meta/vector_to_array.h"
#include "plot.h"

namespace tempura {

// Fast Exponential Computation on SIMD Architectures
// https://wapco.e-ce.uth.gr/2015/papers/SESSION3/WAPCO_3_5.pdf

// Floating point numbers are represented as:
//   (-1)ˢ(1 + m) * 2ˣ⁻ˣ⁰
//
// For doubles:
// - [ 1 bits] s is the sign bit
// - [52 bits] m is the mantissa (the fraction part)
// - [11 bits] x is the exponent
// - x0 is the bias == 1023
//
// eˣ = 2^{x log₂(e)} = 2^{xi + xf}
// where:
//   - xi is some integer
//   - xf ∈ [0, 1)
//
// Core Idea:
//   - Calcuint64_t int64_t late 2^{xi} by calculating the exponent section
//     of the output float directly
//   - Approximate 2^{xf} using (1 + m) section
//
// Derivation:
//
// Define K to be some correction fn such that
//   eˣ = 2^{xi} * (1 + m - K(xf))
//
// Therefore
//   K = 1 + m - 2^{xf}
//
// For the input x, m is defined to be xf ∈ [0, 1)
//   K = 1 + xf - 2^{xf}
//
// Model K(xf) = a xfⁿ + b xfⁿ⁺¹ + c xfⁿ⁺² + ...
// with n ∈ [1, 10]
//
// int64_t i = A(x - ln(2) * K(xf)) + B
//
// Where:
//  - A = S / ln(2)
//  - B = S * 1023
//  - S = 2^{52}
//
// reinterpret i as a double to get the final result
//
// Algo 1:
// x = x * log2(e)
// auto xf = x - floor(x)
// auto x = x - K(xf)
auto exp(double x) -> double {
  constexpr auto coeffs = vectorToArray([] {
    Chebyshev chebyshev(
        [](double xf) {
          return (std::exp2(xf) - 1.0);
        },
        0.0, 1.0, 10);
    return toPolynomial(chebyshev);
  });
  x = std::numbers::log2e * x;  // Convert to base 2 exponent
  const double xi = std::floor(x);
  const double xf = x - xi;

  double k = coeffs[coeffs.size() - 1];
  for (std::int64_t i = coeffs.size() - 2; i >= 0; --i) {
    k = std::fma(k, xf, coeffs[i]);
  }
  k += 1.0;
  auto e = std::bit_cast<uint64_t>(k);
  e += static_cast<uint64_t>(xi) << 52;
  return std::bit_cast<double>(e);
}

}  // namespace tempura
