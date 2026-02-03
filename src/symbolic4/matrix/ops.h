#pragma once

#include <cassert>
#include <cmath>
#include <cstddef>

#include "symbolic4/core.h"

// ============================================================================
// matrix/ops.h - Symbolic matrix operations for linear algebra
// ============================================================================
//
// This file provides operator types and symbolic functions for matrix and
// vector operations commonly used in probabilistic programming, particularly
// for multivariate normal distributions and their Cholesky parameterizations.
//
// Operations provided:
//   dot          - Vector dot product: dot(v1, v2) -> scalar
//   logDetChol   - Log-determinant via Cholesky: logDetChol(L) -> scalar
//   solveTriangular - Forward substitution: solveTriangular(L, b) -> vector
//   diagPreMult  - Diagonal pre-multiply: diagPreMult(d, L) -> matrix
//   quadFormChol - Quadratic form via Cholesky: quadFormChol(x, L) -> scalar
//
// Design notes:
//   - Operators follow the same pattern as meta/function_objects.h
//   - Each Op struct is both a tag type and a callable functor
//   - Symbolic functions return Expression<Op, Args...> for AST construction
//   - Runtime evaluation via operator() for numeric computation
//
// ============================================================================

namespace tempura {

// ============================================================================
// Matrix Operation Functors
// ============================================================================
//
// These structs serve dual purposes:
//   1. Tag types for Expression<Op, ...> - identifies the operation in the AST
//   2. Callable functors - compute the result when given concrete values
//
// The operator() implementations assume standard container interfaces:
//   - Vectors: operator[](i), size()
//   - Matrices: operator[](i, j) or operator()(i, j), rows(), cols()

// Dot product: dot(v1, v2) = Σᵢ v1[i] * v2[i]
struct DotOp {
  template <typename V1, typename V2>
  constexpr auto operator()(const V1& v1, const V2& v2) const {
    assert(v1.size() == v2.size());
    using T = decltype(v1[0] * v2[0]);
    T result{};
    for (size_t i = 0; i < v1.size(); ++i) {
      result += v1[i] * v2[i];
    }
    return result;
  }
};

// Log-determinant of Cholesky factor: logDetChol(L) = Σᵢ log(L[i,i])
// For positive definite matrix A with A = LLᵀ: log|A| = 2·logDetChol(L)
struct LogDetCholOp {
  template <typename L>
  constexpr auto operator()(const L& chol) const {
    using std::log;
    using T = decltype(log(chol[0, 0]));
    T result{};
    auto n = chol.rows();
    for (size_t i = 0; i < n; ++i) {
      result += log(chol[i, i]);
    }
    return result;
  }
};

// Forward substitution: solve Lx = b for x, where L is lower triangular
// Returns L⁻¹b without explicitly forming the inverse
struct SolveTriangularOp {
  template <typename L, typename B>
  constexpr auto operator()(const L& chol, const B& b) const {
    assert(chol.rows() == b.size());
    auto n = b.size();
    B x = b;  // Copy for result
    for (size_t i = 0; i < n; ++i) {
      for (size_t j = 0; j < i; ++j) {
        x[i] -= chol[i, j] * x[j];
      }
      x[i] /= chol[i, i];
    }
    return x;
  }
};

// Diagonal pre-multiply: diagPreMult(d, L) = diag(d) × L
// Scales each row i of L by d[i]
struct DiagPreMultOp {
  template <typename D, typename L>
  constexpr auto operator()(const D& diag, const L& mat) const {
    assert(diag.size() == mat.rows());
    L result = mat;  // Copy for result
    auto rows = mat.rows();
    auto cols = mat.cols();
    for (size_t i = 0; i < rows; ++i) {
      for (size_t j = 0; j < cols; ++j) {
        result[i, j] = diag[i] * mat[i, j];
      }
    }
    return result;
  }
};

// Quadratic form via Cholesky: quadFormChol(x, L) = ||L⁻¹x||² = xᵀ(LLᵀ)⁻¹x
// Computes the Mahalanobis distance squared using forward substitution
struct QuadFormCholOp {
  template <typename X, typename L>
  constexpr auto operator()(const X& x, const L& chol) const {
    // First solve Ly = x (forward substitution)
    auto y = SolveTriangularOp{}(chol, x);
    // Then compute ||y||²
    return DotOp{}(y, y);
  }
};

}  // namespace tempura

namespace tempura::symbolic4 {

// Import operator types into symbolic4 namespace
using tempura::DiagPreMultOp;
using tempura::DotOp;
using tempura::LogDetCholOp;
using tempura::QuadFormCholOp;
using tempura::SolveTriangularOp;

// ============================================================================
// Symbolic Functions
// ============================================================================
//
// These functions construct symbolic expressions from their arguments.
// They return Expression<Op, Args...>, encoding the operation in the type.
//
// When evaluated (via an interpreter), the corresponding Op's operator()
// is called with the evaluated argument values.

// Vector dot product: dot(v1, v2) -> scalar
template <Symbolic V1, Symbolic V2>
constexpr auto dot(V1 v1, V2 v2) {
  return Expression<DotOp, V1, V2>{v1, v2};
}

// Log-determinant of Cholesky factor: logDetChol(L) -> scalar
// Returns sum of log of diagonal elements: Σᵢ log(L[i,i])
template <Symbolic L>
constexpr auto logDetChol(L chol) {
  return Expression<LogDetCholOp, L>{chol};
}

// Forward substitution: solveTriangular(L, b) -> vector
// Solves Lx = b where L is lower triangular, returns x = L⁻¹b
template <Symbolic L, Symbolic B>
constexpr auto solveTriangular(L chol, B b) {
  return Expression<SolveTriangularOp, L, B>{chol, b};
}

// Diagonal pre-multiply: diagPreMult(d, L) -> matrix
// Returns diag(d) × L, scaling each row i by d[i]
template <Symbolic D, Symbolic L>
constexpr auto diagPreMult(D diag, L mat) {
  return Expression<DiagPreMultOp, D, L>{diag, mat};
}

// Quadratic form via Cholesky: quadFormChol(x, L) -> scalar
// Computes ||L⁻¹x||² = xᵀ(LLᵀ)⁻¹x (Mahalanobis distance squared)
template <Symbolic X, Symbolic L>
constexpr auto quadFormChol(X x, L chol) {
  return Expression<QuadFormCholOp, X, L>{x, chol};
}

}  // namespace tempura::symbolic4
