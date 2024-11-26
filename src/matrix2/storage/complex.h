#pragma once

#include <complex>

#include "matrix2/matrix.h"

namespace tempura::matrix {

// Complex is a thin wrapper around std::complex providing the 2x2
// matrix representation of a complex number.
//   ⎡ real -imag ⎤
//   ⎣ imag  real ⎦

template <typename T = double>
class Complex {
 public:
  using ValueType = T;
  static constexpr int64_t kRow = 2;
  static constexpr int64_t kCol = 2;

  constexpr Complex() : value_{1, 0} {}

  constexpr Complex(T real, T imag) : value_{real, imag} {}

  constexpr explicit Complex(const std::complex<T>& value) : value_{value} {}

  constexpr auto operator[](int64_t row, int64_t col) const -> ValueType {
    if (std::is_constant_evaluated()) {
      CHECK(0 <= row and row < kRow);
      CHECK(0 <= col and col < kCol);
    }
    if (row == 0 && col == 0) {
      return value_.real();
    }
    if (row == 1 && col == 0) {
      return value_.imag();
    }
    if (row == 0 && col == 1) {
      return -value_.imag();
    }
    // row == 1 && col == 1
    return value_.real();
  }

  constexpr auto shape() const -> RowCol { return {.row = kRow, .col = kCol}; }

  constexpr auto data() const -> const std::complex<T>& { return value_; }

  constexpr auto operator==(const Complex& other) const -> bool {
    return value_ == other.value_;
  }

 private:
  std::complex<T> value_;
};

}  // namespace tempura::matrix
