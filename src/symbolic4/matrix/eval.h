#pragma once

#include <cassert>
#include <cmath>
#include <cstddef>
#include <initializer_list>
#include <type_traits>
#include <utility>
#include <vector>

#include "symbolic4/core.h"
#include "symbolic4/matrix/diff.h"
#include "symbolic4/matrix/ops.h"
#include "symbolic4/matrix/types.h"
#include "symbolic4/scheme/cata.h"

// ============================================================================
// matrix/eval.h - Evaluation support for matrix operations
// ============================================================================
//
// Extends the symbolic evaluation system to handle matrix-valued bindings
// and matrix operations using std::vector-based storage.
//
// Key types:
//   DimVectorBinding - Binds dimension-tagged vector to std::vector<double>
//   CholeskyBinding  - Binds Cholesky factor to row-major matrix storage
//
// Evaluation rules for matrix operations:
//   DotOp           - Vector dot product
//   LogDetCholOp    - Sum of log of diagonal elements
//   SolveTriangularOp - Forward substitution
//   DiagPreMultOp   - Diagonal scaling of matrix
//   QuadFormCholOp  - Solve then dot
//
// ============================================================================

namespace tempura::symbolic4 {

// ============================================================================
// Simple Matrix/Vector Types
// ============================================================================
//
// Lightweight wrappers around std::vector for runtime-sized vectors and
// matrices. These provide the interface expected by the Op functors.

// Runtime-sized vector
class DynamicVector {
 public:
  DynamicVector() = default;
  explicit DynamicVector(std::size_t n) : data_(n, 0.0) {}
  DynamicVector(std::initializer_list<double> init) : data_{init} {}

  auto size() const -> std::size_t { return data_.size(); }
  auto operator[](std::size_t i) const -> double { return data_[i]; }
  auto operator[](std::size_t i) -> double& { return data_[i]; }

  auto begin() { return data_.begin(); }
  auto end() { return data_.end(); }
  auto begin() const { return data_.begin(); }
  auto end() const { return data_.end(); }

 private:
  std::vector<double> data_;
};

// Vector subtraction: element-wise v1 - v2
inline auto operator-(const DynamicVector& a, const DynamicVector& b) -> DynamicVector {
  assert(a.size() == b.size());
  DynamicVector result(a.size());
  for (std::size_t i = 0; i < a.size(); ++i) {
    result[i] = a[i] - b[i];
  }
  return result;
}

// Runtime-sized matrix (row-major storage)
class DynamicMatrix {
 public:
  DynamicMatrix() = default;
  DynamicMatrix(std::size_t rows, std::size_t cols)
      : rows_{rows}, cols_{cols}, data_(rows * cols, 0.0) {}

  auto rows() const -> std::size_t { return rows_; }
  auto cols() const -> std::size_t { return cols_; }

  // Row-major access: data_[i * cols_ + j]
  auto operator[](std::size_t i, std::size_t j) const -> double {
    return data_[i * cols_ + j];
  }
  auto operator[](std::size_t i, std::size_t j) -> double& {
    return data_[i * cols_ + j];
  }

  // Convenience for diagonal access
  auto diagonal(std::size_t i) const -> double { return (*this)[i, i]; }

 private:
  std::size_t rows_{0};
  std::size_t cols_{0};
  std::vector<double> data_;
};

// ============================================================================
// Vector and Matrix Bindings
// ============================================================================
//
// Bindings connect symbolic matrix/vector symbols to concrete values.

// Binding for dimension-tagged vectors
template <typename Sym>
  requires is_dim_vector_symbol_v<Sym>
struct DimVectorBinding {
  using symbol_type = Sym;

  DynamicVector values;

  explicit DimVectorBinding(DynamicVector v) : values{std::move(v)} {}

  template <typename... Ts>
  explicit DimVectorBinding(Ts... args) : values{static_cast<double>(args)...} {}

  auto size() const { return values.size(); }
  auto operator[](std::size_t i) const { return values[i]; }

  // BinderPack lookup interface
  const DynamicVector& operator[](Sym) const { return values; }
};

// Binding for Cholesky factor matrices
template <typename Sym>
  requires is_cholesky_symbol_v<Sym>
struct CholeskyBinding {
  using symbol_type = Sym;

  DynamicMatrix values;

  explicit CholeskyBinding(DynamicMatrix m) : values{std::move(m)} {}

  auto rows() const { return values.rows(); }
  auto cols() const { return values.cols(); }

  // BinderPack lookup interface
  const DynamicMatrix& operator[](Sym) const { return values; }
};

// Type traits for bindings
template <typename T>
struct IsDimVectorBinding : std::false_type {};

template <typename Sym>
struct IsDimVectorBinding<DimVectorBinding<Sym>> : std::true_type {};

template <typename T>
constexpr bool is_dim_vector_binding_v = IsDimVectorBinding<T>::value;

template <typename T>
struct IsCholeskyBinding : std::false_type {};

template <typename Sym>
struct IsCholeskyBinding<CholeskyBinding<Sym>> : std::true_type {};

template <typename T>
constexpr bool is_cholesky_binding_v = IsCholeskyBinding<T>::value;

// ============================================================================
// MatrixBinderPack
// ============================================================================
//
// Extends BinderPack pattern to support mixed scalar and matrix bindings.

template <typename... Binders>
struct MatrixBinderPack : Binders... {
  MatrixBinderPack(Binders... binders) : Binders{binders}... {}
  using Binders::operator[]...;
};

template <typename... Binders>
MatrixBinderPack(Binders...) -> MatrixBinderPack<Binders...>;

// ============================================================================
// Matrix Operation Implementations
// ============================================================================

namespace matrix_eval_detail {

// Dot product: v1ᵀ v2
inline auto evalDot(const DynamicVector& v1, const DynamicVector& v2) -> double {
  assert(v1.size() == v2.size());
  double result = 0.0;
  for (std::size_t i = 0; i < v1.size(); ++i) {
    result += v1[i] * v2[i];
  }
  return result;
}

// Log-determinant of Cholesky: Σᵢ log(L[i,i])
inline auto evalLogDetChol(const DynamicMatrix& l) -> double {
  double result = 0.0;
  for (std::size_t i = 0; i < l.rows(); ++i) {
    result += std::log(l.diagonal(i));
  }
  return result;
}

// Forward substitution: solve Lx = b for x
inline auto evalSolveTriangular(const DynamicMatrix& l, const DynamicVector& b)
    -> DynamicVector {
  assert(l.rows() == b.size());
  auto n = b.size();
  DynamicVector x(n);

  for (std::size_t i = 0; i < n; ++i) {
    double sum = b[i];
    for (std::size_t j = 0; j < i; ++j) {
      sum -= l[i, j] * x[j];
    }
    x[i] = sum / l[i, i];
  }
  return x;
}

// Diagonal pre-multiply: diag(d) × L
inline auto evalDiagPreMult(const DynamicVector& d, const DynamicMatrix& l)
    -> DynamicMatrix {
  assert(d.size() == l.rows());
  DynamicMatrix result(l.rows(), l.cols());
  for (std::size_t i = 0; i < l.rows(); ++i) {
    for (std::size_t j = 0; j < l.cols(); ++j) {
      result[i, j] = d[i] * l[i, j];
    }
  }
  return result;
}

// Quadratic form: ||L⁻¹x||² = xᵀ(LLᵀ)⁻¹x
inline auto evalQuadFormChol(const DynamicVector& x, const DynamicMatrix& l)
    -> double {
  auto y = evalSolveTriangular(l, x);
  return evalDot(y, y);
}

// Vector subtraction: a - b
inline auto evalVectorSub(const DynamicVector& a, const DynamicVector& b)
    -> DynamicVector {
  assert(a.size() == b.size());
  DynamicVector result(a.size());
  for (std::size_t i = 0; i < a.size(); ++i) {
    result[i] = a[i] - b[i];
  }
  return result;
}

// LogDetCholDerivOp: derivative of log|det(L)| w.r.t. L
// Formula: trace(L⁻¹ dL) = Σᵢ dL[i,i] / L[i,i]
// For triangular L, only the diagonal contributes to the trace
inline auto evalLogDetCholDeriv(const DynamicMatrix& l, const DynamicMatrix& dl)
    -> double {
  assert(l.rows() == dl.rows());
  double result = 0.0;
  for (std::size_t i = 0; i < l.rows(); ++i) {
    result += dl.diagonal(i) / l.diagonal(i);
  }
  return result;
}

// QuadFormCholDerivLOp: derivative of ||L⁻¹x||² w.r.t. L
// Formula: -2 yᵀ L⁻¹ dL y where y = L⁻¹x
// Expands to: -2 Σᵢⱼ y[i] (L⁻¹ dL)[i,j] y[j]
//           = -2 Σᵢⱼ y[i] (L⁻¹ dL y)[i]
//           = -2 yᵀ (L⁻¹ dL y)
// where L⁻¹ dL y is computed by forward substitution on (dL y)
inline auto evalQuadFormCholDerivL(const DynamicVector& x,
                                    const DynamicMatrix& l,
                                    const DynamicMatrix& dl) -> double {
  assert(l.rows() == x.size());
  assert(l.rows() == dl.rows());

  // y = L⁻¹x
  auto y = evalSolveTriangular(l, x);

  // Compute dL * y (matrix-vector product)
  auto n = y.size();
  DynamicVector dl_y(n);
  for (std::size_t i = 0; i < n; ++i) {
    double sum = 0.0;
    for (std::size_t j = 0; j < n; ++j) {
      sum += dl[i, j] * y[j];
    }
    dl_y[i] = sum;
  }

  // L⁻¹(dL * y) via forward substitution
  auto inv_l_dl_y = evalSolveTriangular(l, dl_y);

  // -2 yᵀ (L⁻¹ dL y)
  return -2.0 * evalDot(y, inv_l_dl_y);
}

}  // namespace matrix_eval_detail

// ============================================================================
// MatrixEval Interpreter
// ============================================================================
//
// Extends evaluation to handle matrix-valued terminals and matrix operations.

template <typename Bindings>
struct MatrixEval {
  // Terminal handling
  template <typename T>
  static auto terminal(T term, Bindings& ctx) {
    if constexpr (is_constant_v<T>) {
      return static_cast<double>(T::value);
    } else if constexpr (is_fraction_v<T>) {
      return T::value;
    } else if constexpr (is_literal_v<T>) {
      return static_cast<double>(term.effect_.value);
    } else if constexpr (is_dim_vector_symbol_v<T> || is_cholesky_symbol_v<T>) {
      return ctx[term];
    } else if constexpr (is_atom_v<T>) {
      return ctx[term];
    } else {
      static_assert(is_atom_v<T>, "Unknown terminal type");
      return 0.0;
    }
  }

  // Combine child results using the operator functor
  template <typename Op, typename... Rs>
  static auto combine(Bindings&, Rs... children) {
    if constexpr (std::is_same_v<Op, DotOp>) {
      return matrix_eval_detail::evalDot(children...);
    } else if constexpr (std::is_same_v<Op, LogDetCholOp>) {
      return matrix_eval_detail::evalLogDetChol(children...);
    } else if constexpr (std::is_same_v<Op, SolveTriangularOp>) {
      return matrix_eval_detail::evalSolveTriangular(children...);
    } else if constexpr (std::is_same_v<Op, DiagPreMultOp>) {
      return matrix_eval_detail::evalDiagPreMult(children...);
    } else if constexpr (std::is_same_v<Op, QuadFormCholOp>) {
      return matrix_eval_detail::evalQuadFormChol(children...);
    } else if constexpr (std::is_same_v<Op, LogDetCholDerivOp>) {
      return matrix_eval_detail::evalLogDetCholDeriv(children...);
    } else if constexpr (std::is_same_v<Op, QuadFormCholDerivLOp>) {
      return matrix_eval_detail::evalQuadFormCholDerivL(children...);
    } else if constexpr (std::is_same_v<Op, SubOp> && sizeof...(Rs) == 2) {
      // Special handling for vector subtraction
      return combineSubOp(children...);
    } else {
      // Fall back to standard operator evaluation for scalar ops
      return Op{}(children...);
    }
  }

 private:
  // SubOp handling: dispatch based on operand types
  static auto combineSubOp(const DynamicVector& a, const DynamicVector& b) {
    return matrix_eval_detail::evalVectorSub(a, b);
  }

  // Scalar subtraction fallback
  template <typename A, typename B>
    requires(!std::is_same_v<A, DynamicVector>)
  static auto combineSubOp(A a, B b) {
    return a - b;
  }
};

// ============================================================================
// Convenience function: evaluateMatrix(expr, bindings...)
// ============================================================================

template <Symbolic E, typename... Binders>
auto evaluateMatrix(E expr, MatrixBinderPack<Binders...> bindings) {
  return fold<MatrixEval<MatrixBinderPack<Binders...>>>(expr, bindings);
}

template <Symbolic E, typename... Args>
auto evaluateMatrix(E expr, Args... binders) {
  return evaluateMatrix(expr, MatrixBinderPack{binders...});
}

}  // namespace tempura::symbolic4
