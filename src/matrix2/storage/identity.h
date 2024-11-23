#pragma once

#include <cassert>
#include <type_traits>

#include "matrix2/matrix.h"

namespace tempura::matrix {

// Identity is a matrix where the diagonal is 1 and the rest is 0
//
// ⎡ 1 0 0 0 ⎤
// ⎢ 0 1 0 0 ⎥
// ⎢ 0 0 1 0 ⎥
// ⎣ 0 0 0 1 ⎦

template <Extent N>
class Identity;

template <Extent N>
  requires(N > 0 and N != kDynamic)
class Identity<N> {
 public:
  using ValueType = bool;
  static constexpr Extent kRow = N;
  static constexpr Extent kCol = N;

  constexpr Identity() = default;

  constexpr auto operator[](int64_t row, int64_t col) const -> bool {
    if (std::is_constant_evaluated()) {
      CHECK(0 <= row and row < kRow);
      CHECK(0 <= col and col < kCol);
    }
    return row == col;
  }

  constexpr auto shape() const -> RowCol { return {.row = N, .col = N}; }

  constexpr auto operator==(Identity<N> /*unused*/) const -> bool {
    return true;
  }
};
static_assert(MatrixT<Identity<4>>);

template <>
class Identity<kDynamic> {
 public:
  using ValueType = bool;
  static constexpr Extent kRow = kDynamic;
  static constexpr Extent kCol = kDynamic;

  constexpr Identity(int64_t n) : n_{n} { CHECK(n >= 0); }

  constexpr auto operator[](int64_t row, int64_t col) const -> bool {
    if (std::is_constant_evaluated()) {
      CHECK(0 <= row and row < n_);
      CHECK(0 <= col and col < n_);
    }
    return row == col;
  }

  constexpr auto shape() const -> RowCol { return {.row = n_, .col = n_}; }

  constexpr auto operator==(Identity other) const -> bool {
    CHECK(n_ == other.n_);
    return true;
  }

 private:
  int64_t n_;
};
static_assert(MatrixT<Identity<kDynamic>>);

template <int64_t N>
  requires(N != kDynamic)
constexpr auto operator==(Identity<kDynamic> lhs, Identity<N> /*unused*/)
    -> bool {
  CHECK(lhs.shape().row == N);
  return true;
}

template <int64_t N>
  requires(N != kDynamic)
constexpr auto operator==(Identity<N> /*unused*/, Identity<kDynamic> rhs)
    -> bool {
  CHECK(rhs.shape().row == N);
  return true;
}

Identity(int64_t) -> Identity<kDynamic>;

}  // namespace tempura::matrix
