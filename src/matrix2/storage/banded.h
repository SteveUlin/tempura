#pragma once

#include "matrix2/matrix.h"

namespace tempura::matrix {

// Banded is a matrix that treats the columns of a matrix as "bands" around
// the diagonal.
//
// ⎡ X 1 2 ⎤
// ⎢ 3 4 5 }⎥
// ⎣ 6 7 X ⎦
//
// The above matrix has 1 band above and 1 band below the diagonal. And
// represents:
//
// ⎡ 1 2 0 ⎤
// ⎢ 3 4 5 ⎥
// ⎣ 0 6 7 ⎦
//
// Assigning a value to an element that is not in one of the bands is undefined
// behavior.
template <MatrixT M, Extent CenterBand = M::kCol / 2>
  requires(CenterBand >= 0 and CenterBand < M::kCol)
class Banded {
 public:
  using ChildType = M;
  using ValueType = typename M::ValueType;
  static constexpr int64_t kRow = M::kRow;
  static constexpr int64_t kCol = M::kRow;
  static constexpr int64_t kBands = M::kCol;
  static constexpr int64_t kCenterBand = CenterBand;

  constexpr Banded() = default;
  constexpr explicit Banded(const M& mat) : mat_{mat} {}
  constexpr explicit Banded(M&& mat) : mat_{std::move(mat)} {}

  constexpr auto operator[](this auto&& self, int64_t i, int64_t j)
      -> decltype(self.mat_[0, 0]) {
    if (std::is_constant_evaluated()) {
      CHECK(i < kRow);
      CHECK(j < kCol);
    }
    int64_t band = j - i + CenterBand;
    if (band < 0 or band >= M::kCol) {
      return self.kZero;
    }
    return self.mat_[i, band];
  }

  constexpr auto shape() const -> RowCol { return {.row = kRow, .col = kCol}; }

  constexpr auto data() const -> const M& { return mat_; }

 private:
  ValueType kZero{0};
  M mat_;
};

// Deduction guide for Banded.
template <typename M, Extent CenterBand = M::kCol / 2>
Banded(const M&) -> Banded<std::remove_cvref_t<M>, CenterBand>;

template <Extent CenterBand>
auto makeBanded(auto&& mat) -> Banded<std::remove_cvref_t<decltype(mat)>, CenterBand> {
  return Banded<decltype(mat), CenterBand>{std::forward<decltype(mat)>(mat)};
}

}  // namespace tempura::matrix
