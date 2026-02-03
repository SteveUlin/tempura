#pragma once

#include "symbolic4/core.h"
#include "symbolic4/strategy/diff.h"

// ============================================================================
// grad.h - Typed-rank gradient objects via nesting
// ============================================================================
//
// Grad<Expr> wraps an expression and computes derivatives on demand via
// operator[]. Each grad() call adds one covariant index -- rank is encoded
// in the type:
//
//   Grad<Scalar>             -- rank 1 covector.  g[x] -> scalar.
//   Grad<Grad<Scalar>>       -- rank 2 tensor.    H[x, y] -> scalar.
//   Grad<Grad<Grad<Scalar>>> -- rank 3 tensor.    T[x, y, z] -> scalar.
//
// Single indexing on a higher-rank object peels one level:
//   H[x] on Grad<Grad<F>> -> Grad<df/dx> (the x-row of the Hessian)
//
// C++26 multidim operator[] extracts scalar components from higher-rank
// tensors. The rank is determined by type nesting, not argument count.
//
// Grad is NOT a SymbolicTag -- it's a tensor lookup object, not an
// expression node.
//
// Usage:
//   auto g = grad(f);       // rank 1 covector
//   g[x]                    // df/dx (scalar expression)
//
//   auto H = hessian(f);    // = grad(grad(f)), rank 2
//   H[x, y]                 // d2f/dxdy (scalar expression)
//   H[x]                    // Grad<df/dx> -- still rank 1
//
// ============================================================================

namespace tempura::symbolic4 {

// Forward declare
template <typename E>
struct Grad;

// Type trait: detect Grad<...> (handles const/volatile qualifiers)
template <typename T>
struct IsGrad : std::false_type {};

template <typename E>
struct IsGrad<Grad<E>> : std::true_type {};

template <typename T>
struct IsGrad<const T> : IsGrad<T> {};

template <typename T>
struct IsGrad<volatile T> : IsGrad<T> {};

template <typename T>
struct IsGrad<const volatile T> : IsGrad<T> {};

template <typename T>
constexpr bool is_grad_v = IsGrad<T>::value;

// Grad wraps an expression (or another Grad) and computes derivatives on demand.
// NOT a SymbolicTag -- Grad is a tensor lookup object, not an expression node.
template <typename Expr>
struct Grad {
  [[no_unique_address]] Expr expr_;

  constexpr Grad() = default;
  constexpr explicit Grad(Expr e) : expr_{e} {}

  // Single index: only allowed on rank-1 Grad (plain expression).
  // For rank-2+ (Hessian etc.), use operator[x, y] or row(x).
  template <typename V>
  constexpr auto operator[](V) const {
    static_assert(!is_grad_v<Expr>,
        "Single index on rank-2+ Grad is ambiguous. "
        "Use H[x, y] for scalar entry, or H.row(x) for the x-row covector.");
    return differentiate(expr_, V{});
  }

  // C++26 multidim operator[]: peel indices one at a time from outside in.
  // H[x, y] differentiates twice to get scalar Hessian entry.
  template <typename V1, typename V2, typename... Vs>
  constexpr auto operator[](V1, V2 v2, Vs... vs) const {
    if constexpr (is_grad_v<Expr>) {
      // Peel one layer: differentiate inner expression, wrap result
      auto df = differentiate(expr_.expr_, V1{});
      auto inner = Grad<decltype(df)>{df};
      return inner[v2, vs...];  // recurse (may hit single-index or multidim)
    } else {
      // Should not happen: multidim on rank-1 Grad
      static_assert(is_grad_v<Expr>,
          "Too many indices for gradient rank.");
      return differentiate(expr_, V1{});
    }
  }

  // Explicit partial indexing: returns the x-row of a higher-rank object.
  // H.row(x) -> Grad<df/dx> (a covector)
  template <typename V>
  constexpr auto row(V) const {
    static_assert(is_grad_v<Expr>,
        "row() only makes sense on rank-2+ Grad. Use g[x] for rank-1.");
    auto df = differentiate(expr_.expr_, V{});
    return Grad<decltype(df)>{df};
  }
};

// grad of a plain symbolic expression -> Grad<E> (rank 1 covector)
template <Symbolic E>
constexpr auto grad(E expr) {
  return Grad<E>{expr};
}

// grad of a Grad -> Grad<Grad<E>> (adds one rank)
template <typename E>
constexpr auto grad(Grad<E> g) {
  return Grad<Grad<E>>{g};
}

// Convenience: hessian(f) = grad(grad(f)) (rank 2)
template <Symbolic E>
constexpr auto hessian(E expr) {
  return grad(grad(expr));
}

}  // namespace tempura::symbolic4
