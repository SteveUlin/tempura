#pragma once

#include "matrix/matrix.h"

namespace tempura::matrix {

template <typename Scalar, size_t extent>
class Identity final : public Matrix<{extent, extent}> {
public:
  using ScalarT = Scalar;

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

  auto getImpl(size_t row, size_t col) const -> ScalarT {
    return (row == col) ? ScalarT{1} : ScalarT{0};
  }

  RowCol shape_;
};

}  // namespace tempura::matrix
