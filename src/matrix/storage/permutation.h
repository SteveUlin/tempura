#pragma once

#include <algorithm>
#include <numeric>
#include <ranges>
#include <vector>

#include "matrix/matrix.h"

namespace tempura::matrix {

template <size_t extent>
class RowPermutation final : public Matrix<{extent, extent}> {
 public:
  using ScalarT = bool;
  RowPermutation()
    requires(extent != kDynamic)
  {
    data_.resize(extent);
    iota(data_.begin(), data_.end(), 0);
  }

  RowPermutation(size_t n)
    requires(extent == kDynamic)
  {
    data_.resize(n);
    iota(data_.begin(), data_.end(), 0);
  }

  RowPermutation(std::initializer_list<size_t> perm) {
    if constexpr (extent != kDynamic) {
      CHECK(perm.size() == extent);
    }
    data_.assign(perm.begin(), perm.end());
    auto sorted = data_;
    std::ranges::sort(sorted);
    for (size_t i = 0; i < data_.size(); ++i) {
      CHECK(sorted[i] == i);
    }
  }

  void swap(size_t i, size_t j) {
    CHECK(i < data_.size());
    CHECK(j < data_.size());
    std::swap(data_[i], data_[j]);
  }

  auto indexAt(size_t i) const -> size_t {
    return data_[i];
  }

 private:
  friend class Matrix<{extent, extent}>;

  auto getImpl(size_t row, size_t col) const {
    return static_cast<int>(data_[row] == col);
  }

  auto shapeImpl() const -> RowCol { return {data_.size(), data_.size()}; }

  std::vector<size_t> data_;
};

template <typename M>
requires MatrixT<std::remove_cvref_t<M>>
class RowPermuted final : public Matrix<std::remove_cvref_t<M>::kExtent> {
public:
  using MatT = std::remove_cvref_t<M>;
  RowPermuted(M&& matrix) requires (MatT::kExtent.row != kDynamic)
    : matrix_(std::forward<M>(matrix)) {}

  RowPermuted(M&& matrix) requires (MatT::kExtent.row == kDynamic)
    : matrix_(std::forward<M>(matrix)), permutation_(matrix.shape().row) {}

  auto swap(size_t i, size_t j) {
    permutation_.swap(i, j);
  }

private:
  friend Matrix<MatT::kExtent>;

  auto getImpl(this auto&& self, size_t row, size_t col) -> decltype(auto) {
    return self.matrix_[self.permutation_.indexAt(row), col];
  }

  auto shapeImpl() const -> RowCol { return matrix_.shape(); }

  M matrix_;
  RowPermutation<MatT::kExtent.row> permutation_;
};

template <typename M>
requires MatrixT<std::remove_cvref_t<M>>
RowPermuted(M&&) -> RowPermuted<M>;

}  // namespace tempura::matrix

