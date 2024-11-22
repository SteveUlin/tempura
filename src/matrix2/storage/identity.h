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

template <int64_t N>
  requires(N > 0)
class Identity {
 public:
  using ValueType = bool;
  static constexpr int64_t kRow = N;
  static constexpr int64_t kCol = N;

  constexpr Identity() = default;

  constexpr auto operator[](int64_t row, int64_t col) const -> bool {
    return row == col;
  }

  constexpr auto shape() const -> RowCol { return {.row = N, .col = N}; }

  constexpr auto operator==(Identity<N> /*unused*/) const -> bool {
    return true;
  }
};
static_assert(MatrixT<Identity<4>>);

class DynamicIdentity {
 public:
  using ValueType = bool;
  static constexpr DynamicExtent kRow = kDynamic;
  static constexpr DynamicExtent kCol = kDynamic;

  constexpr DynamicIdentity(int64_t n) : n_{n} { CHECK(n >= 0); }

  constexpr auto operator[](int64_t row, int64_t col) const -> bool {
    return row == col;
  }

  constexpr auto shape() const -> RowCol { return {.row = n_, .col = n_}; }

  constexpr auto operator==(DynamicIdentity other) const -> bool {
    CHECK(n_ == other.n_);
    return true;
  }

 private:
  int64_t n_;
};

template <int64_t N>
constexpr auto operator==(DynamicIdentity lhs, Identity<N> /*unused*/) -> bool {
  CHECK(lhs.shape().row == N);
  return true;
}

template <int64_t N>
constexpr auto operator==(Identity<N> /*unused*/, DynamicIdentity rhs) -> bool {
  CHECK(rhs.shape().row == N);
  return true;
}

}  // namespace tempura::matrix
