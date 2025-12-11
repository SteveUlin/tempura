#pragma once

#include <cassert>
#include <cstdint>
#include <type_traits>
#include <utility>

#include "matrix3/permutation.h"

namespace tempura::matrix3 {

// RowPermuted is a wrapper around a matrix that lets you "swap rows" without
// moving the data in memory. Swaps are performed by modifying the permutation
// index, making them zero-copy operations.
template <typename MatrixType, std::size_t PermSize = kDynamic>
class RowPermuted {
 public:
  using ValueType = std::remove_cvref_t<decltype(std::declval<MatrixType>()[0, 0])>;

  // Construct with identity permutation (static size)
  constexpr explicit RowPermuted(const MatrixType& mat)
    requires(PermSize != kDynamic)
      : mat_{mat}, perm_{} {}

  constexpr explicit RowPermuted(MatrixType&& mat)
    requires(PermSize != kDynamic)
      : mat_{std::move(mat)}, perm_{} {}

  // Construct with identity permutation (dynamic size)
  constexpr explicit RowPermuted(const MatrixType& mat)
    requires(PermSize == kDynamic)
      : mat_{mat}, perm_{Permutation<kDynamic>(mat.extent().extent(0))} {}

  constexpr explicit RowPermuted(MatrixType&& mat)
    requires(PermSize == kDynamic)
      : mat_{std::move(mat)}, perm_{Permutation<kDynamic>(mat.extent().extent(0))} {}

  // Construct with explicit permutation
  constexpr RowPermuted(const MatrixType& mat,
                        const Permutation<PermSize>& perm)
      : mat_{mat}, perm_{perm} {}

  constexpr RowPermuted(MatrixType&& mat, const Permutation<PermSize>& perm)
      : mat_{std::move(mat)}, perm_{perm} {}

  constexpr RowPermuted(const MatrixType& mat, Permutation<PermSize>&& perm)
      : mat_{mat}, perm_{std::move(perm)} {}

  constexpr RowPermuted(MatrixType&& mat, Permutation<PermSize>&& perm)
      : mat_{std::move(mat)}, perm_{std::move(perm)} {}

  // Two-index access: permute row index
  template <typename... Indices>
    requires(sizeof...(Indices) == 2)
  constexpr auto operator[](this auto&& self, Indices... indices)
      -> decltype(auto) {
    auto [i, j] = std::tuple{indices...};
    if (std::is_constant_evaluated()) {
      assert(0 <= i &&
             static_cast<std::size_t>(i) < self.mat_.extent().extent(0) &&
             "row index out of bounds");
      assert(0 <= j &&
             static_cast<std::size_t>(j) < self.mat_.extent().extent(1) &&
             "col index out of bounds");
    }
    return self.mat_[self.perm_.data()[i], j];
  }

  // Extent accessors for compatibility
  constexpr auto rows() const -> std::size_t {
    return mat_.extent().extent(0);
  }
  constexpr auto cols() const -> std::size_t {
    return mat_.extent().extent(1);
  }
  constexpr auto extent() const -> auto {
    return mat_.extent();
  }

  // Zero-copy row swap via permutation modification
  constexpr void swap(std::size_t i, std::size_t j) {
    if (std::is_constant_evaluated()) {
      assert(i < mat_.extent().extent(0) && "first index out of bounds");
      assert(j < mat_.extent().extent(0) && "second index out of bounds");
    }
    perm_.swap(i, j);
  }

  // Access underlying permutation
  constexpr auto permutation() const -> const Permutation<PermSize>& {
    return perm_;
  }

  // Access underlying matrix
  constexpr auto data() const -> const MatrixType& { return mat_; }

 private:
  MatrixType mat_;
  Permutation<PermSize> perm_;
};

// Deduction guides for RowPermuted
template <typename MatrixType>
RowPermuted(MatrixType) -> RowPermuted<std::remove_cvref_t<MatrixType>, kDynamic>;

template <typename MatrixType, std::size_t PermSize>
RowPermuted(MatrixType, Permutation<PermSize>)
    -> RowPermuted<std::remove_cvref_t<MatrixType>, PermSize>;

// ColPermuted is a wrapper around a matrix that lets you "swap columns"
// without moving the data in memory. Swaps are performed by modifying the
// permutation index, making them zero-copy operations.
template <typename MatrixType, std::size_t PermSize = kDynamic>
class ColPermuted {
 public:
  using ValueType = std::remove_cvref_t<decltype(std::declval<MatrixType>()[0, 0])>;

  // Construct with identity permutation (static size)
  constexpr explicit ColPermuted(const MatrixType& mat)
    requires(PermSize != kDynamic)
      : mat_{mat}, perm_{} {}

  constexpr explicit ColPermuted(MatrixType&& mat)
    requires(PermSize != kDynamic)
      : mat_{std::move(mat)}, perm_{} {}

  // Construct with identity permutation (dynamic size)
  constexpr explicit ColPermuted(const MatrixType& mat)
    requires(PermSize == kDynamic)
      : mat_{mat}, perm_{Permutation<kDynamic>(mat.extent().extent(1))} {}

  constexpr explicit ColPermuted(MatrixType&& mat)
    requires(PermSize == kDynamic)
      : mat_{std::move(mat)}, perm_{Permutation<kDynamic>(mat.extent().extent(1))} {}

  // Construct with explicit permutation
  constexpr ColPermuted(const MatrixType& mat,
                        const Permutation<PermSize>& perm)
      : mat_{mat}, perm_{perm} {}

  constexpr ColPermuted(MatrixType&& mat, const Permutation<PermSize>& perm)
      : mat_{std::move(mat)}, perm_{perm} {}

  constexpr ColPermuted(const MatrixType& mat, Permutation<PermSize>&& perm)
      : mat_{mat}, perm_{std::move(perm)} {}

  constexpr ColPermuted(MatrixType&& mat, Permutation<PermSize>&& perm)
      : mat_{std::move(mat)}, perm_{std::move(perm)} {}

  // Two-index access: permute column index
  template <typename... Indices>
    requires(sizeof...(Indices) == 2)
  constexpr auto operator[](this auto&& self, Indices... indices)
      -> decltype(auto) {
    auto [i, j] = std::tuple{indices...};
    if (std::is_constant_evaluated()) {
      assert(0 <= i &&
             static_cast<std::size_t>(i) < self.mat_.extent().extent(0) &&
             "row index out of bounds");
      assert(0 <= j &&
             static_cast<std::size_t>(j) < self.mat_.extent().extent(1) &&
             "col index out of bounds");
    }
    return self.mat_[i, self.perm_.data()[j]];
  }

  // Extent accessors for compatibility
  constexpr auto rows() const -> std::size_t {
    return mat_.extent().extent(0);
  }
  constexpr auto cols() const -> std::size_t {
    return mat_.extent().extent(1);
  }
  constexpr auto extent() const -> auto {
    return mat_.extent();
  }

  // Zero-copy column swap via permutation modification
  constexpr void swap(std::size_t i, std::size_t j) {
    if (std::is_constant_evaluated()) {
      assert(i < mat_.extent().extent(1) && "first index out of bounds");
      assert(j < mat_.extent().extent(1) && "second index out of bounds");
    }
    perm_.swap(i, j);
  }

  // Access underlying permutation
  constexpr auto permutation() const -> const Permutation<PermSize>& {
    return perm_;
  }

  // Access underlying matrix
  constexpr auto data() const -> const MatrixType& { return mat_; }

 private:
  MatrixType mat_;
  Permutation<PermSize> perm_;
};

// Deduction guides for ColPermuted
template <typename MatrixType>
ColPermuted(MatrixType) -> ColPermuted<std::remove_cvref_t<MatrixType>, kDynamic>;

template <typename MatrixType, std::size_t PermSize>
ColPermuted(MatrixType, Permutation<PermSize>)
    -> ColPermuted<std::remove_cvref_t<MatrixType>, PermSize>;

}  // namespace tempura::matrix3
