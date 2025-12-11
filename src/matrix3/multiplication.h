#pragma once

#include <algorithm>
#include <cassert>

#include "matrix3/matrix.h"

namespace tempura::matrix3 {

// Check that matrices can be multiplied: Lhs cols == Rhs rows
template <typename Lhs, typename Rhs>
constexpr void checkMultiplyExtent(const Lhs& lhs, const Rhs& rhs) {
  static_assert(Lhs::ExtentsType::rank() == 2 && Rhs::ExtentsType::rank() == 2,
                "Matrices must have rank 2");
  assert(lhs.extent().extent(1) == rhs.extent().extent(0));
}

// Block-based matrix multiplication with configurable block size
// Optimized for cache locality via tiling
template <int64_t block_size = 16, typename Lhs, typename Rhs>
  requires(Lhs::ExtentsType::rank() == 2 && Rhs::ExtentsType::rank() == 2)
constexpr auto tileMultiply(const Lhs& left, const Rhs& right) {
  checkMultiplyExtent(left, right);

  // Type promotion via decltype of element multiplication
  using ScalarT = decltype(left[0, 0] * right[0, 0]);

  // Extract static extents for output matrix
  constexpr auto kRow = Lhs::ExtentsType::staticExtent(0);
  constexpr auto kCol = Rhs::ExtentsType::staticExtent(1);

  InlineDense<ScalarT, kRow, kCol> out;

  // Five nested loops optimized for cache locality
  // Block over columns (j), then rows (i), then inner dimension (k)
  for (std::size_t jblock = 0; jblock < right.extent().extent(1);
       jblock += block_size) {
    for (std::size_t i = 0; i < left.extent().extent(0); ++i) {
      for (std::size_t kblock = 0; kblock < left.extent().extent(1);
           kblock += block_size) {
        for (std::size_t j = jblock;
             j < std::min(jblock + block_size, right.extent().extent(1)); ++j) {
          for (std::size_t k = kblock;
               k < std::min(kblock + block_size, left.extent().extent(1));
               ++k) {
            out[i, j] += left[i, k] * right[k, j];
          }
        }
      }
    }
  }
  return out;
}

// Convenient operator* for matrix multiplication
template <typename Lhs, typename Rhs>
  requires(Lhs::ExtentsType::rank() == 2 && Rhs::ExtentsType::rank() == 2)
constexpr auto operator*(const Lhs& left, const Rhs& right) {
  return tileMultiply(left, right);
}

}  // namespace tempura::matrix3
