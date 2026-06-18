#pragma once

#include <array>
#include <cassert>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <mdspan>
#include <type_traits>
#include <vector>

#include "matrix.h"
#include "mdarray.h"

namespace tempura {

// Rank-1 analogues of Dense/InlineDense: heap (std::vector) vs stack (std::array),
// both rank-1 MdArray owners (see mdarray.h). No defaulted length — use `dyn` for a
// runtime axis: `Vec<double, dyn>`.
template <typename T, std::size_t N>
using Vec = MdArray<T, std::extents<std::size_t, N>, std::layout_right, std::vector<T>>;

template <typename T, std::size_t N>
using InlineVec = MdArray<T, std::extents<std::size_t, N>, std::layout_right, std::array<T, N>>;

// A kernel operand viewable as a rank-1 mdspan — the vector kernels' constraint.
template <typename M>
concept VecView = ViewableRank<M, 1>;

// y ← A · x, matrix-vector product. PRECONDITION: y must not alias A or x — asserted
// (each yᵢ sums over a full row of A, so sharing storage with the output corrupts it).
template <MatrixView A, VecView X, VecView Y>
constexpr auto multiply(const A& a, const X& x, Y& y) -> Y& {
  auto va = view(a);
  auto vx = view(x);
  auto vy = view(y);
  using Ea = typename decltype(va)::extents_type;
  using Ex = typename decltype(vx)::extents_type;
  using Ey = typename decltype(vy)::extents_type;
  using Ty = typename decltype(vy)::value_type;
  using Acc = Accumulator<typename decltype(va)::value_type, typename decltype(vx)::value_type, Ty>;
  static_assert(dimsCompatible(Ea::static_extent(1), Ex::static_extent(0)), "A columns == x size");
  static_assert(dimsCompatible(Ey::static_extent(0), Ea::static_extent(0)), "y size == A rows");
  assert(va.extent(1) == vx.extent(0));
  assert(vy.extent(0) == va.extent(0));
  assert(!sharesStorage(vy.data_handle(), va.data_handle()) &&
         !sharesStorage(vy.data_handle(), vx.data_handle()));

  const std::size_t m = va.extent(0);
  const std::size_t n = va.extent(1);
  for (std::size_t i = 0; i < m; ++i) {
    Acc sum{};
    for (std::size_t j = 0; j < n; ++j) sum += static_cast<Acc>(va[i, j]) * static_cast<Acc>(vx[j]);
    vy[i] = static_cast<Ty>(sum);
  }
  return y;
}

// y ← y + A · x (the "updating" matvec — the inner step of CG / power iteration).
// Same no-alias precondition as multiply, asserted.
template <MatrixView A, VecView X, VecView Y>
constexpr auto multiplyAdd(const A& a, const X& x, Y& y) -> Y& {
  auto va = view(a);
  auto vx = view(x);
  auto vy = view(y);
  using Ea = typename decltype(va)::extents_type;
  using Ex = typename decltype(vx)::extents_type;
  using Ey = typename decltype(vy)::extents_type;
  using Ty = typename decltype(vy)::value_type;
  using Acc = Accumulator<typename decltype(va)::value_type, typename decltype(vx)::value_type, Ty>;
  static_assert(dimsCompatible(Ea::static_extent(1), Ex::static_extent(0)), "A columns == x size");
  static_assert(dimsCompatible(Ey::static_extent(0), Ea::static_extent(0)), "y size == A rows");
  assert(va.extent(1) == vx.extent(0));
  assert(vy.extent(0) == va.extent(0));
  assert(!sharesStorage(vy.data_handle(), va.data_handle()) &&
         !sharesStorage(vy.data_handle(), vx.data_handle()));

  const std::size_t m = va.extent(0);
  const std::size_t n = va.extent(1);
  for (std::size_t i = 0; i < m; ++i) {
    Acc sum{};
    for (std::size_t j = 0; j < n; ++j) sum += static_cast<Acc>(va[i, j]) * static_cast<Acc>(vx[j]);
    vy[i] = static_cast<Ty>(vy[i] + sum);
  }
  return y;
}

template <MatrixView A, VecView X>
constexpr auto multiply(const A& a, const X& x) {
  auto va = view(a);
  auto vx = view(x);
  using Ea = typename decltype(va)::extents_type;
  using Value = std::common_type_t<typename decltype(va)::value_type, typename decltype(vx)::value_type>;
  HeapResult<Value, std::extents<std::size_t, Ea::static_extent(0)>> y(va.extent(0));
  multiply(va, vx, y);
  return y;
}

template <MatrixView A, VecView X>
constexpr auto operator*(const A& a, const X& x) {
  return multiply(a, x);
}

template <VecView X, VecView Y>
constexpr auto dot(const X& x, const Y& y) {
  auto vx = view(x);
  auto vy = view(y);
  using Ex = typename decltype(vx)::extents_type;
  using Ey = typename decltype(vy)::extents_type;
  using Value = std::common_type_t<typename decltype(vx)::value_type, typename decltype(vy)::value_type>;
  using Acc = Accumulator<typename decltype(vx)::value_type, typename decltype(vy)::value_type>;
  static_assert(dimsCompatible(Ex::static_extent(0), Ey::static_extent(0)), "size mismatch");
  assert(vx.extent(0) == vy.extent(0));
  Acc sum{};
  const std::size_t n = vx.extent(0);
  for (std::size_t i = 0; i < n; ++i) sum += static_cast<Acc>(vx[i]) * static_cast<Acc>(vy[i]);
  return static_cast<Value>(sum);  // accumulate wide, return the operand precision (Acc is internal)
}

// Euclidean (2-)norm √Σxᵢ² — the "2" is the norm order, not a squaring. Scaled by
// the largest magnitude (the LAPACK dnrm2 trick) so a naive Σxᵢ² can't overflow to
// ∞ (or underflow to 0) before the sqrt when elements are very large or very small.
// Real-valued by definition: requires a floating-point element (an integer Euclidean
// norm is a category error — the ratio xᵢ/scale would be integer-divided to 0).
template <VecView X>
  requires std::floating_point<typename ViewOf<X>::value_type>
constexpr auto norm2(const X& x) {
  auto vx = view(x);
  using V = typename decltype(vx)::value_type;
  using Acc = Accumulator<V>;  // sum the n ratios² in the wider type (float → double)
  const std::size_t n = vx.extent(0);
  V scale{};
  for (std::size_t i = 0; i < n; ++i) {
    using std::isnan;
    const V a = vx[i] < V{} ? -vx[i] : vx[i];
    if (isnan(a) || a > scale) scale = a;  // NaN is sticky: it bypasses the zero early-out → returns NaN
  }
  if (scale == V{}) return V{};  // all-zero (and empty) vector
  using std::isinf;
  if (isinf(scale)) return scale;  // ±inf element ⇒ +inf (scale ≥ 0); a NaN made scale NaN above, so NaN still falls through
  Acc sum{};
  for (std::size_t i = 0; i < n; ++i) {
    const Acc r = static_cast<Acc>(vx[i]) / static_cast<Acc>(scale);
    sum += r * r;
  }
  return static_cast<V>(static_cast<Acc>(scale) * std::sqrt(sum));
}

}  // namespace tempura
