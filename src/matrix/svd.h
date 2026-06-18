#pragma once

#include <cassert>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <utility>
#include <vector>

#include "matrix.h"
#include "vec.h"

namespace tempura {

// Singular value decomposition (Golub-Kahan): A = U·Σ·Vᵀ, U and V orthogonal, Σ
// non-negative diagonal (singular values, descending). Three phases:
//   1. Golub-Kahan bidiagonalization (alternating left/right Householder): A = U₁·B·V₁ᵀ
//   2. implicit QR iteration on the bidiagonal B with Wilkinson shift
//   3. accumulate the Givens rotations into U₁, V₁; make σ ≥ 0; sort descending
// Iterating B directly (not the eigendecomposition of AᵀA) avoids squaring the condition
// number — the reason SVD is the stable tool for rank and least-squares.
//
// Design: a free function over a MatrixView returning {u, v, singular_values}.
// Square only (asserted), floating-point only. Reflectors are columns/rows of the packed
// workspace — strided in the row-major owner; correct as-is (layout decision).

template <typename T, typename Extents>
struct Svd {
  using value_type = T;
  using extents_type = Extents;
  HeapResult<T, Extents> u;  // left singular vectors (columns)
  HeapResult<T, Extents> v;  // right singular vectors (columns)
  HeapResult<T, std::extents<std::size_t, Extents::static_extent(0)>> singular_values;  // ≥0, descending
};

template <MatrixView M>
  requires std::floating_point<typename ViewOf<M>::value_type>
constexpr auto svd(const M& a) {
  auto va = view(a);
  using E = typename decltype(va)::extents_type;
  using T = typename decltype(va)::value_type;
  static_assert(dimsCompatible(E::static_extent(0), E::static_extent(1)), "SVD needs a square matrix");
  assert(va.extent(0) == va.extent(1) && "SVD requires a square matrix");
  const std::int64_t n = static_cast<std::int64_t>(va.extent(0));
  const T eps = std::numeric_limits<T>::epsilon();

  HeapResult<T, E> u(va);  // packed workspace → left vectors
  HeapResult<T, E> v(static_cast<std::size_t>(n), static_cast<std::size_t>(n));
  std::vector<T> diag(n);
  std::vector<T> off(n);
  std::vector<T> tau_l(n);
  std::vector<T> tau_r(n);

  // ── Phase 1: bidiagonalize via alternating Householder reflections ──
  for (std::int64_t j = 0; j < n; ++j) {
    {  // left reflection: zero u[j+1:, j]
      T norm_sq{};
      for (std::int64_t i = j; i < n; ++i) norm_sq += u[i, j] * u[i, j];
      const T norm = std::sqrt(norm_sq);
      if (norm != T{}) {
        const T alpha = (u[j, j] >= T{}) ? -norm : norm;
        tau_l[j] = (alpha - u[j, j]) / alpha;
        const T denom = u[j, j] - alpha;
        for (std::int64_t i = j + 1; i < n; ++i) u[i, j] /= denom;
        u[j, j] = alpha;
        for (std::int64_t k = j + 1; k < n; ++k) {
          T dot = u[j, k];
          for (std::int64_t i = j + 1; i < n; ++i) dot += u[i, j] * u[i, k];
          u[j, k] -= tau_l[j] * dot;
          for (std::int64_t i = j + 1; i < n; ++i) u[i, k] -= tau_l[j] * dot * u[i, j];
        }
      } else {
        tau_l[j] = T{};
      }
    }
    if (j < n - 2) {  // right reflection: zero u[j, j+2:]
      T norm_sq{};
      for (std::int64_t k = j + 1; k < n; ++k) norm_sq += u[j, k] * u[j, k];
      const T norm = std::sqrt(norm_sq);
      if (norm != T{}) {
        const T alpha = (u[j, j + 1] >= T{}) ? -norm : norm;
        tau_r[j] = (alpha - u[j, j + 1]) / alpha;
        const T denom = u[j, j + 1] - alpha;
        for (std::int64_t k = j + 2; k < n; ++k) u[j, k] /= denom;
        u[j, j + 1] = alpha;
        for (std::int64_t i = j + 1; i < n; ++i) {
          T dot = u[i, j + 1];
          for (std::int64_t k = j + 2; k < n; ++k) dot += u[i, k] * u[j, k];
          u[i, j + 1] -= tau_r[j] * dot;
          for (std::int64_t k = j + 2; k < n; ++k) u[i, k] -= tau_r[j] * dot * u[j, k];
        }
      } else {
        tau_r[j] = T{};
      }
    }
  }
  for (std::int64_t i = 0; i < n; ++i) diag[i] = u[i, i];
  for (std::int64_t i = 0; i < n - 1; ++i) off[i] = u[i, i + 1];
  if (n > 0) off[n - 1] = T{};

  // ── Phase 2: form V from the right reflectors (must precede formU, which overwrites u) ──
  {
    identity(v);
    for (std::int64_t j = n - 3; j >= 0; --j) {
      if (tau_r[j] == T{}) continue;
      for (std::int64_t k = j + 1; k < n; ++k) {
        T dot = v[j + 1, k];
        for (std::int64_t i = j + 2; i < n; ++i) dot += u[j, i] * v[i, k];
        v[j + 1, k] -= tau_r[j] * dot;
        for (std::int64_t i = j + 2; i < n; ++i) v[i, k] -= tau_r[j] * dot * u[j, i];
      }
    }
  }

  // ── Phase 3: form U from the left reflectors (overwrites u) ──
  {
    HeapResult<T, E> uu(static_cast<std::size_t>(n), static_cast<std::size_t>(n));
    identity(uu);
    for (std::int64_t j = n - 1; j >= 0; --j) {
      if (tau_l[j] == T{}) continue;
      for (std::int64_t k = j; k < n; ++k) {
        T dot = uu[j, k];
        for (std::int64_t i = j + 1; i < n; ++i) dot += u[i, j] * uu[i, k];
        uu[j, k] -= tau_l[j] * dot;
        for (std::int64_t i = j + 1; i < n; ++i) uu[i, k] -= tau_l[j] * dot * u[i, j];
      }
    }
    u = std::move(uu);
  }

  // ── Phase 4: implicit QR on the bidiagonal, accumulating Givens into U and V ──
  if (n > 1) {
    constexpr int kMaxIter = 100;
    bool converged = false;
    for (std::int64_t total = 0; total < kMaxIter * n; ++total) {
      std::int64_t q = n - 1;  // deflate converged tail
      while (q > 0) {
        const T dd = std::abs(diag[q - 1]) + std::abs(diag[q]);
        if (std::abs(off[q - 1]) > eps * dd) break;
        off[q - 1] = T{};
        --q;
      }
      if (q == 0) {  // fully converged
        converged = true;
        break;
      }
      std::int64_t p = q - 1;  // find top of the unreduced block
      while (p > 0) {
        const T dd = std::abs(diag[p - 1]) + std::abs(diag[p]);
        if (std::abs(off[p - 1]) <= eps * dd) {
          off[p - 1] = T{};
          break;
        }
        --p;
      }

      // Wilkinson shift μ from the trailing 2×2 of BᵀB.
      const T d_qm1 = diag[q - 1];
      const T e_qm1 = off[q - 1];
      const T d_q = diag[q];
      const T t_nn = d_q * d_q + e_qm1 * e_qm1;
      const T t_nm = d_qm1 * e_qm1;
      T t_mm = d_qm1 * d_qm1;
      if (q - 2 >= p) t_mm += off[q - 2] * off[q - 2];
      const T delta = (t_mm - t_nn) / T{2};
      const T sign_d = (delta >= T{}) ? T{1} : T{-1};
      const T denom = delta + sign_d * std::hypot(delta, t_nm);
      const T mu = (denom == T{}) ? T{} : t_nn - t_nm * t_nm / denom;

      T y = diag[p] * diag[p] - mu;
      T z = diag[p] * off[p];
      for (std::int64_t k = p; k < q; ++k) {
        T r = std::hypot(y, z);  // right Givens (mix columns k, k+1)
        T c_r = (r == T{}) ? T{1} : y / r;
        T s_r = (r == T{}) ? T{} : z / r;
        if (k > p) off[k - 1] = r;
        T f = c_r * diag[k] + s_r * off[k];
        off[k] = -s_r * diag[k] + c_r * off[k];
        T g = s_r * diag[k + 1];
        diag[k + 1] *= c_r;
        for (std::int64_t i = 0; i < n; ++i) {
          const T t = c_r * v[i, k] + s_r * v[i, k + 1];
          v[i, k + 1] = -s_r * v[i, k] + c_r * v[i, k + 1];
          v[i, k] = t;
        }
        r = std::hypot(f, g);  // left Givens (zero the bulge)
        const T c_l = (r == T{}) ? T{1} : f / r;
        const T s_l = (r == T{}) ? T{} : g / r;
        diag[k] = r;
        f = c_l * off[k] + s_l * diag[k + 1];
        diag[k + 1] = -s_l * off[k] + c_l * diag[k + 1];
        off[k] = f;
        if (k + 1 < q) {
          g = s_l * off[k + 1];
          off[k + 1] *= c_l;
          y = off[k];
          z = g;
        }
        for (std::int64_t i = 0; i < n; ++i) {
          const T t = c_l * u[i, k] + s_l * u[i, k + 1];
          u[i, k + 1] = -s_l * u[i, k] + c_l * u[i, k + 1];
          u[i, k] = t;
        }
      }
    }
    // A leftover off-diagonal means the iteration budget was exhausted — fail loudly
    // rather than return a wrong (un-converged) decomposition.
    assert(converged && "SVD iteration failed to converge");
  }

  // ── Phase 5: make singular values non-negative (flip U columns), then sort descending ──
  for (std::int64_t i = 0; i < n; ++i)
    if (diag[i] < T{}) {
      diag[i] = -diag[i];
      for (std::int64_t k = 0; k < n; ++k) u[k, i] = -u[k, i];
    }
  for (std::int64_t i = 0; i < n - 1; ++i) {
    std::int64_t max_idx = i;
    for (std::int64_t j = i + 1; j < n; ++j)
      if (diag[j] > diag[max_idx]) max_idx = j;
    if (max_idx != i) {
      std::swap(diag[i], diag[max_idx]);
      for (std::int64_t k = 0; k < n; ++k) {
        using std::swap;
        swap(u[k, i], u[k, max_idx]);
        swap(v[k, i], v[k, max_idx]);
      }
    }
  }

  HeapResult<T, std::extents<std::size_t, E::static_extent(0)>> sv(static_cast<std::size_t>(n));
  for (std::int64_t i = 0; i < n; ++i) sv[i] = diag[i];
  return Svd<T, E>{std::move(u), std::move(v), std::move(sv)};
}

}  // namespace tempura
