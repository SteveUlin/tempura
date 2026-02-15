#pragma once

#include <type_traits>

#include "symbolic4/core.h"
#include "symbolic4/matrix_algebra/structure.h"

// ============================================================================
// matrix_symbol.h - Dimension-typed matrix symbols with structure tags
// ============================================================================
//
// MatrixSymbol<Id, RowDim, ColDim, Structure> represents a named matrix in the
// symbolic system. Dimension tags (RowDim, ColDim) are the same empty structs
// used for plates — they name runtime-sized dimensions.
//
// MatrixSymbol does NOT derive from SymbolicTag. Matrix expressions live in a
// separate type universe (MatrixExprTag) to avoid ambiguity with scalar operators.
// The bridge functions (element, trace, logDet) cross from matrix → scalar space.
//
// Vectors are matrices with ColDim = Unit (singleton column dimension).
//
// ============================================================================

namespace tempura::symbolic4 {

// ============================================================================
// MatrixExprTag - Base tag for all matrix-valued expressions
// ============================================================================
//
// Parallel to SymbolicTag for scalars. Matrix expressions form their own
// concept hierarchy, with their own operator overloads. The element() function
// is the bridge: it takes a MatrixExprTag type and produces a SymbolicTag type.

struct MatrixExprTag {};

template <typename T>
concept MatrixExprLike = DerivedFrom<T, MatrixExprTag>;

// ============================================================================
// Unit - Singleton dimension tag for column vectors
// ============================================================================
//
// A vector is a matrix with one column: MatrixSymbol<Id, Dim, Unit, S>.
// Unit has size 1 and is effectively absent during evaluation — the ReduceOver
// over Unit is a no-op (one iteration).

struct Unit {};

// ============================================================================
// MatrixSymbol
// ============================================================================

template <typename Id, typename RowDim, typename ColDim,
          typename Structure = General>
struct MatrixSymbol : MatrixExprTag {
  using id_type = Id;
  using row_dim = RowDim;
  using col_dim = ColDim;
  using structure = Structure;
};

// ============================================================================
// Type Traits
// ============================================================================

template <typename T>
constexpr bool is_matrix_symbol_v =
    core_traits_detail::isSpecOf<T, MatrixSymbol>();

// Extract dimension tags from any matrix expression type
template <typename T>
struct RowDimOf;

template <typename Id, typename R, typename C, typename S>
struct RowDimOf<MatrixSymbol<Id, R, C, S>> {
  using type = R;
};

template <typename T>
using row_dim_t = typename RowDimOf<T>::type;

template <typename T>
struct ColDimOf;

template <typename Id, typename R, typename C, typename S>
struct ColDimOf<MatrixSymbol<Id, R, C, S>> {
  using type = C;
};

template <typename T>
using col_dim_t = typename ColDimOf<T>::type;

template <typename T>
struct StructureOf;

template <typename Id, typename R, typename C, typename S>
struct StructureOf<MatrixSymbol<Id, R, C, S>> {
  using type = S;
};

template <typename T>
using structure_t = typename StructureOf<T>::type;

// Is this a square matrix?
template <typename T>
constexpr bool is_square_matrix_v = std::is_same_v<row_dim_t<T>, col_dim_t<T>>;

// ============================================================================
// Factory Functions
// ============================================================================
//
// Each factory generates a unique Id via decltype([] {}), the same lambda-type
// trick used throughout symbolic4 for unique identity.

// General rectangular matrix
template <typename RowDim, typename ColDim, typename Structure = General,
          typename Id = decltype([] {})>
constexpr auto matrixSym() {
  return MatrixSymbol<Id, RowDim, ColDim, Structure>{};
}

// Square matrix with optional structure
template <typename Dim, typename Structure = General,
          typename Id = decltype([] {})>
constexpr auto squareMatrixSym() {
  return MatrixSymbol<Id, Dim, Dim, Structure>{};
}

// Positive definite covariance matrix
template <typename Dim, typename Id = decltype([] {})>
constexpr auto covMatrixSym() {
  return MatrixSymbol<Id, Dim, Dim, PositiveDefinite>{};
}

// Column vector (matrix with ColDim = Unit)
template <typename Dim, typename Structure = General,
          typename Id = decltype([] {})>
constexpr auto vectorSym() {
  return MatrixSymbol<Id, Dim, Unit, Structure>{};
}

}  // namespace tempura::symbolic4
