#pragma once

#include <cassert>
#include <cmath>
#include <concepts>
#include <cstddef>

#include "matrix.h"
#include "vec.h"

namespace tempura {

// Cholesky decomposition A = L·Lᵀ for symmetric positive-definite A, L lower-triangular
// with positive diagonal (Cholesky–Banachiewicz, row by row). No pivoting — SPD is
// inherently stable. The workhorse for Bayesian inference: multivariate-normal log-density,
// covariance parameterization, and HMC mass matrices all factor a covariance this way.
//
// Design: a free function over a MatrixView returning the factor below. Square,
// ASSUMED symmetric (only the lower triangle is read), floating-point only. A non-PD input
// is FLAGGED (positive_definite=false), never masked by a 1/ε diagonal clamp (matrix3's
// bug, which let solve() return silent garbage); solve()/inverse()/logDeterminant assert PD.

template <typename T, typename Extents>
struct Cholesky {
  using value_type = T;
  using extents_type = Extents;
  HeapResult<T, Extents> factor;     // L in the lower triangle (upper zeroed)
  bool positive_definite = true;

  constexpr auto rows() const -> std::size_t { return static_cast<std::size_t>(factor.extent(0)); }
};

template <MatrixView M>
  requires std::floating_point<typename ViewOf<M>::value_type>
constexpr auto cholesky(const M& a) {
  auto va = view(a);
  using E = typename decltype(va)::extents_type;
  using T = typename decltype(va)::value_type;
  static_assert(dimsCompatible(E::static_extent(0), E::static_extent(1)), "Cholesky needs a square matrix");
  assert(va.extent(0) == va.extent(1) && "Cholesky requires a square matrix");
  const std::size_t n = static_cast<std::size_t>(va.extent(0));

  Cholesky<T, E> ch{HeapResult<T, E>(va), true};
  auto& L = ch.factor;
  for (std::size_t i = 0; i < n; ++i) {
    for (std::size_t j = 0; j < i; ++j) {  // L[i,j] = (A[i,j] − Σ_{k<j} L[i,k]L[j,k]) / L[j,j]
      T sum = L[i, j];
      for (std::size_t k = 0; k < j; ++k) sum -= L[i, k] * L[j, k];
      L[i, j] = sum / L[j, j];
    }
    T diag = L[i, i];  // L[i,i] = √(A[i,i] − Σ_{k<i} L[i,k]²)
    for (std::size_t k = 0; k < i; ++k) diag -= L[i, k] * L[i, k];
    if (diag <= T{}) {  // not positive definite — flag and stop, never clamp-and-continue
      ch.positive_definite = false;
      break;
    }
    L[i, i] = std::sqrt(diag);
    for (std::size_t j = i + 1; j < n; ++j) L[i, j] = T{};  // zero the unused upper triangle
  }
  return ch;
}

// Solve A·x = b via L(Lᵀx) = b (forward then backward substitution). Fresh owned result.
template <typename T, typename E, VecView B>
constexpr auto solve(const Cholesky<T, E>& ch, const B& b) {
  auto vb = view(b);
  const std::size_t n = ch.rows();
  assert(static_cast<std::size_t>(vb.extent(0)) == n && "RHS size must match the system");
  assert(ch.positive_definite && "solve() on a non-positive-definite Cholesky");
  const auto& L = ch.factor;

  HeapResult<T, std::extents<std::size_t, E::static_extent(0)>> x(n);
  for (std::size_t i = 0; i < n; ++i) x[i] = static_cast<T>(vb[i]);
  for (std::size_t i = 0; i < n; ++i) {  // forward: L·y = b
    for (std::size_t j = 0; j < i; ++j) x[i] -= L[i, j] * x[j];
    x[i] /= L[i, i];
  }
  for (std::size_t i = n; i-- > 0;) {  // backward: Lᵀ·x = y  (Lᵀ[i,j] = L[j,i])
    for (std::size_t j = i + 1; j < n; ++j) x[i] -= L[j, i] * x[j];
    x[i] /= L[i, i];
  }
  return x;
}

// log|A| = 2·Σ log(Lᵢᵢ) — the numerically stable form (no determinant overflow), the one
// Bayesian log-densities actually need. PRECONDITION: positive definite (asserted).
template <typename T, typename E>
constexpr auto logDeterminant(const Cholesky<T, E>& ch) -> T {
  assert(ch.positive_definite && "logDeterminant of a non-positive-definite Cholesky");
  T sum{};
  for (std::size_t i = 0; i < ch.rows(); ++i) sum += std::log(ch.factor[i, i]);
  return T{2} * sum;
}

// det(A) = (Π Lᵢᵢ)².
template <typename T, typename E>
constexpr auto determinant(const Cholesky<T, E>& ch) -> T {
  assert(ch.positive_definite && "determinant of a non-positive-definite Cholesky");
  T prod{1};
  for (std::size_t i = 0; i < ch.rows(); ++i) prod *= ch.factor[i, i];
  return prod * prod;
}

// A⁻¹ by solving A·X = I column by column (SPD inverse). PRECONDITION: positive definite.
template <typename T, typename E>
constexpr auto inverse(const Cholesky<T, E>& ch) {
  const std::size_t n = ch.rows();
  assert(ch.positive_definite && "inverse of a non-positive-definite Cholesky");
  const auto& L = ch.factor;
  HeapResult<T, E> inv(n, n);
  for (std::size_t c = 0; c < n; ++c) {  // solve A·x = eₖ into column c
    for (std::size_t i = 0; i < n; ++i) inv[i, c] = (i == c) ? T{1} : T{};
    for (std::size_t i = 0; i < n; ++i) {  // forward
      for (std::size_t j = 0; j < i; ++j) inv[i, c] -= L[i, j] * inv[j, c];
      inv[i, c] /= L[i, i];
    }
    for (std::size_t i = n; i-- > 0;) {  // backward
      for (std::size_t j = i + 1; j < n; ++j) inv[i, c] -= L[j, i] * inv[j, c];
      inv[i, c] /= L[i, i];
    }
  }
  return inv;
}

}  // namespace tempura
