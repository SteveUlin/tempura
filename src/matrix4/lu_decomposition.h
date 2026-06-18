#pragma once

#include <cassert>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <limits>
#include <mdspan>
#include <span>
#include <utility>
#include <vector>

#include "matrix.h"
#include "permutation.h"
#include "vec.h"

namespace tempura {

// LU decomposition with scale-invariant partial pivoting (Numerical Recipes §2.3):
// P·A = L·U, L unit-lower-triangular, U upper-triangular. L and U share one workspace
// — multipliers below the diagonal (L, implicit unit diagonal), U on and above.
//
// matrix4 design: free functions over a MatrixView, returning the Lu factor object
// below. Pivoting does EAGER physical row swaps in the owned workspace; the row map +
// parity ride along in a Permutation reused for the RHS. Eager beats deferring the swap
// to a gather-on-access — never slower (measured ~2–3% at large n, up to ~40% at tiny
// n) and the factors stay contiguous so solve/extract stream without indirection. Not a
// cache effect: in row-major, permuting which row you touch keeps each row's sweep
// contiguous; the gather only adds a per-row index load. Singularity is FLAGGED and
// asserted in solve(), never masked by a magic 1/ε perturbation (matrix3's bug).
// Floating-point only — Gaussian elimination needs field division (integer LU is a
// category error).

// The combined L\U factorization plus its pivot permutation. `factors` owns the
// workspace; `perm` carries the row pivots and parity for det and for permuting a RHS.
template <typename T, typename Extents>
struct Lu {
  using value_type = T;
  using extents_type = Extents;
  static constexpr std::size_t kRows = Extents::static_extent(0);

  HeapResult<T, Extents> factors;  // combined L\U (unit-diagonal L below, U on/above)
  Permutation<kRows> perm;         // row pivots + parity (det sign)
  bool singular = false;           // a pivot fell to ~‖A‖·ε — solve() refuses, det ≈ 0

  constexpr auto rows() const -> std::size_t { return static_cast<std::size_t>(factors.extent(0)); }
};

// Identity permutation of the right size: a static Permutation<N> for a static system,
// a dynamic one sized at runtime otherwise.
template <std::size_t N>
constexpr auto identityPerm(std::size_t n) -> Permutation<N> {
  if constexpr (N == std::dynamic_extent) {
    return Permutation<N>(n);
  } else {
    assert(n == N);
    return Permutation<N>{};
  }
}

// Factorize a square matrix view into P·A = L·U.
template <MatrixView M>
  requires std::floating_point<typename ViewOf<M>::value_type>
constexpr auto luDecompose(const M& a) {
  auto va = view(a);
  using E = typename decltype(va)::extents_type;
  using T = typename decltype(va)::value_type;
  static_assert(dimsCompatible(E::static_extent(0), E::static_extent(1)), "LU needs a square matrix");
  assert(va.extent(0) == va.extent(1) && "LU requires a square matrix");
  const std::size_t n = static_cast<std::size_t>(va.extent(0));

  Lu<T, E> lu{HeapResult<T, E>(va), identityPerm<E::static_extent(0)>(n), false};
  auto& f = lu.factors;

  // Implicit scaling: the largest magnitude per row, used both as the pivot denominator
  // and as the scale for the dimensionless singularity test below. A zero row is singular.
  std::vector<T> scale(n);
  for (std::size_t i = 0; i < n; ++i) {
    T s{};
    for (std::size_t j = 0; j < n; ++j) {
      const T m = std::abs(f[i, j]);
      if (m > s) s = m;
    }
    scale[i] = s;
    if (s == T{}) lu.singular = true;  // a zero row ⇒ singular
  }

  for (std::size_t i = 0; i < n; ++i) {
    // Scale-invariant partial pivot: argmax over ii ≥ i of |f[ii,i]| / scale[ii].
    std::size_t pivot = i;
    T best{};
    for (std::size_t ii = i; ii < n; ++ii) {
      const T sc = scale[ii] == T{} ? T{} : std::abs(f[ii, i]) / scale[ii];
      if (sc > best) {
        best = sc;
        pivot = ii;
      }
    }
    if (pivot != i) {
      for (std::size_t k = 0; k < n; ++k) {
        using std::swap;
        swap(f[i, k], f[pivot, k]);
      }
      std::swap(scale[i], scale[pivot]);
      lu.perm.swap(i, pivot);
    }
    // Scale-RELATIVE singularity test (matching the scale-invariant pivot metric): the
    // realized pivot is dimensionless against its row scale, so a well-conditioned but
    // badly-scaled system like diag(1e16, 1) is not mistaken for singular.
    const T rel = scale[i] == T{} ? T{} : std::abs(f[i, i]) / scale[i];
    if (rel <= std::numeric_limits<T>::epsilon() * static_cast<T>(n)) lu.singular = true;
    if (f[i, i] != T{}) {  // an exactly-zero pivot can't divide; leave the column, det → 0
      for (std::size_t j = i + 1; j < n; ++j) {
        f[j, i] = static_cast<T>(f[j, i] / f[i, i]);
        for (std::size_t k = i + 1; k < n; ++k)
          f[j, k] = static_cast<T>(f[j, k] - f[j, i] * f[i, k]);
      }
    }
  }
  return lu;
}

// Solve A·x = b for a single RHS. Returns a fresh owned vector (never mutates b, unlike
// matrix3). PRECONDITION: the factorization is non-singular (asserted).
template <typename T, typename E, VecView B>
constexpr auto solve(const Lu<T, E>& lu, const B& b) {
  auto vb = view(b);
  using R = Accumulator<typename decltype(vb)::value_type, T>;  // widen float → double
  const std::size_t n = lu.rows();
  assert(static_cast<std::size_t>(vb.extent(0)) == n && "RHS size must match the system");
  assert(!lu.singular && "solve() on a singular factorization");

  HeapResult<R, std::extents<std::size_t, E::static_extent(0)>> x(n);
  const auto ord = lu.perm.span();
  for (std::size_t i = 0; i < n; ++i) x[i] = static_cast<R>(vb[ord[i]]);  // x ← P·b (gather)
  const auto& f = lu.factors;
  for (std::size_t i = 1; i < n; ++i)  // forward substitution: L·y = P·b (unit diagonal)
    for (std::size_t j = 0; j < i; ++j) x[i] = static_cast<R>(x[i] - static_cast<R>(f[i, j]) * x[j]);
  for (std::size_t i = n; i-- > 0;) {  // back substitution: U·x = y
    for (std::size_t j = i + 1; j < n; ++j) x[i] = static_cast<R>(x[i] - static_cast<R>(f[i, j]) * x[j]);
    x[i] = static_cast<R>(x[i] / static_cast<R>(f[i, i]));
  }
  return x;
}

// Solve A·X = B for multiple RHS columns (each column of B independently).
template <typename T, typename E, MatrixView B>
constexpr auto solve(const Lu<T, E>& lu, const B& b) {
  auto vb = view(b);
  using R = Accumulator<typename decltype(vb)::value_type, T>;
  const std::size_t n = lu.rows();
  const std::size_t rhs = static_cast<std::size_t>(vb.extent(1));
  assert(static_cast<std::size_t>(vb.extent(0)) == n && "RHS rows must match the system");
  assert(!lu.singular && "solve() on a singular factorization");

  HeapResult<R, std::extents<std::size_t, E::static_extent(0), std::dynamic_extent>> x(n, rhs);
  for (std::size_t i = 0; i < n; ++i)
    for (std::size_t k = 0; k < rhs; ++k) x[i, k] = static_cast<R>(vb[i, k]);
  lu.perm.permuteRows(x);  // X ← P·B (rank-2 gather)
  const auto& f = lu.factors;
  for (std::size_t k = 0; k < rhs; ++k) {
    for (std::size_t i = 1; i < n; ++i)
      for (std::size_t j = 0; j < i; ++j)
        x[i, k] = static_cast<R>(x[i, k] - static_cast<R>(f[i, j]) * x[j, k]);
    for (std::size_t i = n; i-- > 0;) {
      for (std::size_t j = i + 1; j < n; ++j)
        x[i, k] = static_cast<R>(x[i, k] - static_cast<R>(f[i, j]) * x[j, k]);
      x[i, k] = static_cast<R>(x[i, k] / static_cast<R>(f[i, i]));
    }
  }
  return x;
}

// det(A) = sign(P) · ∏ Uᵢᵢ. From an existing factorization, or factoring internally.
template <typename T, typename E>
constexpr auto determinant(const Lu<T, E>& lu) -> T {
  T det = static_cast<T>(lu.perm.determinantSign());
  const std::size_t n = lu.rows();
  for (std::size_t i = 0; i < n; ++i) det *= lu.factors[i, i];
  return det;
}
template <MatrixView M>
  requires std::floating_point<typename ViewOf<M>::value_type>
constexpr auto determinant(const M& a) {
  return determinant(luDecompose(a));
}

// A⁻¹ via solving A·X = I. PRECONDITION: A is non-singular (asserted).
template <MatrixView M>
  requires std::floating_point<typename ViewOf<M>::value_type>
constexpr auto inverse(const M& a) {
  auto va = view(a);
  using E = typename decltype(va)::extents_type;
  using T = typename decltype(va)::value_type;
  const std::size_t n = static_cast<std::size_t>(va.extent(0));
  auto lu = luDecompose(a);
  assert(!lu.singular && "inverse of a singular matrix");
  HeapResult<T, E> id(n, n);
  identity(id);
  return solve(lu, id);
}

}  // namespace tempura
