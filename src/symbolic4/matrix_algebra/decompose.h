#pragma once

#include "meta/tags.h"
#include "symbolic4/indexed/dim.h"
#include "symbolic4/indexed/reduce_over.h"
#include "symbolic4/matrix_algebra/matrix_expr.h"
#include "symbolic4/operators.h"

// ============================================================================
// decompose.h - Scalar decomposition of matrix expressions
// ============================================================================
//
// The key bridge between matrix algebra and the scalar expression system.
//
// element(M, Tag<RowDim>, Tag<ColDim>) extracts a scalar symbolic expression
// for one component of a matrix expression. The result is an IndexedSymbol or
// ReduceOver expression that the existing evaluation machinery handles directly.
//
// Einstein summation falls out naturally:
//   element(A * B, i, j) = SumOver<K>(element(A, i, k) * element(B, k, j))
//
// Tag<DimTag> serves as the index variable — during ReduceOver evaluation,
// the DimTag's current loop index is looked up in the StaticDimIndexMap.
//
// ============================================================================

namespace tempura::symbolic4 {

// ============================================================================
// element() — Extract scalar component from a matrix expression
// ============================================================================

// Base case: MatrixSymbol → IndexedSymbol
// The matrix symbol decomposes to a 2D indexed symbol with the same Id.
// Evaluation uses IndexedBinding<IndexedSymbol<Id, R, C>, 2> with the current
// dimension indices from the ReduceOver context.
template <typename Id, typename R, typename C, typename S>
constexpr auto element(MatrixSymbol<Id, R, C, S>, Tag<R>, Tag<C>) {
  return IndexedSymbol<Id, R, C>{};
}

// MatMul: element(A×B, i, j) = Σ_k element(A, i, k) · element(B, k, j)
// The contracted dimension K = col_dim_t<A> = row_dim_t<B> is summed over.
template <MatrixExprLike A, MatrixExprLike B, typename I, typename J>
constexpr auto element(MatMulExpr<A, B>, Tag<I>, Tag<J>) {
  using K = col_dim_t<A>;
  auto a_ik = element(A{}, Tag<I>{}, Tag<K>{});
  auto b_kj = element(B{}, Tag<K>{}, Tag<J>{});
  return sumOver(a_ik * b_kj, K{});
}

// Transpose: element(Aᵀ, i, j) = element(A, j, i)
template <MatrixExprLike A, typename I, typename J>
constexpr auto element(TransposeExpr<A>, Tag<I>, Tag<J>) {
  return element(A{}, Tag<J>{}, Tag<I>{});
}

// ============================================================================
// traceDecompose() — Decompose trace to SumOver
// ============================================================================
//
// trace(A) = Σ_i A[i,i] = SumOver<Dim>(element(A, Dim, Dim))
// where Dim = row_dim_t<A> = col_dim_t<A> (A must be square).

template <MatrixExprLike A>
  requires(is_square_matrix_v<A>)
constexpr auto traceDecompose(A) {
  using Dim = row_dim_t<A>;
  return sumOver(element(A{}, Tag<Dim>{}, Tag<Dim>{}), Dim{});
}

// ============================================================================
// trace() definition — forward-declared in matrix_expr.h
// ============================================================================

template <MatrixExprLike A>
  requires(is_square_matrix_v<A>)
constexpr auto trace(A a) {
  return traceDecompose(a);
}

}  // namespace tempura::symbolic4
