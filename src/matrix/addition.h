#pragma once

#include "matrix/matrix.h"

namespace tempura::matrix {

template <MatrixT Lhs, MatrixT Rhs>
  requires (matchExtent(Lhs::kExtent, Rhs::kExtent))
auto operator+=(Lhs& left, const Rhs& right) -> Lhs& {
  CHECK(left.shape() == right.shape());
  for (size_t i = 0; i < left.shape().row; ++i) {
    for (size_t j = 0; j < right.shape().col; ++j) {
      left[i, j] += right[i, j];
    }
  }
  return left;
}

template <MatrixT Lhs, MatrixT Rhs>
  requires (matchExtent(Lhs::kExtent, Rhs::kExtent))
auto operator+(Lhs left, const Rhs& right) -> Lhs {
  CHECK(left.shape() == right.shape());
  left += right;
  return left;
}

template <MatrixT Lhs, MatrixT Rhs>
  requires (matchExtent(Lhs::kExtent, Rhs::kExtent))
auto operator-=(Lhs& left, const Rhs& right) -> Lhs& {
  CHECK(left.shape() == right.shape());
  for (size_t i = 0; i < left.shape().row; ++i) {
    for (size_t j = 0; j < right.shape().col; ++j) {
      left[i, j] -= right[i, j];
    }
  }
  return left;
}

template <MatrixT Lhs, MatrixT Rhs>
  requires (matchExtent(Lhs::kExtent, Rhs::kExtent))
auto operator-(Lhs left, const Rhs& right) -> Lhs {
  CHECK(left.shape() == right.shape());
  left -= right;
  return left;
}

}  // namespace tempura::matrix
