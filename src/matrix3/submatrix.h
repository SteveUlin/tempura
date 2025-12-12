#pragma once

#include <cassert>
#include <cstdint>
#include <type_traits>
#include <utility>
#include <vector>

#include "extents.h"
#include "layouts.h"

namespace tempura::matrix3 {

// Forward declaration
template <typename Scalar, std::size_t... Ns>
class Dense;

// Submatrix is a zero-cost view into a rectangular region of a parent matrix.
// For a parent matrix M, Submatrix(M, row_offset, col_offset, rows, cols)
// provides a view into M[row_offset:row_offset+rows, col_offset:col_offset+cols].
// Access pattern: Submatrix[i,j] returns M[i + row_offset, j + col_offset]
//
// Supports both const and mutable access through "deducing this".
// Nested submatrices are supported by composing offsets.
template <typename MatrixType>
class Submatrix {
 public:
  using ValueType = std::remove_cvref_t<decltype(std::declval<MatrixType>()[0, 0])>;

  // Helper to detect Submatrix types
  template <typename T>
  struct is_submatrix : std::false_type {};

  template <typename M>
  struct is_submatrix<Submatrix<M>> : std::true_type {};

  // Create submatrix from a parent matrix with given offset and dimensions
  constexpr Submatrix(MatrixType& mat, std::size_t row_offset,
                      std::size_t col_offset, std::size_t rows, std::size_t cols)
    requires(!is_submatrix<std::remove_cvref_t<MatrixType>>::value)
      : mat_{mat},
        row_offset_{row_offset},
        col_offset_{col_offset},
        rows_{rows},
        cols_{cols} {
    if (std::is_constant_evaluated()) {
      assert(row_offset + rows <= matRows(mat) && "submatrix exceeds parent rows");
      assert(col_offset + cols <= matCols(mat) && "submatrix exceeds parent cols");
    }
  }

  // Create nested submatrix (composes offsets)
  template <typename ParentMatrixType>
  constexpr Submatrix(Submatrix<ParentMatrixType>& parent,
                      std::size_t row_offset, std::size_t col_offset,
                      std::size_t rows, std::size_t cols)
      : mat_{parent.mat_},
        row_offset_{parent.row_offset_ + row_offset},
        col_offset_{parent.col_offset_ + col_offset},
        rows_{rows},
        cols_{cols} {
    if (std::is_constant_evaluated()) {
      assert(row_offset + rows <= parent.rows() &&
             "nested submatrix exceeds parent rows");
      assert(col_offset + cols <= parent.cols() &&
             "nested submatrix exceeds parent cols");
    }
  }

  // Two-index access: offset indices into parent
  template <typename... Indices>
    requires(sizeof...(Indices) == 2)
  constexpr auto operator[](this auto&& self, Indices... indices)
      -> decltype(auto) {
    auto [i, j] = std::tuple{indices...};
    if (std::is_constant_evaluated()) {
      assert(0 <= i &&
             static_cast<std::size_t>(i) < self.rows_ &&
             "row index out of bounds");
      assert(0 <= j &&
             static_cast<std::size_t>(j) < self.cols_ &&
             "col index out of bounds");
    }
    return self.mat_[i + self.row_offset_, j + self.col_offset_];
  }

  // Extent accessors
  constexpr auto rows() const -> std::size_t { return rows_; }
  constexpr auto cols() const -> std::size_t { return cols_; }

  // Pattern A interface for compatibility with toString() and generic algorithms
  constexpr auto extent() const -> Extents<std::size_t, kDynamic, kDynamic> {
    return Extents<std::size_t, kDynamic, kDynamic>{rows_, cols_};
  }

  // Access to offset (useful for nested submatrices)
  constexpr auto rowOffset() const -> std::size_t { return row_offset_; }
  constexpr auto colOffset() const -> std::size_t { return col_offset_; }

  // Materialize view into a concrete Dense matrix
  template <typename OutScalar = ValueType>
  constexpr auto materialize() const -> Dense<OutScalar, kDynamic, kDynamic> {
    Extents<std::size_t, kDynamic, kDynamic> extents{rows(), cols()};
    std::vector<OutScalar> data(rows() * cols());

    // Fill in column-major order (LayoutLeft)
    for (std::size_t j = 0; j < cols(); ++j) {
      for (std::size_t i = 0; i < rows(); ++i) {
        data[i + j * rows()] = static_cast<OutScalar>((*this)[i, j]);
      }
    }

    return Dense<OutScalar, kDynamic, kDynamic>{
        extents,
        typename LayoutLeft::Mapping<Extents<std::size_t, kDynamic, kDynamic>, std::size_t>{extents},
        std::move(data)};
  }

  // Access underlying matrix
  constexpr auto data() const -> const MatrixType& { return mat_; }

 private:
  // Helpers to get dimensions from different matrix types
  template <typename M>
  static constexpr auto matRows(const M& m) -> std::size_t {
    if constexpr (requires { m.extent(); }) {
      return m.extent().extent(0);
    } else if constexpr (requires { m.rows(); }) {
      return m.rows();
    } else {
      // For Transpose or other views
      static_assert(requires { m.cols(); });
      return m.cols();
    }
  }

  template <typename M>
  static constexpr auto matCols(const M& m) -> std::size_t {
    if constexpr (requires { m.extent(); }) {
      return m.extent().extent(1);
    } else if constexpr (requires { m.cols(); }) {
      return m.cols();
    } else {
      // For Transpose or other views
      static_assert(requires { m.rows(); });
      return m.rows();
    }
  }

  MatrixType& mat_;
  std::size_t row_offset_;
  std::size_t col_offset_;
  std::size_t rows_;
  std::size_t cols_;
};

// Deduction guides
template <typename MatrixType>
Submatrix(MatrixType&, std::size_t, std::size_t, std::size_t, std::size_t)
    -> Submatrix<MatrixType>;

template <typename ParentMatrixType>
Submatrix(Submatrix<ParentMatrixType>&, std::size_t, std::size_t, std::size_t, std::size_t)
    -> Submatrix<ParentMatrixType>;

// Factory function for convenient submatrix creation
template <typename MatrixType>
constexpr auto makeSubmatrix(MatrixType& mat, std::size_t row_offset,
                              std::size_t col_offset, std::size_t rows,
                              std::size_t cols) -> Submatrix<MatrixType> {
  return Submatrix<MatrixType>{mat, row_offset, col_offset, rows, cols};
}

}  // namespace tempura::matrix3
