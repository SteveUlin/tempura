#pragma once

#include <algorithm>
#include <numeric>
#include <vector>

#include "matrix/matrix.h"

namespace tempura::matrix {

template <int64_t extent>
class RowPermutation final : public Matrix<{extent, extent}> {
 public:
  using ScalarT = bool;
  RowPermutation()
    requires(extent != kDynamic)
  {
    data_.resize(extent);
    iota(data_.begin(), data_.end(), 0);
  }

  RowPermutation(int64_t n)
    requires(extent == kDynamic)
  {
    data_.resize(n);
    iota(data_.begin(), data_.end(), 0);
  }

  RowPermutation(std::initializer_list<int64_t> perm) {
    if constexpr (extent != kDynamic) {
      CHECK(perm.size() == extent);
    }
    data_.assign(perm.begin(), perm.end());
    auto sorted = data_;
    std::ranges::sort(sorted);
    for (int64_t i = 0; i < data_.size(); ++i) {
      CHECK(sorted[i] == i);
    }
  }

  void swap(int64_t i, int64_t j) {
    CHECK(i < data_.size());
    CHECK(j < data_.size());
    std::swap(data_[i], data_[j]);
  }

  auto indexAt(int64_t i) const -> int64_t {
    return data_[i];
  }

  auto permute(auto&& matrix) -> decltype(auto) {
    auto visited = std::vector<bool>(data_.size(), false);
    for (int64_t i = 0; i < data_.size(); ++i) {
      if (visited[i]) {
        continue;
      }
      int64_t j = data_[i];
      while (j != i) {
        for (int64_t k = 0; k < matrix.shape().col; ++k) {
          std::swap(matrix[i, k], matrix[j, k]);
        }
        visited[j] = true;
        j = data_[j];
      }
    }
    return matrix;
  }

 private:
  friend class Matrix<{extent, extent}>;

  auto getImpl(int64_t row, int64_t col) const {
    return static_cast<int>(data_[row] == col);
  }

  auto shapeImpl() const -> RowCol {
    return {static_cast<int64_t>(data_.size()),
            static_cast<int64_t>(data_.size())};
  }

  std::vector<int64_t> data_;
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

  auto swap(int64_t i, int64_t j) {
    permutation_.swap(i, j);
  }

  auto permutation() const -> const RowPermutation<MatT::kExtent.row>& {
    return permutation_;
  }

private:
  friend Matrix<MatT::kExtent>;

  auto getImpl(this auto&& self, int64_t row, int64_t col) -> decltype(auto) {
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

