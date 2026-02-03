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
#include "symbolic4/operators.h"

// ============================================================================
// matrix/eval.h - Evaluation support for matrix operations
// ============================================================================
//
// Extends the symbolic evaluation system to handle matrix-valued bindings
// and matrix operations. Uses direct recursion instead of cata/fold.
//
// ============================================================================

namespace tempura::symbolic4 {

// ============================================================================
// Simple Matrix/Vector Types
// ============================================================================

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

inline auto operator-(const DynamicVector& a, const DynamicVector& b) -> DynamicVector {
  assert(a.size() == b.size());
  DynamicVector result(a.size());
  for (std::size_t i = 0; i < a.size(); ++i) {
    result[i] = a[i] - b[i];
  }
  return result;
}

class DynamicMatrix {
 public:
  DynamicMatrix() = default;
  DynamicMatrix(std::size_t rows, std::size_t cols)
      : rows_{rows}, cols_{cols}, data_(rows * cols, 0.0) {}

  auto rows() const -> std::size_t { return rows_; }
  auto cols() const -> std::size_t { return cols_; }

  auto operator[](std::size_t i, std::size_t j) const -> double {
    return data_[i * cols_ + j];
  }
  auto operator[](std::size_t i, std::size_t j) -> double& {
    return data_[i * cols_ + j];
  }

  auto diagonal(std::size_t i) const -> double { return (*this)[i, i]; }

 private:
  std::size_t rows_{0};
  std::size_t cols_{0};
  std::vector<double> data_;
};

// ============================================================================
// Vector and Matrix Bindings
// ============================================================================

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

  const DynamicVector& operator[](Sym) const { return values; }
};

template <typename Sym>
  requires is_cholesky_symbol_v<Sym>
struct CholeskyBinding {
  using symbol_type = Sym;

  DynamicMatrix values;

  explicit CholeskyBinding(DynamicMatrix m) : values{std::move(m)} {}

  auto rows() const { return values.rows(); }
  auto cols() const { return values.cols(); }

  const DynamicMatrix& operator[](Sym) const { return values; }
};

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

inline auto evalDot(const DynamicVector& v1, const DynamicVector& v2) -> double {
  assert(v1.size() == v2.size());
  double result = 0.0;
  for (std::size_t i = 0; i < v1.size(); ++i) {
    result += v1[i] * v2[i];
  }
  return result;
}

inline auto evalLogDetChol(const DynamicMatrix& l) -> double {
  double result = 0.0;
  for (std::size_t i = 0; i < l.rows(); ++i) {
    result += std::log(l.diagonal(i));
  }
  return result;
}

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

inline auto evalQuadFormChol(const DynamicVector& x, const DynamicMatrix& l)
    -> double {
  auto y = evalSolveTriangular(l, x);
  return evalDot(y, y);
}

inline auto evalVectorSub(const DynamicVector& a, const DynamicVector& b)
    -> DynamicVector {
  assert(a.size() == b.size());
  DynamicVector result(a.size());
  for (std::size_t i = 0; i < a.size(); ++i) {
    result[i] = a[i] - b[i];
  }
  return result;
}

inline auto evalLogDetCholDeriv(const DynamicMatrix& l, const DynamicMatrix& dl)
    -> double {
  assert(l.rows() == dl.rows());
  double result = 0.0;
  for (std::size_t i = 0; i < l.rows(); ++i) {
    result += dl.diagonal(i) / l.diagonal(i);
  }
  return result;
}

inline auto evalQuadFormCholDerivL(const DynamicVector& x,
                                    const DynamicMatrix& l,
                                    const DynamicMatrix& dl) -> double {
  assert(l.rows() == x.size());
  assert(l.rows() == dl.rows());

  auto y = evalSolveTriangular(l, x);

  auto n = y.size();
  DynamicVector dl_y(n);
  for (std::size_t i = 0; i < n; ++i) {
    double sum = 0.0;
    for (std::size_t j = 0; j < n; ++j) {
      sum += dl[i, j] * y[j];
    }
    dl_y[i] = sum;
  }

  auto inv_l_dl_y = evalSolveTriangular(l, dl_y);

  return -2.0 * evalDot(y, inv_l_dl_y);
}

}  // namespace matrix_eval_detail

// ============================================================================
// Direct recursion matrix evaluation
// ============================================================================

namespace matrix_eval_impl {

// Forward declaration
template <Symbolic E, typename Bindings>
auto matrixEvalImpl(E expr, Bindings& ctx);

// Helper: evaluate Expression children and apply Op
template <typename Op, typename... Args, typename Bindings, SizeT... Is>
auto matrixEvalExpression(Expression<Op, Args...> expr, Bindings& ctx,
                          IndexSequence<Is...>) {
  // Evaluate children, then combine based on Op
  return matrixEvalCombine<Op>(matrixEvalImpl(expr.template arg<Is>(), ctx)...);
}

// Combine child results using the operator
template <typename Op, typename... Rs>
auto matrixEvalCombine(Rs... children) {
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
    return matrixEvalCombineSub(children...);
  } else {
    return Op{}(children...);
  }
}

// SubOp dispatch based on operand types
inline auto matrixEvalCombineSub(const DynamicVector& a, const DynamicVector& b) {
  return matrix_eval_detail::evalVectorSub(a, b);
}

template <typename A, typename B>
  requires(!std::is_same_v<A, DynamicVector>)
auto matrixEvalCombineSub(A a, B b) {
  return a - b;
}

// Terminal handling
template <typename T, typename Bindings>
auto matrixEvalTerminal(T term, Bindings& ctx) {
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

template <Symbolic E, typename Bindings>
auto matrixEvalImpl(E expr, Bindings& ctx) {
  if constexpr (is_expression_v<E>) {
    return matrixEvalExpression(expr, ctx, MakeIndexSequence<E::arity>{});
  } else {
    return matrixEvalTerminal(expr, ctx);
  }
}

}  // namespace matrix_eval_impl

// ============================================================================
// Convenience function: evaluateMatrix(expr, bindings...)
// ============================================================================

template <Symbolic E, typename... Binders>
auto evaluateMatrix(E expr, MatrixBinderPack<Binders...> bindings) {
  return matrix_eval_impl::matrixEvalImpl(expr, bindings);
}

template <Symbolic E, typename... Args>
auto evaluateMatrix(E expr, Args... binders) {
  return evaluateMatrix(expr, MatrixBinderPack{binders...});
}

}  // namespace tempura::symbolic4
