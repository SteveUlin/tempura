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

// Symmetric eigendecomposition: A = V·D·Vᵀ for symmetric A, V orthogonal (columns are
// eigenvectors), D diagonal (eigenvalues, ascending). Three phases:
//   1. Householder reduction to tridiagonal form (T = QᵀAQ)
//   2. implicit QL iteration with Wilkinson shift on T (cubic convergence)
//   3. accumulate the Givens rotations into Q to get V, then sort
// QL (vs QR) sweeps from the bottom-right, converging the smallest eigenvalues first.
//
// Design: a free function over a MatrixView returning {values, vectors}. Square,
// and ASSUMED symmetric (only the lower triangle drives the reduction). Floating-point
// only. The iteration is inherently sequential; the workspace is row-major like LU.

template <typename T, typename Extents>
struct SymmetricEigen {
  using value_type = T;
  using extents_type = Extents;
  HeapResult<T, std::extents<std::size_t, Extents::static_extent(0)>> values;  // ascending
  HeapResult<T, Extents> vectors;  // column i is the eigenvector for values[i]
};

template <MatrixView M>
  requires std::floating_point<typename ViewOf<M>::value_type>
constexpr auto symmetricEigen(const M& a) {
  auto va = view(a);
  using E = typename decltype(va)::extents_type;
  using T = typename decltype(va)::value_type;
  static_assert(dimsCompatible(E::static_extent(0), E::static_extent(1)),
                "symmetricEigen needs a square matrix");
  assert(va.extent(0) == va.extent(1) && "symmetricEigen requires a square matrix");
  const std::int64_t n = static_cast<std::int64_t>(va.extent(0));

  HeapResult<T, E> v(va);  // working matrix → eigenvectors
  std::vector<T> d(n);     // diagonal → eigenvalues
  std::vector<T> e(n);     // subdiagonal
  std::vector<T> tau(n);   // Householder coefficients

  // ── Phase 1: Householder reduction to tridiagonal; store reflectors in v's lower tri ──
  {
    std::vector<T> p(n);
    for (std::int64_t j = 0; j < n - 2; ++j) {
      T norm_sq{};
      for (std::int64_t i = j + 1; i < n; ++i) norm_sq += v[i, j] * v[i, j];
      const T norm = std::sqrt(norm_sq);
      if (norm == T{}) {
        tau[j] = T{};
        e[j] = T{};
        continue;
      }
      const T alpha = (v[j + 1, j] >= T{}) ? -norm : norm;
      tau[j] = (alpha - v[j + 1, j]) / alpha;
      const T denom = v[j + 1, j] - alpha;
      for (std::int64_t i = j + 2; i < n; ++i) v[i, j] /= denom;
      v[j + 1, j] = T{1};  // v₀ = 1 stored explicitly here
      e[j] = alpha;

      // Symmetric similarity A ← HAH, H = I − τvvᵀ, on the trailing (n−j−1) block.
      const std::int64_t m = n - j - 1;
      for (std::int64_t i = 0; i < m; ++i) {
        T sum{};
        for (std::int64_t k = 0; k < m; ++k) sum += v[j + 1 + i, j + 1 + k] * v[j + 1 + k, j];
        p[i] = tau[j] * sum;
      }
      T K{};
      for (std::int64_t i = 0; i < m; ++i) K += v[j + 1 + i, j] * p[i];
      K *= tau[j] / T{2};
      for (std::int64_t i = 0; i < m; ++i) p[i] -= K * v[j + 1 + i, j];  // p ← q = p − Kv
      for (std::int64_t i = 0; i < m; ++i)
        for (std::int64_t k = 0; k <= i; ++k) {
          const T upd = v[j + 1 + i, j] * p[k] + p[i] * v[j + 1 + k, j];
          v[j + 1 + i, j + 1 + k] -= upd;
          if (i != k) v[j + 1 + k, j + 1 + i] -= upd;
        }
    }
    for (std::int64_t i = 0; i < n; ++i) d[i] = v[i, i];
    if (n > 1) e[n - 2] = v[n - 1, n - 2];
    if (n > 0) e[n - 1] = T{};
  }

  // ── Phase 2: form Q from the stored reflectors (overwrites v) ──
  {
    HeapResult<T, E> q(static_cast<std::size_t>(n), static_cast<std::size_t>(n));
    identity(q);
    for (std::int64_t j = n - 3; j >= 0; --j) {
      if (tau[j] == T{}) continue;
      for (std::int64_t k = j + 1; k < n; ++k) {
        T dot = q[j + 1, k];
        for (std::int64_t i = j + 2; i < n; ++i) dot += v[i, j] * q[i, k];
        q[j + 1, k] -= tau[j] * dot;
        for (std::int64_t i = j + 2; i < n; ++i) q[i, k] -= tau[j] * dot * v[i, j];
      }
    }
    v = std::move(q);
  }

  // ── Phase 3: implicit QL with Wilkinson shift; accumulate Givens into v ──
  if (n > 1) {
    constexpr int kMaxIter = 50;
    for (std::int64_t l = 0; l < n; ++l) {
      int iter = 0;
      for (;;) {
        std::int64_t m = l;
        for (; m < n - 1; ++m) {
          const T dd = std::abs(d[m]) + std::abs(d[m + 1]);
          if (std::abs(e[m]) <= std::numeric_limits<T>::epsilon() * dd) break;
        }
        if (m == l) break;
        // iter must increment unconditionally: an assert is stripped under NDEBUG, which
        // would also drop `iter++` and spin this for(;;) forever on a non-converging
        // (e.g. inf/nan-seeded) block. Fail loud in debug/constexpr, degrade in release.
        if (++iter > kMaxIter) {
          assert(false && "QL iteration failed to converge");
          break;
        }

        T g = (d[l + 1] - d[l]) / (T{2} * e[l]);
        T r = std::hypot(g, T{1});
        g = d[m] - d[l] + e[l] / (g + (g >= T{} ? r : -r));
        T s{1};
        T c{1};
        T pp{};
        bool underflow = false;
        for (std::int64_t i = m - 1; i >= l; --i) {
          T f = s * e[i];
          const T b = c * e[i];
          r = std::hypot(f, g);
          e[i + 1] = r;
          if (r == T{}) {
            d[i + 1] -= pp;
            e[m] = T{};
            underflow = true;
            break;
          }
          s = f / r;
          c = g / r;
          g = d[i + 1] - pp;
          r = (d[i] - g) * s + T{2} * c * b;
          pp = s * r;
          d[i + 1] = g + pp;
          g = c * r - b;
          for (std::int64_t k = 0; k < n; ++k) {  // Givens into eigenvector columns i, i+1
            f = v[k, i + 1];
            v[k, i + 1] = s * v[k, i] + c * f;
            v[k, i] = c * v[k, i] - s * f;
          }
        }
        if (underflow) continue;
        d[l] -= pp;
        e[l] = g;
        e[m] = T{};
      }
    }
  }

  // ── Phase 4: sort eigenvalues ascending, reorder eigenvector columns ──
  for (std::int64_t i = 0; i < n - 1; ++i) {
    std::int64_t min_idx = i;
    for (std::int64_t j = i + 1; j < n; ++j)
      if (d[j] < d[min_idx]) min_idx = j;
    if (min_idx != i) {
      std::swap(d[i], d[min_idx]);
      for (std::int64_t k = 0; k < n; ++k) {
        using std::swap;
        swap(v[k, i], v[k, min_idx]);
      }
    }
  }

  HeapResult<T, std::extents<std::size_t, E::static_extent(0)>> values(static_cast<std::size_t>(n));
  for (std::int64_t i = 0; i < n; ++i) values[i] = d[i];
  return SymmetricEigen<T, E>{std::move(values), std::move(v)};
}

}  // namespace tempura
