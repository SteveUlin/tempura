#pragma once

#include <array>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <mdspan>
#include <span>
#include <type_traits>
#include <vector>

#include "matrix.h"

namespace tempura {

// Rank-1 analogues of Matrix/InlineMatrix: heap (std::vector) vs stack (std::array),
// both rank-1 Dense owners (see matrix.h).
template <typename T, std::size_t N = std::dynamic_extent>
using Vec = Dense<T, std::extents<std::size_t, N>, std::vector<T>>;

template <typename T, std::size_t N>
using InlineVec = Dense<T, std::extents<std::size_t, N>, std::array<T, N>>;

template <typename M>
concept Vector1D = Viewable<M> && (ViewExtentsOf<M>::rank() == 1);

// y ← A · x, matrix-vector product. PRECONDITION: y must not alias A or x.
template <Matrix2D A, Vector1D X, Vector1D Y>
constexpr auto multiply(const A& a, const X& x, Y& y) -> Y& {
  auto va = view(a);
  auto vx = view(x);
  auto vy = view(y);
  using Ea = typename decltype(va)::extents_type;
  using Ex = typename decltype(vx)::extents_type;
  using Ey = typename decltype(vy)::extents_type;
  using Ty = typename decltype(vy)::value_type;
  using Acc = std::common_type_t<typename decltype(va)::value_type, typename decltype(vx)::value_type>;
  static_assert(dimsCompatible(Ea::static_extent(1), Ex::static_extent(0)), "A columns == x size");
  static_assert(dimsCompatible(Ey::static_extent(0), Ea::static_extent(0)), "y size == A rows");
  assert(va.extent(1) == vx.extent(0));
  assert(vy.extent(0) == va.extent(0));

  const std::size_t m = va.extent(0);
  const std::size_t n = va.extent(1);
  for (std::size_t i = 0; i < m; ++i) {
    Acc sum{};
    for (std::size_t j = 0; j < n; ++j) sum += static_cast<Acc>(va[i, j]) * static_cast<Acc>(vx[j]);
    vy[i] = static_cast<Ty>(sum);
  }
  return y;
}

template <Matrix2D A, Vector1D X>
constexpr auto multiply(const A& a, const X& x) {
  auto va = view(a);
  auto vx = view(x);
  using Ea = typename decltype(va)::extents_type;
  using Value = std::common_type_t<typename decltype(va)::value_type, typename decltype(vx)::value_type>;
  HeapResult<Value, std::extents<std::size_t, Ea::static_extent(0)>> y(va.extent(0));
  multiply(va, vx, y);
  return y;
}

template <Matrix2D A, Vector1D X>
constexpr auto operator*(const A& a, const X& x) {
  return multiply(a, x);
}

template <Vector1D X, Vector1D Y>
constexpr auto dot(const X& x, const Y& y) {
  auto vx = view(x);
  auto vy = view(y);
  using Ex = typename decltype(vx)::extents_type;
  using Ey = typename decltype(vy)::extents_type;
  using Acc = std::common_type_t<typename decltype(vx)::value_type, typename decltype(vy)::value_type>;
  static_assert(dimsCompatible(Ex::static_extent(0), Ey::static_extent(0)), "size mismatch");
  assert(vx.extent(0) == vy.extent(0));
  Acc sum{};
  const std::size_t n = vx.extent(0);
  for (std::size_t i = 0; i < n; ++i) sum += static_cast<Acc>(vx[i]) * static_cast<Acc>(vy[i]);
  return sum;
}

// Euclidean (2-)norm √Σxᵢ² — the "2" is the norm order, not a squaring.
template <Vector1D X>
constexpr auto norm2(const X& x) {
  auto vx = view(x);
  using V = typename decltype(vx)::value_type;
  V sum{};
  const std::size_t n = vx.extent(0);
  for (std::size_t i = 0; i < n; ++i) sum += vx[i] * vx[i];
  return std::sqrt(sum);
}

}  // namespace tempura
