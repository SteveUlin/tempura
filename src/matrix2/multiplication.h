#pragma once

#include "matrix2/matrix.h"
#include "matrix2/dense.h"

namespace tempura::matrix {

template <int64_t block_size = 16, MatrixT Lhs, MatrixT Rhs>
constexpr auto tileMultiply(const Lhs& left, const Rhs& right)
  requires(Lhs::kExtent.col == Rhs::kExtent.row) {
  using ScalarT = decltype(left[0, 0] * right[0, 0]);
  Dense<ScalarT, Lhs::kExtent.row, Rhs::kExtent.col> out;

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

constexpr auto operator*(const MatrixT auto& left, const MatrixT auto& right) {
  return tileMultiply(left, right);
}

};
