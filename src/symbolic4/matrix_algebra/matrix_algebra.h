#pragma once

// ============================================================================
// matrix_algebra.h - Symbolic matrix algebra for symbolic4
// ============================================================================
//
// Provides dimension-typed, structure-tagged matrix symbols and operations
// that decompose to scalar ReduceOver expressions for evaluation.
//
// Usage:
//   struct Rows {};
//   struct Cols {};
//   struct Inner {};
//
//   auto A = matrixSym<Rows, Inner>();
//   auto B = matrixSym<Inner, Cols>();
//   auto C = A * B;                         // MatMulExpr — matrix multiply
//   auto t = trace(squareMatrixSym<Rows>()); // SumOver — scalar
//
//   // Scalar decomposition via element():
//   auto c_ij = element(C, Tag<Rows>{}, Tag<Cols>{});
//   // → SumOver<Inner>(IndexedSymbol<AId, Rows, Inner>
//   //                   * IndexedSymbol<BId, Inner, Cols>)
//
// ============================================================================

#include "symbolic4/matrix_algebra/structure.h"
#include "symbolic4/matrix_algebra/matrix_symbol.h"
#include "symbolic4/matrix_algebra/matrix_expr.h"
#include "symbolic4/matrix_algebra/decompose.h"
