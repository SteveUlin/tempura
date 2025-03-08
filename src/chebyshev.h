#pragma once

#include <algorithm>
#include <cassert>
#include <cmath>
#include <concepts>
#include <cstdint>
#include <vector>

namespace tempura {

// Chebyshev Polynomials
//
// The Chevyshev Polynomial of degree n is defined as:
// Tₙ(x) = cos(n * arccos(x))
//
// You can use trigonometric identities to yield:
// T₀(x) = 1
// T₁(x) = x
// T₂(x) = 2x² - 1
// T₃(x) = 4x³ - 3x
// T₄(x) = 8x⁴ - 8x² + 1
// ...
// Tₙ₊₁(x) = 2xTₙ(x) - Tₙ₋₁(x) n ≥ 1
//
// Chebyshev Polynomials are orthogonal on the interval [-1, 1] with respect to
// 1 / sqrt(1 - x²).
//
// ⌠ 1                               ⎧ 0   n ≠ m
// ⎮  Tₙ(x)Tₘ(x) / sqrt(1 - x²) dx = ⎨ π/2 n = m ≠ 0
// ⌡-1                               ⎩ π   n = m = 0
//
// The polynomial Tₙ(x) has n zeros in the interval [-1, 1] and the zeros are
// at:
//
// x = cos((2k + 1)π / 2n) for k = 0, 1, ..., n - 1
//
// In the same interval [-1, 1] there are also n + 1 extrema and they are at:
//
// x = cos(kπ / n) for k = 0, 1, ..., n
//
// All of the maxima/minima are ±1.
//
// The Chebyshev Polynomials also have discrete orthogonally properties. That
// is if xₖ (k = 0 ... m-1) are the zeros of Tₘ(x) then:
//
//  m - 1            ⎧ 0   n ≠ m
//  ∑ Tₙ(xₖ)Tₘ(xₖ) = ⎨ m/2 n = m ≠ 0
//  k = 0            ⎩ m   n = m = 0
//
// We can use these orthogonality properties to approximate functions!
//
// f(x) ≈ [Σ aₙTₙ(x)] - a₀ / 2
//
// Where the coefficients aₙ are given by:
//
// aₙ = 2 / N ∑ f(xₖ)Tₙ(xₖ)
//
// Here this approximation is exact for the N zeros of Tₙ(x).
//
// This approximation isn't necessarily more accurate than any other nth
// degree polynomial approximation. However, take N to be large. If we truncate
// the Chebyshev approximation at degree m << N, then the maximum error is
// bounded by the dropped aₙ's. As the aₙ tend to decrease rapidly with n, we
// can easily bound the maximum possible error.

template <typename T = double>
class Chebyshev {
 public:
  constexpr Chebyshev(std::regular_invocable<T> auto func, double a, double b,
                      int64_t n = 50)
      : a_{a}, b_{b}, n_{n}, m_{n} {
    using std::cos;
    constexpr auto π = std::numbers::pi;
    const T bma = 0.5 * (b - a);
    const T bpa = 0.5 * (b + a);

    // Memoize function evaluations at the zeros of the Chebyshev Polynomial.
    std::vector<T> f;
    f.reserve(n_);
    for (int64_t i = 0; i < n_; ++i) {
      T y = cos(π * (static_cast<T>(i) + 0.5) / n_);
      f.push_back(func((bma * y) + bpa));
    }

    coefficients_.reserve(n_);
    const T fac = 2. / n_;
    for (int64_t i = 0; i < n_; ++i) {
      T sum = 0.0;
      for (int64_t j = 0; j < n_; ++j) {
        sum +=
            f[j] * cos(π * (static_cast<T>(j) + 0.5) * static_cast<T>(i) / n_);
      }
      coefficients_.push_back(fac * sum);
    }
  }

  // Construct with coefficients directly
  constexpr Chebyshev(std::ranges::sized_range auto&& coefficients, double a,
                      double b)
      : a_{a}, b_{b}, n_{std::ranges::size(coefficients)}, m_{n_} {
    coefficients_.reserve(n_);
    std::ranges::copy(coefficients, std::back_inserter(coefficients_));
  }

  auto setThreshold(T threshold) -> int64_t {
    assert(threshold > 0.0);
    using std::abs;
    while (m_ > 1 && abs(coefficients_[m_ - 1]) < threshold) --m_;
    return m_;
  }

  void setDegree(int64_t m) {
    assert(m > 0);
    assert(m <= n_);
    m_ = m;
  }

  auto operator()(T x) const -> T {
    using std::cos;
    T y = (2. * x - a_ - b_) / (b_ - a_);
    T y2 = 2. * y;
    T curr = 0.0;
    T prev = 0.0;
    // Recurrence relation for Chebyshev Polynomials.
    for (int64_t i = m_ - 1; i > 0; --i) {
      std::tie(curr, prev) =
          std::make_pair((y2 * curr) - prev + coefficients_[i], curr);
    }
    // Slightly different scaling for the final term
    return (y * curr) - prev + (0.5 * coefficients_[0]);
  }

  auto coefficients() const -> const std::vector<T>& { return coefficients_; }

  auto size() const -> int64_t { return n_; }

  auto degree() const -> int64_t { return m_; }

 private:
  T a_;
  T b_;

  // The number of zeros of the Chebyshev Polynomial.
  int64_t n_;
  // The number of terms to use in the evaluation.
  int64_t m_;

  std::vector<T> coefficients_;
};

}  // namespace tempura
