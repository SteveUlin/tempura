#pragma once

#include <type_traits>

#include "symbolic4/core.h"
#include "symbolic4/matrix_algebra/matrix_symbol.h"

// ============================================================================
// matrix_expr.h - Matrix expression nodes and operator overloads
// ============================================================================
//
// Matrix operations produce types in the MatrixExprTag hierarchy:
//   MatMulExpr<A, B>    — matrix multiply (dimension contraction)
//   TransposeExpr<A>    — matrix transpose (dimension swap)
//
// Scalar-boundary operations cross from MatrixExprTag → SymbolicTag:
//   LogDetNode<A>       — log-determinant (produces a scalar symbolic node)
//   trace(A)            — eagerly decomposes to SumOver (see decompose.h)
//
// Operator overloads on MatrixExprLike don't conflict with scalar operator*
// because the two concept hierarchies are disjoint.
//
// ============================================================================

namespace tempura::symbolic4 {

// ============================================================================
// MatMulExpr - Matrix multiply: C = A × B
// ============================================================================
//
// Dimension contraction: col_dim of A must equal row_dim of B.
// The contracted dimension disappears in the result:
//   A ∈ ℝ^{R×K}, B ∈ ℝ^{K×C} → A×B ∈ ℝ^{R×C}

template <MatrixExprLike A, MatrixExprLike B>
struct MatMulExpr : MatrixExprTag {
  using lhs_type = A;
  using rhs_type = B;
};

// Dimension traits for MatMulExpr
template <MatrixExprLike A, MatrixExprLike B>
struct RowDimOf<MatMulExpr<A, B>> {
  using type = row_dim_t<A>;
};

template <MatrixExprLike A, MatrixExprLike B>
struct ColDimOf<MatMulExpr<A, B>> {
  using type = col_dim_t<B>;
};

template <MatrixExprLike A, MatrixExprLike B>
struct StructureOf<MatMulExpr<A, B>> {
  using type = General;  // A*B is generally unstructured
};

// ============================================================================
// TransposeExpr - Matrix transpose: Aᵀ
// ============================================================================
//
// Swaps row and column dimensions. Structure transforms via TransposeStructure.
// For symmetric types, transpose() returns the input itself (see operator below).

template <MatrixExprLike A>
struct TransposeExpr : MatrixExprTag {
  using arg_type = A;
};

template <MatrixExprLike A>
struct RowDimOf<TransposeExpr<A>> {
  using type = col_dim_t<A>;
};

template <MatrixExprLike A>
struct ColDimOf<TransposeExpr<A>> {
  using type = row_dim_t<A>;
};

template <MatrixExprLike A>
struct StructureOf<TransposeExpr<A>> {
  using type = transpose_structure_t<structure_t<A>>;
};

// ============================================================================
// LogDetNode - log|A| as a scalar symbolic expression
// ============================================================================
//
// Crosses the matrix → scalar boundary. Derives from SymbolicTag so it can
// participate in scalar expressions (grad, evaluate, simplify).
//
// No simple element-wise decomposition exists — evaluation requires matrix-level
// computation (Cholesky → sum of log diagonals, or LU → sum of log |pivots|).

template <MatrixExprLike A>
struct LogDetNode : SymbolicTag {
  using matrix_type = A;
};

// ============================================================================
// is_matrix_expr_v - Detect matrix-valued expressions
// ============================================================================

template <typename T>
constexpr bool is_matrix_expr_v = MatrixExprLike<T>;

// ============================================================================
// Operator Overloads
// ============================================================================

// Matrix multiply: A * B
// Dimension compatibility enforced at compile time.
template <MatrixExprLike A, MatrixExprLike B>
constexpr auto operator*(A, B) {
  static_assert(std::is_same_v<col_dim_t<A>, row_dim_t<B>>,
                "Matrix dimension mismatch: col_dim of A must equal row_dim of B");
  return MatMulExpr<A, B>{};
}

// Transpose: symmetric types are identity (Aᵀ = A), others wrap in TransposeExpr
template <MatrixExprLike A>
constexpr auto transpose(A a) {
  if constexpr (is_symmetric_v<structure_t<A>>) {
    return a;
  } else {
    return TransposeExpr<A>{};
  }
}

// Log-determinant: requires square matrix. Returns a scalar symbolic node.
template <MatrixExprLike A>
  requires(is_square_matrix_v<A>)
constexpr auto logDet(A) {
  return LogDetNode<A>{};
}

}  // namespace tempura::symbolic4
