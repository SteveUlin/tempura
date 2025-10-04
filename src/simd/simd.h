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

// A 512-bit vector of 8 64-bit doubles.
//
// This class overloads the arithmetic operators to allow for SIMD operations
// on the vector. It uses AVX-512 intrinsics to perform the operations.
class Vec8d {
 public:
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

class Vec8i64 {
 public:
  Vec8i64() = default;

  explicit Vec8i64(__m512i data) : data_(data) {}


  explicit Vec8i64(uint64_t val)
      : data_(_mm512_set1_epi64(static_cast<int64_t>(val))) {}

  // Precondition:
  //   - data cannot be null
  //   - data must point to an array of at least 8 int64_t
  //   - data must be aligned to 64 bytes
  explicit Vec8i64(const int64_t* data) { data_ = _mm512_loadu_si512(data); }

  Vec8i64(int64_t v0, int64_t v1, int64_t v2, int64_t v3, int64_t v4,
           int64_t v5, int64_t v6, int64_t v7)
      : data_(_mm512_set_epi64(v7, v6, v5, v4, v3, v2, v1, v0)) {}

  constexpr static auto size() -> std::size_t {
    return 8;  // Number of elements in the vector
  }

  // Precondition:
  //   - index must be in the range [0, 7]
  auto operator[](std::size_t index) const -> int64_t {
    std::array<int64_t, 8> arr;
    _mm512_storeu_si512(arr.data(), data_);
    return arr[index];
  }

  auto operator==(const Vec8i64 other) const -> bool {
    // Compare all elements for equality
    __mmask8 mask = _mm512_cmpeq_epi64_mask(data_, other.data_);
    return mask == 0xFF;  // All bits set means all elements are equal
  }

  auto operator!=(const Vec8i64 other) const -> bool {
    return !(*this == other);
  }

  auto operator+(const Vec8i64 other) const -> Vec8i64 {
    return Vec8i64{_mm512_add_epi64(data_, other.data_)};
  }

  auto operator+=(const Vec8i64 other) -> Vec8i64& {
    data_ = _mm512_add_epi64(data_, other.data_);
    return *this;
  }

  auto operator-(const Vec8i64 other) const -> Vec8i64 {
    return Vec8i64{_mm512_sub_epi64(data_, other.data_)};
  }

  auto operator-=(const Vec8i64 other) -> Vec8i64& {
    data_ = _mm512_sub_epi64(data_, other.data_);
    return *this;
  }

  auto operator*(const Vec8i64 other) const -> Vec8i64 {
    return Vec8i64{_mm512_mullo_epi64(data_, other.data_)};
  }

  auto operator*=(const Vec8i64 other) -> Vec8i64& {
    data_ = _mm512_mullo_epi64(data_, other.data_);
    return *this;
  }

  auto operator&(const Vec8i64 other) const -> Vec8i64 {
    return Vec8i64{_mm512_and_si512(data_, other.data_)};
  }

  auto operator&=(const Vec8i64 other) -> Vec8i64& {
    data_ = _mm512_and_si512(data_, other.data_);
    return *this;
  }

  auto operator^(const Vec8i64 other) const -> Vec8i64 {
    return Vec8i64{_mm512_xor_si512(data_, other.data_)};
  }

  auto operator^=(const Vec8i64 other) -> Vec8i64& {
    data_ = _mm512_xor_si512(data_, other.data_);
    return *this;
  }

  auto operator>>(const Vec8i64 other) const -> Vec8i64 {
    return Vec8i64{_mm512_srlv_epi64(data_, other.data_)};
  }

  auto operator>>=(const Vec8i64 other) -> Vec8i64& {
    data_ = _mm512_srlv_epi64(data_, other.data_);
    return *this;
  }

  auto operator<<(const Vec8i64 other) const -> Vec8i64 {
    return Vec8i64{_mm512_sllv_epi64(data_, other.data_)};
  }

  auto operator<<=(const Vec8i64 other) -> Vec8i64& {
    data_ = _mm512_sllv_epi64(data_, other.data_);
    return *this;
  }

 private:
  __m512i data_;
};

} // namespace tempura
