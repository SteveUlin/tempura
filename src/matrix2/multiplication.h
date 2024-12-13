#pragma once

#include "matrix2/matrix.h"
#include "matrix2/storage/inline_dense.h"

namespace tempura::matrix {

template <int64_t block_size = 16, MatrixT Lhs, MatrixT Rhs>
constexpr auto tileMultiply(const Lhs& left, const Rhs& right)
  requires(Lhs::kCol == Rhs::kRow) {
  using ScalarT = decltype(left[0, 0] * right[0, 0]);
  InlineDense<ScalarT, Lhs::kRow, Rhs::kCol> out;

  for (int64_t jblock = 0; jblock < right.shape().col; jblock += block_size) {
    for (int64_t i = 0; i < left.shape().row; ++i) {
      for (int64_t kblock = 0; kblock < left.shape().col; kblock += block_size) {
        for (int64_t j = jblock;
             j < std::min(jblock + block_size, right.shape().col); ++j) {
          for (int64_t k = kblock;
               k < std::min(kblock + block_size, left.shape().col); ++k) {
            out[i, j] += left[i, k] * right[k, j];
          }
        }
      }
    }
  }
  return out;
}

template <MatrixT Lhs, MatrixT Rhs>
constexpr auto operator*(const Lhs& left, const Rhs& right) {
  return tileMultiply(left, right);
}

}  // namespace tempura::matrix
