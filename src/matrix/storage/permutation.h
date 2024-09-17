#pragma once

#include <algorithm>
#include <numeric>
#include <vector>

#include "matrix/matrix.h"

namespace tempura::matrix {

// Permutation represents a particular ordering of a "set of elements".
template <int64_t extent = kDynamic>
class Permutation final {
 public:
  Permutation()
    requires(extent != kDynamic)
  {
    // Vector's C++23 range constructor is not implemented yet. So manually
    // create and fill the data_ vector.
    //
    // TODO: update to data_(std::from_range_t, iota_view(extent));
    data_.resize(extent);
    iota(data_.begin(), data_.end(), 0);
  }

  Permutation(int64_t n)
    requires(extent == kDynamic)
  {
    data_.resize(n);
    iota(data_.begin(), data_.end(), 0);
  }

  Permutation(std::initializer_list<int64_t> perm) : data_(perm) {
    if constexpr (extent != kDynamic) {
      CHECK(data_.size() == extent);
    }
    std::vector<int64_t> sorted = data_;
    std::ranges::sort(sorted);
    for (int64_t i = 0; i < data_.size(); ++i) {
      CHECK(sorted[i] == i);
    }
  }

  Permutation(std::vector<int64_t> perm) : data_(std::move(perm)) {
    if constexpr (extent != kDynamic) {
      CHECK(data_.size() == extent);
    }
    std::vector<int64_t> sorted = data_;
    std::ranges::sort(sorted);
    for (int64_t i = 0; i < data_.size(); ++i) {
      CHECK(sorted[i] == i);
    }
  }

  // Get the index of the element at position i in the permutation.
  auto operator[](int64_t i) const -> int64_t {
    CHECK(i < data_.size());
    return data_[i];
  }

  auto swap(int64_t i, int64_t j) -> void {
    CHECK(i < data_.size());
    CHECK(j < data_.size());
    std::swap(data_[i], data_[j]);
  }

  auto size() const -> int64_t { return data_.size(); }

  // Gets the next permutation in lexicographic order.
  auto operator++(int) -> Permutation& {
    std::next_permutation(data_.begin(), data_.end());
    return *this;
  }

  // Gets the previous permutation in lexicographic order.
  auto operator--(int) -> Permutation& {
    std::prev_permutation(data_.begin(), data_.end());
    return *this;
  }

  template <int64_t other_extent>
  auto permute(Permutation<other_extent>& other) const -> void {
    permute(other.data_);
  }

  // Permute swaps the elements of some indexable object to match the
  // permutation.
  auto permute(auto&& vec) const -> void;

 private:
  std::vector<int64_t> data_;
};

template <int64_t extent>
auto Permutation<extent>::permute(auto&& vec) const -> void {
  CHECK(vec.size() == data_.size());
  // Permutations form cycles of swapped elements. Here we push one element
  // around the cycle until we reach the end of that loop. Then we move onto
  // the next cycle.
  auto visited = std::vector<bool>(data_.size(), false);
  for (int64_t i = 0; i < data_.size(); ++i) {
    if (visited[i]) {
      continue;
    }
    int64_t j = data_[i];
    while (j != i) {
      using std::swap;
      swap(vec[j], vec[data_[j]]);
      visited[j] = true;
      j = data_[j];
    }
  }
}

// RowPermutation represents a matrix that swaps rows when applied from the
// left hand side. That is, if you want to swap the rows of M with the
// RowPermutation P, you would do P * M.
template <int64_t extent>
class RowPermutation final : public Matrix<{extent, extent}> {
 public:
  RowPermutation()
    requires(extent != kDynamic)
      : permutation_() {}

  RowPermutation(int64_t n)
    requires(extent == kDynamic)
      : permutation_(n) {}

  template <int64_t other_extent>
    requires(other_extent == extent || other_extent == kDynamic ||
             extent == kDynamic)
  RowPermutation(Permutation<other_extent> perm)
      : permutation_(std::move(perm)) {
    CHECK(this->shape().row == permutation_.size());
  }

  void swap(int64_t i, int64_t j) { permutation_.swap(i, j); }

  auto permute(auto&& matrix) -> decltype(auto) {
    permutation_.permute(matrix.rows());
    return matrix;
  }

 private:
  friend class Matrix<{extent, extent}>;

  auto getImpl(int64_t row, int64_t col) const {
    return static_cast<int>(permutation_[row] == col);
  }

  auto shapeImpl() const -> RowCol {
    return {static_cast<int64_t>(permutation_.size()),
            static_cast<int64_t>(permutation_.size())};
  }

  Permutation<extent> permutation_;
};
template <int64_t other_extent>
RowPermutation(Permutation<other_extent> perm) -> RowPermutation<other_extent>;

// RowPermuted is a wrapper around a matrix that lets you "swap rows" without
// moving the data in memory. It is a "view" into the original matrix.
template <typename M>
  requires MatrixT<std::remove_cvref_t<M>>
class RowPermuted final : public Matrix<std::remove_cvref_t<M>::kExtent> {
 public:
  using MatT = std::remove_cvref_t<M>;
  RowPermuted(M&& matrix)
    requires(MatT::kExtent.row != kDynamic)
      : matrix_(std::forward<M>(matrix)) {}

  RowPermuted(M&& matrix)
    requires(MatT::kExtent.row == kDynamic)
      : matrix_(std::forward<M>(matrix)), permutation_(matrix.shape().row) {}

  auto swap(int64_t i, int64_t j) { permutation_.swap(i, j); }

  auto permutation() const -> const Permutation<MatT::kExtent.row>& {
    return permutation_;
  }

 private:
  friend Matrix<MatT::kExtent>;

  auto getImpl(this auto&& self, int64_t row, int64_t col) -> decltype(auto) {
    return self.matrix_[self.permutation_[row], col];
  }

  auto shapeImpl() const -> RowCol { return matrix_.shape(); }

  M matrix_;
  Permutation<MatT::kExtent.row> permutation_;
};

template <typename M>
  requires MatrixT<std::remove_cvref_t<M>>
RowPermuted(M&&) -> RowPermuted<M>;

}  // namespace tempura::matrix
