#pragma once

#include "matrix/matrix.h"

namespace tempura::matrix {

template <int64_t extent>
class Identity final : public Matrix<{extent, extent}> {
public:
  using ScalarT = bool;

  Identity() {
    const int64_t n = (extent == kDynamic) ? 0 : extent;
    shape_ = {n, n};
  }

  Identity(int64_t N) requires (extent == kDynamic) {
    shape_ = {N, N};
  }

private:
  friend Matrix<{extent, extent}>;

  auto shapeImpl() const -> RowCol { return shape_; }

  auto getImpl(int64_t row, int64_t col) const -> bool {
    return row == col;
  }

  RowCol shape_;
};

}  // namespace tempura::matrix
