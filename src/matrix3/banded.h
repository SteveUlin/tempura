#pragma once

#include <cassert>
#include <cstdint>
#include <type_traits>
#include <utility>

#include "matrix3/matrix.h"
#include "matrix3/matrix_traits.h"

namespace tempura::matrix3 {

// Banded is a wrapper that interprets a dense matrix's columns as "bands"
// around the diagonal of a square matrix.
//
// Storage layout (3 bands, center=1):
//   ⎡ X 1 2 ⎤
//   ⎢ 3 4 5 ⎥
//   ⎣ 6 7 X ⎦
//
// Represents this banded matrix:
//   ⎡ 1 2 0 ⎤
//   ⎢ 3 4 5 ⎥
//   ⎣ 0 6 7 ⎦
//
// Band calculation: band = col - row + kCenterBand
// Out-of-band elements return a reference to kZero member (reads as zero).
//
// DANGER: Writing to out-of-band elements is undefined behavior - they return
// a reference to the kZero member, so writes would corrupt it and affect all
// subsequent out-of-band reads. Always check bounds before writing, or use an
// accessor that throws on out-of-band access.
template <typename ChildType, int64_t CenterBand = MatrixTraits<ChildType>::kCols / 2>
  requires(CenterBand >= 0 && CenterBand < static_cast<int64_t>(MatrixTraits<ChildType>::kCols))
class Banded {
 public:
  using ValueType = typename MatrixTraits<ChildType>::ValueType;
  static constexpr int64_t kRows = MatrixTraits<ChildType>::kRows;
  static constexpr int64_t kCols = MatrixTraits<ChildType>::kRows;  // Square matrix
  static constexpr int64_t kBands = MatrixTraits<ChildType>::kCols;
  static constexpr int64_t kCenterBand = CenterBand;

  constexpr Banded() = default;
  constexpr explicit Banded(const ChildType& mat) : mat_{mat} {}
  constexpr explicit Banded(ChildType&& mat) : mat_{std::move(mat)} {}

  // Access element (i, j) of the banded matrix
  // Out-of-band elements return reference to kZero
  template <typename... Indices>
    requires(sizeof...(Indices) == 2)
  constexpr auto operator[](this auto&& self, Indices... indices)
      -> decltype(self.mat_[0, 0]) {
    auto [i, j] = std::tuple{indices...};
    if (std::is_constant_evaluated()) {
      assert(i >= 0 && i < kRows && "row index out of bounds");
      assert(j >= 0 && j < kCols && "col index out of bounds");
    }
    int64_t band = j - i + CenterBand;
    if (band < 0 || band >= kBands) {
      return self.kZero;
    }
    return self.mat_[i, band];
  }

  // Extent accessors for compatibility
  static constexpr auto rows() -> int64_t { return kRows; }
  static constexpr auto cols() -> int64_t { return kCols; }

  constexpr auto data() const -> const ChildType& { return mat_; }

 private:
  ValueType kZero{0};  // Out-of-band elements return reference to this
  ChildType mat_;
};

// Deduction guide for Banded with default center band
template <typename Scalar, std::size_t... Ns>
Banded(const InlineDense<Scalar, Ns...>&)
    -> Banded<InlineDense<Scalar, Ns...>, MatrixTraits<InlineDense<Scalar, Ns...>>::kCols / 2>;

// Factory function with explicit center band
template <int64_t CenterBand, typename M>
constexpr auto makeBanded(M&& mat)
    -> Banded<std::remove_cvref_t<M>, CenterBand> {
  return Banded<std::remove_cvref_t<M>, CenterBand>{std::forward<M>(mat)};
}

// Solve tridiagonal system Ax = b using Thomas algorithm
// A must be a Banded matrix with upper=lower=1 (3 bands, center=1)
// Returns solution vector x
//
// For system:
//   [b0 c0  0  0] [x0]   [d0]
//   [a1 b1 c1  0] [x1] = [d1]
//   [0  a2 b2 c2] [x2]   [d2]
//   [0  0  a3 b3] [x3]   [d3]
//
// Time complexity: O(n), space complexity: O(n)
//
// PRECONDITION: Matrix must be strictly diagonally dominant or symmetric
// positive definite to guarantee stability. No pivoting is performed.
template <typename ChildType, int64_t CenterBand, typename Vector>
constexpr auto solveTridiagonal(const Banded<ChildType, CenterBand>& A, const Vector& b)
    -> InlineDense<typename MatrixTraits<ChildType>::ValueType,
                   MatrixTraits<ChildType>::kRows, 1>
  requires(Banded<ChildType, CenterBand>::kBands == 3 && CenterBand == 1)
{
  using T = typename MatrixTraits<ChildType>::ValueType;
  constexpr int64_t n = MatrixTraits<ChildType>::kRows;

  static_assert(MatrixTraits<Vector>::kRows == n, "Vector size must match matrix size");
  static_assert(MatrixTraits<Vector>::kCols == 1, "b must be a column vector");

  InlineDense<T, n, 1> x{};

  if constexpr (n == 1) {
    // Trivial case: single equation
    x[0, 0] = b[0, 0] / A[0, 0];
    return x;
  }

  // Temporary arrays for modified coefficients
  InlineDense<T, n, 1> c_prime{};  // Modified upper diagonal
  InlineDense<T, n, 1> d_prime{};  // Modified RHS

  // Forward elimination: modify coefficients
  // First row
  c_prime[0, 0] = A[0, 1] / A[0, 0];  // c0 / b0
  d_prime[0, 0] = b[0, 0] / A[0, 0];  // d0 / b0

  for (int64_t i = 1; i < n; ++i) {
    T denom = A[i, i] - A[i, i - 1] * c_prime[i - 1, 0];  // bi - ai * c'[i-1]

    if (i < n - 1) {
      c_prime[i, 0] = A[i, i + 1] / denom;  // ci / denom
    }
    d_prime[i, 0] = (b[i, 0] - A[i, i - 1] * d_prime[i - 1, 0]) / denom;
  }

  // Back substitution
  x[n - 1, 0] = d_prime[n - 1, 0];
  for (int64_t i = n - 2; i >= 0; --i) {
    x[i, 0] = d_prime[i, 0] - c_prime[i, 0] * x[i + 1, 0];
  }

  return x;
}

// Optimized banded matrix-vector multiplication: y = A * x
// Only multiplies within the bandwidth, skipping zero elements
// Time complexity: O(n * bandwidth) instead of O(n²) for dense multiplication
template <typename ChildType, int64_t CenterBand, typename Vector>
constexpr auto multiplyBanded(const Banded<ChildType, CenterBand>& A, const Vector& x)
    -> InlineDense<typename MatrixTraits<ChildType>::ValueType,
                   MatrixTraits<ChildType>::kRows, 1> {
  using T = typename MatrixTraits<ChildType>::ValueType;
  constexpr int64_t n = MatrixTraits<ChildType>::kRows;
  constexpr int64_t bands = Banded<ChildType, CenterBand>::kBands;

  static_assert(MatrixTraits<Vector>::kRows == n, "Vector size must match matrix size");
  static_assert(MatrixTraits<Vector>::kCols == 1, "x must be a column vector");

  InlineDense<T, n, 1> y{};

  // For each row, only multiply columns within bandwidth
  for (int64_t i = 0; i < n; ++i) {
    T sum = 0;

    // Determine range of columns in this row that are within the band
    // band = j - i + CenterBand must be in [0, bands)
    // So: j must be in [i - CenterBand, i - CenterBand + bands)
    int64_t j_min = i - CenterBand;
    int64_t j_max = i - CenterBand + bands;

    // Clamp to valid column range [0, n)
    if (j_min < 0) j_min = 0;
    if (j_max > n) j_max = n;

    for (int64_t j = j_min; j < j_max; ++j) {
      sum += A[i, j] * x[j, 0];
    }

    y[i, 0] = sum;
  }

  return y;
}

}  // namespace tempura::matrix3
