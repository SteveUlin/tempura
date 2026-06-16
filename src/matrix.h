#pragma once

#include <array>
#include <cassert>
#include <cstddef>
#include <mdspan>
#include <span>
#include <type_traits>
#include <utility>
#include <vector>

#include "mdarray.h"

namespace tempura {

// Heap (std::vector) with optionally-static dims: storage and dim-staticness are
// independent axes, so a large matrix can still carry a compile-time-checked shape.
template <typename T, std::size_t Rows = std::dynamic_extent, std::size_t Cols = std::dynamic_extent>
using Matrix = MdArray<T, std::extents<std::size_t, Rows, Cols>, std::layout_right, std::vector<T>>;

template <typename T, std::size_t Rows, std::size_t Cols>
using InlineMatrix = MdArray<T, std::extents<std::size_t, Rows, Cols>, std::layout_right,
                             std::array<T, Rows * Cols>>;

constexpr auto mergeExtent(std::size_t a, std::size_t b) -> std::size_t {
  return a != std::dynamic_extent ? a : b;
}

// A dynamic (unknown) dim defers the check to runtime, so a static_assert on this
// fails only when both dims are known and unequal.
constexpr auto dimsCompatible(std::size_t a, std::size_t b) -> bool {
  return a == std::dynamic_extent || b == std::dynamic_extent || a == b;
}

template <typename ExtA, typename ExtB>
using SumExtents =
    std::extents<std::size_t, mergeExtent(ExtA::static_extent(0), ExtB::static_extent(0)),
                 mergeExtent(ExtA::static_extent(1), ExtB::static_extent(1))>;
template <typename ExtA, typename ExtB>
using ProductExtents = std::extents<std::size_t, ExtA::static_extent(0), ExtB::static_extent(1)>;

// Value forms always own heap storage: result size isn't bounded by operand size
// (an outer product of two small stack matrices can be megabytes), so stack is
// opt-in via the destination form.
template <typename T, typename Ext>
using ResultMatrix = MdArray<T, Ext, std::layout_right, std::vector<T>>;

// Kernels compute over views, not owners, so one definition serves owners,
// sub-blocks, foreign buffers, and transposed/strided views. view() adapts an
// owner to a (writable) mdspan and passes an mdspan through unchanged.
template <typename T, typename E, typename L, typename A>
constexpr auto view(std::mdspan<T, E, L, A> m) {
  return m;
}
template <typename T, typename E, typename L, typename C>
constexpr auto view(MdArray<T, E, L, C>& m) {
  return m.toMdspan();
}
template <typename T, typename E, typename L, typename C>
constexpr auto view(const MdArray<T, E, L, C>& m) {
  return m.toMdspan();
}

template <typename M>
concept Viewable = requires(M& m) { view(m); };
template <typename M>
using ViewExtentsOf = typename decltype(view(std::declval<M&>()))::extents_type;
template <typename M>
concept Matrix2D = Viewable<M> && (ViewExtentsOf<M>::rank() == 2);

// dst ← a + b, element-wise. Aliasing: dst may be exactly a or b (in-place);
// partial/shifted overlap is undefined.
template <Matrix2D A, Matrix2D B, Matrix2D D>
constexpr auto add(const A& a, const B& b, D& dst) -> D& {
  auto va = view(a);
  auto vb = view(b);
  auto vd = view(dst);
  using Ea = typename decltype(va)::extents_type;
  using Eb = typename decltype(vb)::extents_type;
  using Ed = typename decltype(vd)::extents_type;
  using Td = typename decltype(vd)::value_type;
  static_assert(dimsCompatible(Ea::static_extent(0), Eb::static_extent(0)), "row counts differ");
  static_assert(dimsCompatible(Ea::static_extent(1), Eb::static_extent(1)), "column counts differ");
  static_assert(dimsCompatible(Ed::static_extent(0), Ea::static_extent(0)), "dst rows mismatch");
  static_assert(dimsCompatible(Ed::static_extent(1), Ea::static_extent(1)), "dst columns mismatch");
  assert(va.extent(0) == vb.extent(0) && va.extent(1) == vb.extent(1));
  assert(vd.extent(0) == va.extent(0) && vd.extent(1) == va.extent(1));

  const std::size_t rows = va.extent(0);
  const std::size_t cols = va.extent(1);
  for (std::size_t i = 0; i < rows; ++i)
    for (std::size_t j = 0; j < cols; ++j) vd[i, j] = static_cast<Td>(va[i, j] + vb[i, j]);
  return dst;
}

// dst ← a · b, matrix product (not element-wise). PRECONDITION: dst must not
// overlap a or b — each cell sums over a full row of a and column of b, so
// aliasing corrupts the result. Unchecked (std::linalg-style contract).
template <Matrix2D A, Matrix2D B, Matrix2D D>
constexpr auto multiply(const A& a, const B& b, D& dst) -> D& {
  auto va = view(a);
  auto vb = view(b);
  auto vd = view(dst);
  using Ea = typename decltype(va)::extents_type;
  using Eb = typename decltype(vb)::extents_type;
  using Ed = typename decltype(vd)::extents_type;
  using Td = typename decltype(vd)::value_type;
  using Acc = std::common_type_t<typename decltype(va)::value_type, typename decltype(vb)::value_type>;
  static_assert(dimsCompatible(Ea::static_extent(1), Eb::static_extent(0)),
                "inner dimensions must match: A columns == B rows");
  static_assert(dimsCompatible(Ed::static_extent(0), Ea::static_extent(0)), "dst rows mismatch");
  static_assert(dimsCompatible(Ed::static_extent(1), Eb::static_extent(1)), "dst columns mismatch");
  assert(va.extent(1) == vb.extent(0));
  assert(vd.extent(0) == va.extent(0) && vd.extent(1) == vb.extent(1));

  const std::size_t m = va.extent(0);
  const std::size_t k = va.extent(1);
  const std::size_t n = vb.extent(1);
  for (std::size_t i = 0; i < m; ++i)
    for (std::size_t j = 0; j < n; ++j) {
      Acc sum{};
      for (std::size_t p = 0; p < k; ++p) sum += static_cast<Acc>(va[i, p]) * static_cast<Acc>(vb[p, j]);
      vd[i, j] = static_cast<Td>(sum);
    }
  return dst;
}

// dst ← dst + a · b. Same no-overlap precondition as multiply.
template <Matrix2D A, Matrix2D B, Matrix2D D>
constexpr auto multiplyAdd(const A& a, const B& b, D& dst) -> D& {
  auto va = view(a);
  auto vb = view(b);
  auto vd = view(dst);
  using Ea = typename decltype(va)::extents_type;
  using Eb = typename decltype(vb)::extents_type;
  using Ed = typename decltype(vd)::extents_type;
  using Td = typename decltype(vd)::value_type;
  using Acc = std::common_type_t<typename decltype(va)::value_type, typename decltype(vb)::value_type>;
  static_assert(dimsCompatible(Ea::static_extent(1), Eb::static_extent(0)),
                "inner dimensions must match: A columns == B rows");
  static_assert(dimsCompatible(Ed::static_extent(0), Ea::static_extent(0)), "dst rows mismatch");
  static_assert(dimsCompatible(Ed::static_extent(1), Eb::static_extent(1)), "dst columns mismatch");
  assert(va.extent(1) == vb.extent(0));
  assert(vd.extent(0) == va.extent(0) && vd.extent(1) == vb.extent(1));

  const std::size_t m = va.extent(0);
  const std::size_t k = va.extent(1);
  const std::size_t n = vb.extent(1);
  for (std::size_t i = 0; i < m; ++i)
    for (std::size_t j = 0; j < n; ++j) {
      Acc sum{};
      for (std::size_t p = 0; p < k; ++p) sum += static_cast<Acc>(va[i, p]) * static_cast<Acc>(vb[p, j]);
      vd[i, j] = static_cast<Td>(vd[i, j] + sum);
    }
  return dst;
}

template <Matrix2D A, Matrix2D B>
constexpr auto add(const A& a, const B& b) {
  auto va = view(a);
  auto vb = view(b);
  using Ea = typename decltype(va)::extents_type;
  using Eb = typename decltype(vb)::extents_type;
  using Value = std::common_type_t<typename decltype(va)::value_type, typename decltype(vb)::value_type>;
  ResultMatrix<Value, SumExtents<Ea, Eb>> c(va.extent(0), va.extent(1));
  add(va, vb, c);
  return c;
}
template <Matrix2D A, Matrix2D B>
constexpr auto multiply(const A& a, const B& b) {
  auto va = view(a);
  auto vb = view(b);
  using Ea = typename decltype(va)::extents_type;
  using Eb = typename decltype(vb)::extents_type;
  using Value = std::common_type_t<typename decltype(va)::value_type, typename decltype(vb)::value_type>;
  ResultMatrix<Value, ProductExtents<Ea, Eb>> c(va.extent(0), vb.extent(1));
  multiply(va, vb, c);
  return c;
}

template <Matrix2D A, Matrix2D B>
constexpr auto operator+(const A& a, const B& b) {
  return add(a, b);
}
template <Matrix2D A, Matrix2D B>
constexpr auto operator*(const A& a, const B& b) {
  return multiply(a, b);
}

// add is alias-safe, so a is its own destination.
template <Matrix2D A, Matrix2D B>
constexpr auto operator+=(A& a, const B& b) -> A& {
  add(a, b, a);
  return a;
}

// a ← a·b. Matmul can't write into its own input and a·b's type may differ from
// a, so compute then copy back — a's type and storage are preserved.
template <Matrix2D A, Matrix2D B>
constexpr auto operator*=(A& a, const B& b) -> A& {
  auto va = view(a);
  auto vb = view(b);
  using Ea = typename decltype(va)::extents_type;
  using Eb = typename decltype(vb)::extents_type;
  using Ta = typename decltype(va)::value_type;
  static_assert(dimsCompatible(Eb::static_extent(0), Eb::static_extent(1)), "b must be square");
  static_assert(dimsCompatible(Ea::static_extent(1), Eb::static_extent(0)), "b side != a columns");
  const auto product = multiply(va, vb);
  assert(product.extent(0) == va.extent(0) && product.extent(1) == va.extent(1));
  auto vp = view(product);
  const std::size_t rows = va.extent(0);
  const std::size_t cols = va.extent(1);
  for (std::size_t i = 0; i < rows; ++i)
    for (std::size_t j = 0; j < cols; ++j) va[i, j] = static_cast<Ta>(vp[i, j]);
  return a;
}

}  // namespace tempura
