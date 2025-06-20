#pragma once

#include <immintrin.h>

#include <array>
#include <cmath>
#include <cstdint>
#include <print>
#include <stdexcept>

#include "chebyshev.h"
#include "meta/vector_to_array.h"

namespace tempura {

// tempura math ops


// Factorial of numbers larger than 170 overflow in double precision.
// Factorials up to 22! are exact in double precision.
//
// Precondition:
//   - n must be in the range [0, 170]
constexpr auto factorial(const int64_t n) -> double {
  static constexpr std::array mem = [] consteval {
    std::array<double, 171> arr = {};
    arr[0] = 1.;
    for (size_t i = 1; i < arr.size(); ++i) {
      arr[i] = static_cast<double>(i) * arr[i - 1];
    }
    return arr;
  }();
  return mem[n];
}

// A 512-bit vector of 8 64-bit doubles.
//
// This class overloads the arithmetic operators to allow for SIMD operations
// on the vector. It uses AVX-512 intrinsics to perform the operations.
class Vec8d {
 public:
  Vec8d() = default;

  explicit Vec8d(__m512d data) : data_(data) {}

  // Set all elements to the same value
  explicit Vec8d(double val) : data_(_mm512_set1_pd(val)) {}

  // Precondition:
  //   - data cannot be null
  //   - data must point to an array of at least 8 doubles
  //   - data must be aligned to 64 bytes
  explicit Vec8d(const double* data) { data_ = _mm512_loadu_pd(data); }

  Vec8d(double v0, double v1, double v2, double v3, double v4, double v5,
        double v6, double v7)
      : data_(_mm512_set_pd(v7, v6, v5, v4, v3, v2, v1, v0)) {}

  // Precondition:
  //   - data cannot be null
  //   - data must point to an array of at least 8 doubles
  //   - data must be aligned to 64 bytes
  void store(double* data) const { _mm512_storeu_pd(data, data_); }

  // Precondition:
  //   - index must be in the range [0, 7]
  auto operator[](std::size_t index) const -> double {
    return reinterpret_cast<const double*>(&data_)[index];
  }

  auto operator==(const Vec8d other) const -> bool {
    // Compare all elements for equality
    __mmask8 mask = _mm512_cmp_pd_mask(data_, other.data_, _CMP_EQ_OQ);
    return mask == 0xFF;  // All bits set means all elements are equal
  }

  auto operator!=(const Vec8d other) const -> bool { return !(*this == other); }

  auto operator+(const Vec8d other) const -> Vec8d {
    return Vec8d{_mm512_add_pd(data_, other.data_)};
  }

  auto operator+=(const Vec8d other) -> Vec8d& {
    data_ = _mm512_add_pd(data_, other.data_);
    return *this;
  }

  auto operator-(const Vec8d other) const -> Vec8d {
    return Vec8d{_mm512_sub_pd(data_, other.data_)};
  }

  auto operator-=(const Vec8d other) -> Vec8d& {
    data_ = _mm512_sub_pd(data_, other.data_);
    return *this;
  }

  auto operator*(const Vec8d other) const -> Vec8d {
    return Vec8d{_mm512_mul_pd(data_, other.data_)};
  }

  auto operator*=(const Vec8d other) -> Vec8d& {
    data_ = _mm512_mul_pd(data_, other.data_);
    return *this;
  }

  auto operator/(const Vec8d other) const -> Vec8d {
    return Vec8d{_mm512_div_pd(data_, other.data_)};
  }

  auto operator/=(const Vec8d other) -> Vec8d& {
    data_ = _mm512_div_pd(data_, other.data_);
    return *this;
  }

 private:
  friend auto fma(Vec8d a, Vec8d b, Vec8d c) -> Vec8d {
    return Vec8d{_mm512_fmadd_pd(a.data_, b.data_, c.data_)};
  }

  friend auto round(Vec8d v) -> Vec8d {
    return Vec8d{_mm512_roundscale_pd(v.data_, _MM_FROUND_TO_NEAREST_INT)};
  }

  __m512d data_;
};

template <typename T>
auto sinImpl(T x) -> T {
  constexpr double π = std::numbers::pi;
  // Reduce the range to [-π, π]
  T q = x * T{1.0 / (2.0 * π)};
  q = round(q);
  const T x_reduced = fma(q, T{-2.0 * π}, x);
  const T x2 = x_reduced * x_reduced;

  constexpr auto coeff = vectorToArray([] {
    // Chebyshev coefficients for sin(x) with the nearby zeros factored out
    Chebyshev chebyshev(
        [](double x) { return std::sin(x) / (x * (x - π) * (x + π)); }, -π, π,
        11);
    chebyshev.setThreshold(1e-10);
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

  auto poly = T{coeff[coeff.size() - 1]};
  for (std::int64_t i = coeff.size() - 2; i >= 0; i -= 1) {
    poly = fma(poly, x2, T{coeff[i]});
  }
  poly *= x_reduced * (x_reduced - T{π}) * (x_reduced + T{π});
  return poly;
}

}  // namespace tempura
