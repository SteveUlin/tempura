#pragma once

#include "matrix/matrix.h"

namespace tempura::matrix {

namespace internal {
constexpr auto kroneckerExtent(const RowCol& lhs, const RowCol& rhs) -> RowCol {
  RowCol shape;
  if (lhs.row == kDynamic || rhs.row == kDynamic) {
    shape.row = kDynamic;
  } else {
    shape.row = lhs.row * rhs.row;
  }

  if (lhs.col == kDynamic || rhs.col == kDynamic) {
    shape.col = kDynamic;
  } else {
    shape.col = lhs.col * rhs.col;
  }
  return shape;
}
}  // namespace internal

template <MatrixT Lhs, MatrixT Rhs>
class KroneckerProduct
    : public Matrix<internal::kroneckerExtent(Lhs::kExtent, Rhs::kExtent)> {
 public:
  using ScalarT = decltype(std::declval<typename Lhs::ScalarT>() *
                           std::declval<typename Rhs::ScalarT>());
  KroneckerProduct(const Lhs& lhs, const Rhs& rhs)
      : lhs_(lhs),
        rhs_(rhs),
        shape_(internal::kroneckerExtent(lhs_.shape(), rhs_.shape())) {
    CHECK(verifyShape(*this));
  }

 private:
  friend Matrix<internal::kroneckerExtent(Lhs::kExtent, Rhs::kExtent)>;
  auto getImpl(size_t row, size_t col) const {
    const auto lhs_row = row / rhs_.shape().row;
    const auto rhs_row = row % rhs_.shape().row;
    const auto lhs_col = col / rhs_.shape().col;
    const auto rhs_col = col % rhs_.shape().col;
    return lhs_[lhs_row, lhs_col] * rhs_[rhs_row, rhs_col];
  }

  auto shapeImpl() const -> RowCol { return shape_; }

  const Lhs& lhs_;
  const Rhs& rhs_;
  const RowCol shape_;
};

}  // namespace tempura::matrix
