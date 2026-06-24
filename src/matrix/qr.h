#pragma once

#include <cassert>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <vector>

#include "matrix.h"
#include "vec.h"

namespace tempura {

// QR decomposition via Householder reflections (Numerical Recipes §2.10): A = Q·R, Q
// orthogonal (QᵀQ = I), R upper-triangular. The factor for least-squares and the
// building block for the symmetric-eigen / SVD iterations.
//
// Design: a free function over a MatrixView returning the Qr object below, which
// packs both factors into one workspace — R in the upper triangle, the Householder
// vectors vⱼ (with implicit v₀ = 1) in the strict lower triangle, τ stored separately —
// exactly as LAPACK does. formQ/formR materialize the explicit factors; solve applies
// Qᵀ then back-substitutes. Square only (asserted); floating-point only.
//
// NOTE: the reflector vⱼ = A[j:n, j] is a COLUMN, so the inner loops walk columns —
// strided in the row-major owner (the layout decision's "column-favoring" case). Correct
// as-is; a hot path would reflect over a transposed view or a materialized panel.

template <typename T, typename Extents>
struct Qr {
  using value_type = T;
  using extents_type = Extents;
  static constexpr std::size_t kRows = Extents::static_extent(0);

  HeapResult<T, Extents> factors;  // R upper, packed Householder vectors below diagonal
  std::vector<T> tau;              // Householder coefficients, length n
  bool singular = false;           // a column reduced to ~0 ⇒ zero R pivot; solve() refuses

  constexpr auto rows() const -> std::size_t { return static_cast<std::size_t>(factors.extent(0)); }
};

template <MatrixView M>
  requires std::floating_point<typename ViewOf<M>::value_type>
constexpr auto qrDecompose(const M& a) {
  auto va = view(a);
  using E = typename decltype(va)::extents_type;
  using T = typename decltype(va)::value_type;
  static_assert(dimsCompatible(E::static_extent(0), E::static_extent(1)), "QR needs a square matrix");
  assert(va.extent(0) == va.extent(1) && "QR requires a square matrix");
  const std::size_t n = static_cast<std::size_t>(va.extent(0));

  Qr<T, E> qr{HeapResult<T, E>(va), std::vector<T>(n)};
  auto& f = qr.factors;
  auto& tau = qr.tau;

  for (std::size_t j = 0; j < n; ++j) {
    // Scaled 2-norm of the sub-column f[j:n, j] (LAPACK dnrm2 trick): a naive Σxᵢ²
    // overflows to ∞ for large entries and underflows to 0 for tiny ones — the latter
    // would silently misclassify a nonzero column as "already zero" and drop its
    // reflector, breaking Q·R = A.
    T col_scale{};
    for (std::size_t i = j; i < n; ++i) col_scale = std::max(col_scale, std::abs(f[i, j]));
    if (col_scale == T{}) {  // genuinely zero sub-column ⇒ zero R pivot, A is singular
      tau[j] = T{};
      qr.singular = true;
      continue;
    }
    T ssq{};
    for (std::size_t i = j; i < n; ++i) {
      const T r = f[i, j] / col_scale;
      ssq += r * r;
    }
    const T norm = col_scale * std::sqrt(ssq);
    // α = −sign(x₀)·‖x‖ so that x₀ − α has the larger magnitude (no cancellation).
    const T alpha = (f[j, j] >= T{}) ? -norm : norm;
    tau[j] = (alpha - f[j, j]) / alpha;
    const T denom = f[j, j] - alpha;
    for (std::size_t i = j + 1; i < n; ++i) f[i, j] /= denom;  // v normalized, v₀ = 1 implicit
    f[j, j] = alpha;                                           // R diagonal

    for (std::size_t k = j + 1; k < n; ++k) {  // A ← (I − τvvᵀ)·A on trailing columns
      T dot = f[j, k];                         // v₀ = 1
      for (std::size_t i = j + 1; i < n; ++i) dot += f[i, j] * f[i, k];
      f[j, k] -= tau[j] * dot;
      for (std::size_t i = j + 1; i < n; ++i) f[i, k] -= tau[j] * dot * f[i, j];
    }
  }
  return qr;
}

// Materialize the orthogonal Q = H₀H₁…H_{n-1} by applying the reflections, in reverse, to I.
template <typename T, typename E>
constexpr auto formQ(const Qr<T, E>& qr) {
  const std::size_t n = qr.rows();
  HeapResult<T, E> q(dims(n, n));
  identity(q);
  const auto& f = qr.factors;
  for (std::size_t jj = n; jj-- > 0;) {
    if (qr.tau[jj] == T{}) continue;
    for (std::size_t k = jj; k < n; ++k) {
      T dot = q[jj, k];
      for (std::size_t i = jj + 1; i < n; ++i) dot += f[i, jj] * q[i, k];
      q[jj, k] -= qr.tau[jj] * dot;
      for (std::size_t i = jj + 1; i < n; ++i) q[i, k] -= qr.tau[jj] * dot * f[i, jj];
    }
  }
  return q;
}

// Materialize R (the upper triangle of the packed workspace; zero below).
template <typename T, typename E>
constexpr auto formR(const Qr<T, E>& qr) {
  const std::size_t n = qr.rows();
  HeapResult<T, E> r(dims(n, n));
  const auto& f = qr.factors;
  for (std::size_t i = 0; i < n; ++i)
    for (std::size_t j = 0; j < n; ++j) r[i, j] = (j >= i) ? f[i, j] : T{};
  return r;
}

// Solve A·x = b via x = R⁻¹·Qᵀ·b. Returns a fresh owned vector (never mutates b).
template <typename T, typename E, VecView B>
constexpr auto solve(const Qr<T, E>& qr, const B& b) {
  auto vb = view(b);
  const std::size_t n = qr.rows();
  assert(static_cast<std::size_t>(vb.extent(0)) == n && "RHS size must match the system");
  assert(!qr.singular && "solve() on a singular QR factorization (zero R pivot)");
  const auto& f = qr.factors;

  HeapResult<T, std::extents<std::size_t, E::static_extent(0)>> x(dims(n));
  for (std::size_t i = 0; i < n; ++i) x[i] = static_cast<T>(vb[i]);

  for (std::size_t j = 0; j < n; ++j) {  // x ← Qᵀ·b, reflections in forward order
    if (qr.tau[j] == T{}) continue;
    T dot = x[j];
    for (std::size_t i = j + 1; i < n; ++i) dot += f[i, j] * x[i];
    x[j] -= qr.tau[j] * dot;
    for (std::size_t i = j + 1; i < n; ++i) x[i] -= qr.tau[j] * dot * f[i, j];
  }
  for (std::size_t i = n; i-- > 0;) {  // back substitution R·x = Qᵀ·b
    for (std::size_t j = i + 1; j < n; ++j) x[i] -= f[i, j] * x[j];
    x[i] /= f[i, i];
  }
  return x;
}

}  // namespace tempura
