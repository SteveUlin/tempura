#pragma once

#include <cassert>

#include "matrix3/matrix.h"

namespace tempura::matrix3 {

// Check that two matrices have the same shape
template <typename Lhs, typename Rhs>
constexpr void checkSameExtent(const Lhs& lhs, const Rhs& rhs) {
  static_assert(Lhs::ExtentsType::rank() == Rhs::ExtentsType::rank(),
                "Matrices must have the same rank");
  for (std::size_t i = 0; i < Lhs::ExtentsType::rank(); ++i) {
    assert(lhs.extent().extent(i) == rhs.extent().extent(i));
  }
}

// In-place addition: left += right
template <typename Lhs, typename Rhs>
  requires(Lhs::ExtentsType::rank() == 2 && Rhs::ExtentsType::rank() == 2)
constexpr auto operator+=(Lhs& left, const Rhs& right) -> Lhs& {
  checkSameExtent(left, right);
  for (std::size_t i = 0; i < left.extent().extent(0); ++i) {
    for (std::size_t j = 0; j < left.extent().extent(1); ++j) {
      left[i, j] += right[i, j];
    }
  }
  return left;
}

// In-place subtraction: left -= right
template <typename Lhs, typename Rhs>
  requires(Lhs::ExtentsType::rank() == 2 && Rhs::ExtentsType::rank() == 2)
constexpr auto operator-=(Lhs& left, const Rhs& right) -> Lhs& {
  checkSameExtent(left, right);
  for (std::size_t i = 0; i < left.extent().extent(0); ++i) {
    for (std::size_t j = 0; j < left.extent().extent(1); ++j) {
      left[i, j] -= right[i, j];
    }
  }
  return left;
}

// Explicit output type addition: result = lhs + rhs
template <typename Out, typename Lhs, typename Rhs>
  requires(Out::ExtentsType::rank() == 2 && Lhs::ExtentsType::rank() == 2 &&
           Rhs::ExtentsType::rank() == 2)
constexpr auto add(const Lhs& lhs, const Rhs& rhs) -> Out {
  checkSameExtent(lhs, rhs);
  Out result;
  for (std::size_t i = 0; i < lhs.extent().extent(0); ++i) {
    for (std::size_t j = 0; j < lhs.extent().extent(1); ++j) {
      result[i, j] = lhs[i, j] + rhs[i, j];
    }
  }
  return result;
}

// Explicit output type subtraction: result = lhs - rhs
template <typename Out, typename Lhs, typename Rhs>
  requires(Out::ExtentsType::rank() == 2 && Lhs::ExtentsType::rank() == 2 &&
           Rhs::ExtentsType::rank() == 2)
constexpr auto subtract(const Lhs& lhs, const Rhs& rhs) -> Out {
  checkSameExtent(lhs, rhs);
  Out result;
  for (std::size_t i = 0; i < lhs.extent().extent(0); ++i) {
    for (std::size_t j = 0; j < lhs.extent().extent(1); ++j) {
      result[i, j] = lhs[i, j] - rhs[i, j];
    }
  }
  return result;
}

// Auto-inference addition: result = left + right
// Returns InlineDense with promoted scalar type
template <typename Lhs, typename Rhs>
  requires(Lhs::ExtentsType::rank() == 2 && Rhs::ExtentsType::rank() == 2)
constexpr auto operator+(const Lhs& left, const Rhs& right) {
  // Type promotion via decltype of element addition
  using ScalarT = decltype(left[0, 0] + right[0, 0]);
  // Extract static extents (assuming both matrices have the same static shape)
  constexpr auto kRow = Lhs::ExtentsType::staticExtent(0);
  constexpr auto kCol = Lhs::ExtentsType::staticExtent(1);
  return add<InlineDense<ScalarT, kRow, kCol>>(left, right);
}

// Auto-inference subtraction: result = left - right
// Returns InlineDense with promoted scalar type
template <typename Lhs, typename Rhs>
  requires(Lhs::ExtentsType::rank() == 2 && Rhs::ExtentsType::rank() == 2)
constexpr auto operator-(const Lhs& left, const Rhs& right) {
  // Type promotion via decltype of element subtraction
  using ScalarT = decltype(left[0, 0] - right[0, 0]);
  // Extract static extents (assuming both matrices have the same static shape)
  constexpr auto kRow = Lhs::ExtentsType::staticExtent(0);
  constexpr auto kCol = Lhs::ExtentsType::staticExtent(1);
  return subtract<InlineDense<ScalarT, kRow, kCol>>(left, right);
}

}  // namespace tempura::matrix3
