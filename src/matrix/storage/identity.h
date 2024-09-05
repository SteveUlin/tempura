#pragma once

#include "matrix/matrix.h"

namespace tempura::matrix {

template <size_t extent>
class Identity final : public Matrix<{extent, extent}> {
public:
  using ScalarT = bool;

  Identity() {
    const size_t n = (extent == kDynamic) ? 0 : extent;
    shape_ = {n, n};
  }

  Identity(size_t N) requires (extent == kDynamic) {
    shape_ = {N, N};
  }

private:
  friend Matrix<{extent, extent}>;

  auto shapeImpl() const -> RowCol { return shape_; }

  auto getImpl(size_t row, size_t col) const -> bool {
    return row == col;
  }

  RowCol shape_;
};

}  // namespace tempura::matrix
