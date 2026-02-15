#include "symbolic4/matrix_algebra/matrix_algebra.h"

#include "unit.h"

using namespace tempura;
using namespace tempura::symbolic4;

// Dimension tags for testing
struct Rows {};
struct Cols {};
struct Inner {};
struct Dims {};

auto main() -> int {
  // ========================================================================
  // Structure Tags
  // ========================================================================

  "Structure — is_symmetric_v"_test = [] {
    static_assert(is_symmetric_v<Symmetric>);
    static_assert(is_symmetric_v<PositiveDefinite>);
    static_assert(is_symmetric_v<Diagonal>);
    static_assert(!is_symmetric_v<General>);
    static_assert(!is_symmetric_v<LowerTriangular>);
    static_assert(!is_symmetric_v<UpperTriangular>);
  };

  "Structure — is_triangular_v"_test = [] {
    static_assert(is_triangular_v<LowerTriangular>);
    static_assert(is_triangular_v<UpperTriangular>);
    static_assert(!is_triangular_v<General>);
    static_assert(!is_triangular_v<Symmetric>);
    static_assert(!is_triangular_v<Diagonal>);
    static_assert(!is_triangular_v<PositiveDefinite>);
  };

  "Structure — TransposeStructure"_test = [] {
    static_assert(std::is_same_v<transpose_structure_t<General>, General>);
    static_assert(std::is_same_v<transpose_structure_t<Symmetric>, Symmetric>);
    static_assert(
        std::is_same_v<transpose_structure_t<PositiveDefinite>, PositiveDefinite>);
    static_assert(std::is_same_v<transpose_structure_t<Diagonal>, Diagonal>);
    static_assert(
        std::is_same_v<transpose_structure_t<LowerTriangular>, UpperTriangular>);
    static_assert(
        std::is_same_v<transpose_structure_t<UpperTriangular>, LowerTriangular>);
  };

  // ========================================================================
  // MatrixSymbol
  // ========================================================================

  "MatrixSymbol — construction and traits"_test = [] {
    using M = MatrixSymbol<struct MId, Rows, Cols, General>;
    static_assert(is_matrix_symbol_v<M>);
    static_assert(std::is_same_v<row_dim_t<M>, Rows>);
    static_assert(std::is_same_v<col_dim_t<M>, Cols>);
    static_assert(std::is_same_v<structure_t<M>, General>);
  };

  "MatrixSymbol — square detection"_test = [] {
    using Square = MatrixSymbol<struct SId, Dims, Dims, Symmetric>;
    using Rect = MatrixSymbol<struct RId, Rows, Cols, General>;
    static_assert(is_square_matrix_v<Square>);
    static_assert(!is_square_matrix_v<Rect>);
  };

  "MatrixSymbol — is MatrixExprLike"_test = [] {
    using M = MatrixSymbol<struct MId, Rows, Cols>;
    static_assert(MatrixExprLike<M>);
    static_assert(is_matrix_expr_v<M>);
  };

  "MatrixSymbol — is NOT Symbolic"_test = [] {
    // MatrixSymbol lives in a separate type universe from scalar expressions
    using M = MatrixSymbol<struct MId, Rows, Cols>;
    static_assert(!Symbolic<M>);
  };

  "MatrixSymbol — factory: matrixSym"_test = [] {
    auto A = matrixSym<Rows, Cols>();
    using AT = decltype(A);
    static_assert(is_matrix_symbol_v<AT>);
    static_assert(std::is_same_v<row_dim_t<AT>, Rows>);
    static_assert(std::is_same_v<col_dim_t<AT>, Cols>);
    static_assert(std::is_same_v<structure_t<AT>, General>);
  };

  "MatrixSymbol — factory: squareMatrixSym"_test = [] {
    auto S = squareMatrixSym<Dims, Symmetric>();
    using ST = decltype(S);
    static_assert(is_matrix_symbol_v<ST>);
    static_assert(is_square_matrix_v<ST>);
    static_assert(std::is_same_v<structure_t<ST>, Symmetric>);
  };

  "MatrixSymbol — factory: covMatrixSym"_test = [] {
    auto C = covMatrixSym<Dims>();
    using CT = decltype(C);
    static_assert(is_matrix_symbol_v<CT>);
    static_assert(is_square_matrix_v<CT>);
    static_assert(std::is_same_v<structure_t<CT>, PositiveDefinite>);
  };

  "MatrixSymbol — factory: vectorSym"_test = [] {
    auto v = vectorSym<Dims>();
    using VT = decltype(v);
    static_assert(is_matrix_symbol_v<VT>);
    static_assert(std::is_same_v<row_dim_t<VT>, Dims>);
    static_assert(std::is_same_v<col_dim_t<VT>, Unit>);
  };

  // ========================================================================
  // Matrix Multiply
  // ========================================================================

  "MatMul — expression building"_test = [] {
    auto A = matrixSym<Rows, Inner>();
    auto B = matrixSym<Inner, Cols>();
    auto C = A * B;
    using CT = decltype(C);

    // Result is a MatMulExpr
    static_assert(MatrixExprLike<CT>);
    static_assert(is_matrix_expr_v<CT>);
  };

  "MatMul — result dimensions"_test = [] {
    auto A = matrixSym<Rows, Inner>();
    auto B = matrixSym<Inner, Cols>();
    auto C = A * B;
    using CT = decltype(C);

    static_assert(std::is_same_v<row_dim_t<CT>, Rows>);
    static_assert(std::is_same_v<col_dim_t<CT>, Cols>);
    static_assert(std::is_same_v<structure_t<CT>, General>);
  };

  "MatMul — chain A*B*C"_test = [] {
    auto A = matrixSym<Rows, Inner>();
    auto B = matrixSym<Inner, Cols>();
    auto C = matrixSym<Cols, Dims>();
    auto D = A * B * C;  // (A*B)*C
    using DT = decltype(D);

    static_assert(std::is_same_v<row_dim_t<DT>, Rows>);
    static_assert(std::is_same_v<col_dim_t<DT>, Dims>);
  };

  // ========================================================================
  // Transpose
  // ========================================================================

  "Transpose — expression building"_test = [] {
    auto A = matrixSym<Rows, Cols>();
    auto At = transpose(A);
    using AtT = decltype(At);

    static_assert(MatrixExprLike<AtT>);
    static_assert(std::is_same_v<row_dim_t<AtT>, Cols>);
    static_assert(std::is_same_v<col_dim_t<AtT>, Rows>);
  };

  "Transpose — symmetric no-op"_test = [] {
    auto S = squareMatrixSym<Dims, Symmetric>();
    auto St = transpose(S);

    // Symmetric transpose returns the same type (identity)
    static_assert(std::is_same_v<decltype(S), decltype(St)>);
  };

  "Transpose — PositiveDefinite no-op"_test = [] {
    auto P = covMatrixSym<Dims>();
    auto Pt = transpose(P);

    static_assert(std::is_same_v<decltype(P), decltype(Pt)>);
  };

  "Transpose — Diagonal no-op"_test = [] {
    auto D = squareMatrixSym<Dims, Diagonal>();
    auto Dt = transpose(D);

    static_assert(std::is_same_v<decltype(D), decltype(Dt)>);
  };

  "Transpose — LowerTriangular flips"_test = [] {
    auto L = squareMatrixSym<Dims, LowerTriangular>();
    auto Lt = transpose(L);
    using LtT = decltype(Lt);

    static_assert(std::is_same_v<structure_t<LtT>, UpperTriangular>);
  };

  // ========================================================================
  // LogDet
  // ========================================================================

  "LogDet — expression building"_test = [] {
    auto A = squareMatrixSym<Dims>();
    auto ld = logDet(A);
    using LdT = decltype(ld);

    // LogDet crosses the boundary: matrix → scalar
    static_assert(Symbolic<LdT>);          // IS a scalar symbolic expression
    static_assert(!MatrixExprLike<LdT>);   // is NOT a matrix expression
  };

  // ========================================================================
  // Scalar Decomposition — element()
  // ========================================================================

  "Decompose — MatrixSymbol element"_test = [] {
    using M = MatrixSymbol<struct MId, Rows, Cols, General>;
    auto e = element(M{}, Tag<Rows>{}, Tag<Cols>{});
    using ET = decltype(e);

    // Decomposed to IndexedSymbol with same Id and dimensions
    static_assert(is_indexed_symbol_v<ET>);
    static_assert(Symbolic<ET>);  // Crosses into scalar world
  };

  "Decompose — MatMul element produces SumOver"_test = [] {
    using A = MatrixSymbol<struct AId, Rows, Inner, General>;
    using B = MatrixSymbol<struct BId, Inner, Cols, General>;
    auto ab = MatMulExpr<A, B>{};
    auto e = element(ab, Tag<Rows>{}, Tag<Cols>{});
    using ET = decltype(e);

    // MatMul decomposes to SumOver<Inner>(...)
    static_assert(is_reduce_over_v<ET>);
    static_assert(is_sum_over_v<ET>);
    static_assert(Symbolic<ET>);
  };

  "Decompose — Transpose element swaps indices"_test = [] {
    using M = MatrixSymbol<struct MId, Rows, Cols, General>;
    auto mt = TransposeExpr<M>{};

    // element(Mᵀ, i, j) = element(M, j, i)
    auto e_trans = element(mt, Tag<Cols>{}, Tag<Rows>{});
    auto e_orig = element(M{}, Tag<Rows>{}, Tag<Cols>{});

    // Both should produce the same IndexedSymbol type
    static_assert(std::is_same_v<decltype(e_trans), decltype(e_orig)>);
  };

  "Decompose — Trace produces SumOver"_test = [] {
    using M = MatrixSymbol<struct MId, Dims, Dims, General>;
    auto t = trace(M{});
    using TT = decltype(t);

    // trace(A) decomposes to SumOver<Dims>(element(A, Dims, Dims))
    static_assert(is_reduce_over_v<TT>);
    static_assert(is_sum_over_v<TT>);
    static_assert(Symbolic<TT>);
  };

  // ========================================================================
  // Mixed scalar + matrix expressions
  // ========================================================================

  "Mixed — trace(A) + scalar"_test = [] {
    using M = MatrixSymbol<struct MId, Dims, Dims, General>;
    Symbol<struct Sigma> sigma;

    // trace produces a scalar Symbolic, so it can combine with other scalars
    auto f = trace(M{}) + sigma * sigma;
    static_assert(Symbolic<decltype(f)>);
    static_assert(is_expression_v<decltype(f)>);
  };

  "Mixed — logDet(A) + scalar"_test = [] {
    using M = MatrixSymbol<struct MId, Dims, Dims, General>;
    Symbol<struct Sigma> sigma;

    auto f = logDet(M{}) + sigma;
    static_assert(Symbolic<decltype(f)>);
  };

  return TestRegistry::result();
}
