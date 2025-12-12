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

// Transpose is a zero-cost wrapper that swaps row/column indices.
// For a matrix M with extent [m,n], Transpose(M) has extent [n,m].
// Access pattern: Transpose[i,j] returns M[j,i]
//
// Supports both const and mutable access through "deducing this".
template <typename MatrixType>
class Transpose {
 public:
  using ValueType = std::remove_cvref_t<decltype(std::declval<MatrixType>()[0, 0])>;

  constexpr explicit Transpose(MatrixType& mat) : mat_{mat} {}

  // Two-index access: swap indices
  template <typename... Indices>
    requires(sizeof...(Indices) == 2)
  constexpr auto operator[](this auto&& self, Indices... indices)
      -> decltype(auto) {
    auto [i, j] = std::tuple{indices...};
    if (std::is_constant_evaluated()) {
      assert(0 <= i &&
             static_cast<std::size_t>(i) < self.rows() &&
             "row index out of bounds");
      assert(0 <= j &&
             static_cast<std::size_t>(j) < self.cols() &&
             "col index out of bounds");
    }
    return self.mat_[j, i];  // Swap indices
  }

  // Extent accessors: swapped dimensions
  constexpr auto rows() const -> std::size_t {
    // Use cols() for all matrix types to avoid circular dependencies
    return rowsImpl(mat_);
  }
  constexpr auto cols() const -> std::size_t {
    // Use rows() for all matrix types to avoid circular dependencies
    return colsImpl(mat_);
  }

  // Pattern A interface for compatibility with toString() and generic algorithms
  constexpr auto extent() const -> Extents<std::size_t, kDynamic, kDynamic> {
    return Extents<std::size_t, kDynamic, kDynamic>{rows(), cols()};
  }

 private:
  // Helper to detect if a type has extent() method
  template <typename M>
  struct has_extent : std::bool_constant<requires(M m) { m.extent(); }> {};

  // Helper to get dimension from matrices with extent()
  template <typename M>
  static constexpr auto rowsImpl(const M& m) -> std::size_t {
    if constexpr (has_extent<M>::value) {
      return m.extent().extent(1);  // Return cols of underlying
    } else {
      return m.cols();  // For Transpose<...>, return cols which gives underlying rows
    }
  }

  template <typename M>
  static constexpr auto colsImpl(const M& m) -> std::size_t {
    if constexpr (has_extent<M>::value) {
      return m.extent().extent(0);  // Return rows of underlying
    } else {
      return m.rows();  // For Transpose<...>, return rows which gives underlying cols
    }
  }

 public:

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
  MatrixType& mat_;
};

// Deduction guide
template <typename MatrixType>
Transpose(MatrixType&) -> Transpose<MatrixType>;

// HermitianTranspose is conjugate transpose: A^H[i,j] = conj(A[j,i])
// For real matrices, this is identical to regular transpose.
// For complex matrices, each element is conjugated.
//
// Supports both const and mutable access through "deducing this".
template <typename MatrixType>
class HermitianTranspose {
 public:
  using ValueType = std::remove_cvref_t<decltype(std::declval<MatrixType>()[0, 0])>;

  constexpr explicit HermitianTranspose(MatrixType& mat) : mat_{mat} {}

  // Helper to conjugate complex values, identity for real values
  template <typename T>
  static constexpr auto conj(const T& val) -> T {
    if constexpr (requires { val.real(); val.imag(); }) {
      // std::complex-like type
      return T{val.real(), -val.imag()};
    } else {
      // Real scalar type
      return val;
    }
  }

  // Two-index access: swap indices and conjugate
  template <typename... Indices>
    requires(sizeof...(Indices) == 2)
  constexpr auto operator[](this auto&& self, Indices... indices)
      -> ValueType {
    auto [i, j] = std::tuple{indices...};
    if (std::is_constant_evaluated()) {
      assert(0 <= i &&
             static_cast<std::size_t>(i) < self.rows() &&
             "row index out of bounds");
      assert(0 <= j &&
             static_cast<std::size_t>(j) < self.cols() &&
             "col index out of bounds");
    }
    return conj(self.mat_[j, i]);  // Swap indices and conjugate
  }

  // Extent accessors: swapped dimensions (same as Transpose)
  constexpr auto rows() const -> std::size_t {
    return rowsImpl(mat_);
  }
  constexpr auto cols() const -> std::size_t {
    return colsImpl(mat_);
  }

  // Pattern A interface for compatibility with toString() and generic algorithms
  constexpr auto extent() const -> Extents<std::size_t, kDynamic, kDynamic> {
    return Extents<std::size_t, kDynamic, kDynamic>{rows(), cols()};
  }

 private:
  // Helper to detect if a type has extent() method
  template <typename M>
  struct has_extent : std::bool_constant<requires(M m) { m.extent(); }> {};

  // Helper to get dimension from matrices with extent()
  template <typename M>
  static constexpr auto rowsImpl(const M& m) -> std::size_t {
    if constexpr (has_extent<M>::value) {
      return m.extent().extent(1);  // Return cols of underlying
    } else {
      return m.cols();  // For views, return cols which gives underlying rows
    }
  }

  template <typename M>
  static constexpr auto colsImpl(const M& m) -> std::size_t {
    if constexpr (has_extent<M>::value) {
      return m.extent().extent(0);  // Return rows of underlying
    } else {
      return m.rows();  // For views, return rows which gives underlying cols
    }
  }

 public:

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
  MatrixType& mat_;
};

// Deduction guide
template <typename MatrixType>
HermitianTranspose(MatrixType&) -> HermitianTranspose<MatrixType>;

}  // namespace tempura::matrix3
