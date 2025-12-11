#pragma once

#include <cassert>
#include <complex>
#include <cstdint>

namespace tempura::matrix3 {

// Complex is a thin wrapper around std::complex providing the 2x2
// matrix representation of a complex number.
//   ⎡ real -imag ⎤
//   ⎣ imag  real ⎦

template <typename T = double>
class Complex {
 public:
  using ValueType = T;
  static constexpr int64_t kRows = 2;
  static constexpr int64_t kCols = 2;

  constexpr Complex() : value_{1, 0} {}

  constexpr Complex(T real, T imag) : value_{real, imag} {}

  constexpr explicit Complex(const std::complex<T>& value) : value_{value} {}

  // Read access using C++23 "deducing this" with variadic operator[]
  // Returns by value since this is a mathematical view, not storage
  template <typename... Indices>
    requires(sizeof...(Indices) == 2)
  constexpr auto operator[](this auto const& self, Indices... indices)
      -> ValueType {
    auto [row, col] = std::tuple{indices...};
    if (std::is_constant_evaluated()) {
      assert(0 <= row && row < kRows && "row index out of bounds");
      assert(0 <= col && col < kCols && "col index out of bounds");
    }
    if (row == 0 && col == 0) {
      return self.value_.real();
    }
    if (row == 1 && col == 0) {
      return self.value_.imag();
    }
    if (row == 0 && col == 1) {
      return -self.value_.imag();
    }
    // row == 1 && col == 1
    return self.value_.real();
  }

  // Extent accessors for compatibility
  static constexpr auto rows() -> int64_t { return kRows; }
  static constexpr auto cols() -> int64_t { return kCols; }

  constexpr auto data() const -> const std::complex<T>& { return value_; }

  constexpr auto operator==(const Complex& other) const -> bool {
    return value_ == other.value_;
  }

 private:
  std::complex<T> value_;
};

}  // namespace tempura::matrix3
