#pragma once

#include "matrix2/matrix.h"
#include "matrix2/dense.h"

namespace tempura::matrix {

template <MatrixT Lhs, MatrixT Rhs>
constexpr auto operator+=(Lhs& left, const Rhs& right) -> Lhs& {
  checkSameShape(left, right);
  for (int64_t i = 0; i < left.shape().row; ++i) {
    for (int64_t j = 0; j < right.shape().col; ++j) {
      left[i, j] += right[i, j];
    }
  }
  return left;
}

template <MatrixT Out, MatrixT Lhs, MatrixT Rhs>
constexpr auto add(const Lhs& lhs, const Rhs& rhs) -> Out {
  checkSameShape(lhs, rhs);
  Out result;
  for (int64_t i = 0; i < lhs.shape().row; ++i) {
    for (int64_t j = 0; j < rhs.shape().col; ++j) {
      result[i, j] = lhs[i, j] + rhs[i, j];
    }
  }
  return result;
}

template <MatrixT Lhs, MatrixT Rhs>
constexpr auto operator+(const Lhs& left, const Rhs& right) {
  // In general we cannot assume that left is mutable, so we take the worst
  // case scenario and copy it into a Dense matrix.
  using ScalarT = decltype(left[0, 0] + right[0, 0]);
  return add<Dense<ScalarT, Lhs::kRow, Lhs::kCol>>(left, right);
}

template <MatrixT Lhs, MatrixT Rhs>
constexpr auto operator-=(Lhs& left, const Rhs& right) -> Lhs& {
  checkSameShape(left, right);
  for (int64_t i = 0; i < left.shape().row; ++i) {
    for (int64_t j = 0; j < right.shape().col; ++j) {
      left[i, j] -= right[i, j];
    }
  }
  return left;
}

template <MatrixT Out, MatrixT Lhs, MatrixT Rhs>
constexpr auto subtract(const Lhs& lhs, const Rhs& rhs) -> Out {
  checkSameShape(lhs, rhs);
  Out result;
  for (int64_t i = 0; i < lhs.shape().row; ++i) {
    for (int64_t j = 0; j < rhs.shape().col; ++j) {
      result[i, j] = lhs[i, j] - rhs[i, j];
    }
  }
  return result;
}

template <MatrixT Lhs, MatrixT Rhs>
constexpr auto operator-(const Lhs& left, const Rhs& right) {
  // In general we cannot assume that left is mutable, so we take the worst
  // case scenario and copy it into a Dense matrix.
  using ScalarT = decltype(left[0, 0] - right[0, 0]);
  return subtract<Dense<ScalarT, Lhs::kRow, Lhs::kCol>>(left, right);
}

}  // namespace tempura::matrix
