
#pragma once

#include <algorithm>
#include <cassert>
#include <cmath>
#include <concepts>
#include <cstdint>
#include <functional>
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

template <typename T>
class Chebyshev {
 public:
  // Constructor that takes a function, interval [a, b], and number of points n.
  // This constructor evaluates the function at the Chebyshev nodes and computes
  // the coefficients for the Chebyshev series.
  constexpr Chebyshev(std::regular_invocable<T> auto&& func, T a, T b,
                      int64_t n = 50);

  // Constructor that takes precomputed coefficients and interval [a, b].
  constexpr Chebyshev(std::vector<T> coefficients, double a, double b)
      : a_{a},
        b_{b},
        n_{static_cast<int64_t>(std::ranges::size(coefficients))},
        m_{n_},
        coefficients_{std::move(coefficients)} {}

  auto a() const -> const T& { return a_; }

  auto b() const -> const T& { return b_; }

  // Set the threshold for the coefficients.
  //
  // This function scans from the highest degree down to the lowest degree
  // and reduces the degree of the polynomial to the highest degree m such that
  // the absolute value of the coefficient at m is greater than or equal to
  // threshold.
  constexpr auto setThreshold(T threshold) -> int64_t {
    assert(threshold > 0.0);
    using std::abs;
    while (m_ > 1 && abs(coefficients_[m_ - 1]) < threshold) --m_;
    return m_;
  }

  // Set the degree of the approximating polynomial to m. All Chebyshev
  // polynomials of higher degree will be ignored in the evaluation. This is
  // non-destructive and does not change the coefficients of the polynomial.
  //
  // Precondition:
  //   - m must be in the range [1, n_]
  constexpr void setDegree(int64_t m) {
    assert(m > 0);
    assert(m <= n_);
    m_ = m;
  }

  // Evaluate the Chebyshev polynomial at a given point x.
  //
  // This function uses the Clenshaw algorithm to efficiently evaluate the
  // polynomial.
  constexpr auto operator()(T x) const -> T;

  // Returns the coefficients of the Chebyshev Polynomial.
  constexpr auto coefficients() const -> const std::vector<T>& {
    return coefficients_;
  }

  // Returns the number of terms in the Chebyshev series.
  constexpr auto size() const -> int64_t { return n_; }

  // Returns the degree of the Chebyshev Polynomial.
  constexpr auto degree() const -> int64_t { return m_; }

 private:
  T a_;
  T b_;

  // The number of zeros of the Chebyshev Polynomial.
  int64_t n_;
  // The number of terms to use in the evaluation.
  int64_t m_;

  std::vector<T> coefficients_;
};

template <typename T>
constexpr Chebyshev<T>::Chebyshev(std::regular_invocable<T> auto&& func, T a,
                                  T b, int64_t n)
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

  // Compute the coefficients for the Chebyshev series.
  coefficients_.reserve(n_);
  const T fac = 2. / n_;
  for (int64_t i = 0; i < n_; ++i) {
    T sum = 0.0;
    for (int64_t j = 0; j < n_; ++j) {
      sum += f[j] * cos(π * (static_cast<T>(j) + 0.5) * static_cast<T>(i) /
                        static_cast<T>(n_));
    }
    coefficients_.push_back(fac * sum);
  }
}

template <typename T>
constexpr auto Chebyshev<T>::operator()(T x) const -> T {
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

// Returns the derivative of the Chebyshev Polynomial as a new Chebyshev
// object. This function computes the derivative coefficients using the
// recurrence relation for derivatives.
//
// Precondition:
//   input.size() >= 2
template <typename T>
auto differentiate(const Chebyshev<T>& input) -> Chebyshev<T> {
  const T a = input.a();
  const T b = input.b();
  const std::size_t n = input.size();
  const std::vector<T>& coefficients = input.coefficients();

  std::vector<T> deriv(input.size());
  // a'ᵢ₋₁ = 2i * aᵢ + a'ᵢ₊₁
  // a'ₙ = a'ₙ₋₁ = 0.0
  deriv[n - 1] = 0.0;
  deriv[n - 2] = 2. * (n - 1) * coefficients[n - 1];
  for (int64_t j = n - 2; j > 0; --j) {
    deriv[j - 1] = 2. * static_cast<T>(j) * coefficients[j] + deriv[j + 1];
  }

  const T con = 2.0 / (b - a);
  for (T& d : deriv) d *= con;
  return Chebyshev<T>(std::move(deriv), a, b);
}

// Returns the integral of the Chebyshev Polynomial as a new Chebyshev
// object such that evaluating the object at `a_` will return 0. This function
// computes the integral coefficients using the recurrence relation for
// integrals.
//
// Precondition:
//   - input.size() >= 2
template <typename T>
constexpr auto integrate(const Chebyshev<T>& input) -> Chebyshev<T> {
  const T a = input.a();
  const T b = input.b();
  const std::size_t n = input.size();
  const std::vector<T>& coefficients = input.coefficients();

  std::vector<T> integral(n);
  // Aᵢ = (aᵢ₋₁ - aᵢ₊₁) / 2i
  const T con = 0.25 * (b - a);
  T sum = 0.0;
  T fac = 1.0;
  for (std::size_t i = 1; i < n - 1; ++i) {
    integral[i] =
        con * (coefficients[i - 1] - coefficients[i + 1]) / static_cast<T>(i);
    sum += fac * integral[i];
    fac = -fac;
  }
  integral[n - 1] = con * coefficients[n - 2] / (n - 1);
  sum += fac * integral[n - 1];
  integral[0] = 2. * sum;
  return Chebyshev<T>(std::move(integral), a, b);
}

template <typename T>
auto eval(const Chebyshev<T>& chebyshev, const T& x) -> T {
  // Evaluate the Chebyshev Polynomial at x.
  return chebyshev(x);
}

// Converts a Chebyshev object (a polynomial w.r.t. Chebyshev Polynomials) to
// a polynomial p(x) = ∑ dₖ xᵏ = ∑ cₖ Tₖ(y) + cₒ / 2
template <typename T>
auto toPoly(const Chebyshev<T>& chebyshev) -> std::vector<T> {
  const auto& coeffs = chebyshev.coefficients();
  const std::int64_t m = chebyshev.degree();

  std::vector<T> d(m);
  std::vector<T> dd(m);
  d[0] = coeffs[m - 1];

  double sv;
  for (std::int64_t j = m - 2; j > 0; --j) {
    for (std::int64_t k = m - j; k > 0; --k) {
      sv = d[k];
      d[k] = 2. * d[k - 1] - dd[k];
      dd[k] = sv;
    }
    sv = d[0];
    d[0] = -dd[0] + coeffs[j];
    dd[0] = sv;
  }

  for (std::int64_t j = m-1; j > 0; --j) d[j] = d[j - 1] - dd[j];

  d[0] = -dd[0] + .5 * coeffs[0];

  // Rescale the coefficients to the original interval [a, b].
  T cnst = 2.0 / (chebyshev.b() - chebyshev.a());
  T fac = 1.0;
  for (int64_t i = 0; i < m; ++i) {
    d[i] *= fac;
    fac *= cnst;
  }
  cnst = 0.5 * (chebyshev.b() + chebyshev.a());
  for (std::int64_t j = 0; j < m - 2; ++j) {
    for (std::int64_t k = m - 2; k >= j; --k) {
      d[k] -= cnst * d[k + 1];
    }
  }

  return d;
}

}  // namespace tempura
