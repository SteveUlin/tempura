#pragma once

#include <numeric>
#include <type_traits>

#include "matrix2/matrix.h"
#include "matrix2/storage/permutation.h"

namespace tempura::matrix {

// RowPermuted is a wrapper around a matrix that lets you "swap rows" without
// moving the data in memory.
template <typename MatT>
class RowPermuted {
 public:
  using ValueType = typename std::remove_cvref_t<MatT>::ValueType;
  static constexpr int64_t kRow = std::remove_cvref_t<MatT>::kRow;
  static constexpr int64_t kCol = std::remove_cvref_t<MatT>::kCol;
  constexpr RowPermuted(MatT&& mat) : mat_(std::forward<MatT>(mat)) {}

  constexpr RowPermuted(MatT&& mat, Permutation<kRow> perm)
      : mat_(std::forward<MatT>(mat)), perm_(std::move(perm)) {}

  constexpr auto operator[](this auto&& self, int64_t i, int64_t j)
      -> decltype(auto) {
    return self.mat_[self.perm_.data()[i], j];
  }

  constexpr auto operator[](this auto&& self, int64_t i) -> decltype(auto)
    requires(MatT::kCol == 1)
  {
    return self.mat_[self.perm_.data()[i]];
  }

  constexpr auto shape() const -> RowCol { return mat_.shape(); }

  constexpr auto swap(int64_t i, int64_t j) {
    if (std::is_constant_evaluated()) {
      CHECK(i < mat_.shape().row);
      CHECK(j < mat_.shape().row);
    }
    perm_.swap(i, j);
  }

  constexpr auto permutation() const -> const Permutation<kRow>& {
    return perm_;
  }

 private:
  MatT mat_;
  Permutation<kRow> perm_ = {};
};

template <typename MatT>
RowPermuted(MatT&&) -> RowPermuted<MatT>;

template <typename MatT>
RowPermuted(MatT&&, Permutation<std::remove_cvref_t<MatT>::kRow>)
    -> RowPermuted<MatT>;

// ColPermuted is a wrapper around a matrix that lets you "swap columns"
// without moving the data in memory.
template <typename MatT>
class ColPermuted {
 public:
  using ValueType = typename std::remove_cvref_t<MatT>::ValueType;
  static constexpr int64_t kRow = std::remove_cvref_t<MatT>::kRow;
  static constexpr int64_t kCol = std::remove_cvref_t<MatT>::kCol;

  constexpr ColPermuted(MatT&& mat) : mat_(std::forward<MatT>(mat)) {}

  constexpr ColPermuted(MatT&& mat, Permutation<kCol> perm)
      : mat_(std::forward<MatT>(mat)), perm_(std::move(perm)) {}

  constexpr auto operator[](this auto&& self, int64_t i, int64_t j)
      -> decltype(auto) {
    return self.mat_[i, self.perm_.data()[j]];
  }

  constexpr auto shape() const -> RowCol { return mat_.shape(); }

  constexpr auto swap(int64_t i, int64_t j) {
    if (std::is_constant_evaluated()) {
      CHECK(i < mat_.shape().col);
      CHECK(j < mat_.shape().col);
    }
    perm_.swap(i, j);
  }

  constexpr auto permutation() const -> const Permutation<kCol>& {
    return perm_;
  }

 private:
  MatT mat_;
  Permutation<kCol> perm_ = {};
};

template <typename MatT>
ColPermuted(MatT&&) -> ColPermuted<MatT>;

template <typename MatT>
ColPermuted(MatT&&, Permutation<std::remove_cvref_t<MatT>::kCol>)
    -> ColPermuted<MatT>;

}  // namespace tempura::matrix
