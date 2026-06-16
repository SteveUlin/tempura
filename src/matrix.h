#pragma once

#include <array>
#include <cassert>
#include <cstddef>
#include <mdspan>
#include <span>
#include <type_traits>
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
// opt-in via the destination form. See docs/2026-06-15-matrix-design.md.
template <typename T, typename Ext>
using ResultMatrix = MdArray<T, Ext, std::layout_right, std::vector<T>>;

// Owning matrices alias only by being the same object, so address identity is a
// sound (and constexpr) aliasing test.
constexpr auto sameStorage(const auto& a, const auto& b) -> bool {
  return static_cast<const void*>(&a) == static_cast<const void*>(&b);
}

// The destination forms (write into dst) are the primitives; value forms and
// operators wrap them. dst is where the caller picks stack vs heap.

// Alias-safe: dst may be a or b — operator+= relies on this.
template <typename Ta, typename Ea, typename La, typename Ca, typename Tb, typename Eb,
          typename Lb, typename Cb, typename Td, typename Ed, typename Ld, typename Cd>
  requires(Ea::rank() == 2 && Eb::rank() == 2 && Ed::rank() == 2)
constexpr auto add(const MdArray<Ta, Ea, La, Ca>& a, const MdArray<Tb, Eb, Lb, Cb>& b,
                   MdArray<Td, Ed, Ld, Cd>& dst) -> MdArray<Td, Ed, Ld, Cd>& {
  static_assert(dimsCompatible(Ea::static_extent(0), Eb::static_extent(0)), "row counts differ");
  static_assert(dimsCompatible(Ea::static_extent(1), Eb::static_extent(1)), "column counts differ");
  static_assert(dimsCompatible(Ed::static_extent(0), Ea::static_extent(0)), "dst rows mismatch");
  static_assert(dimsCompatible(Ed::static_extent(1), Ea::static_extent(1)), "dst columns mismatch");
  assert(a.extent(0) == b.extent(0) && a.extent(1) == b.extent(1));
  assert(dst.extent(0) == a.extent(0) && dst.extent(1) == a.extent(1));

  const std::size_t rows = a.extent(0);
  const std::size_t cols = a.extent(1);
  for (std::size_t i = 0; i < rows; ++i)
    for (std::size_t j = 0; j < cols; ++j) dst[i, j] = static_cast<Td>(a[i, j] + b[i, j]);
  return dst;
}

// Matrix product, not element-wise. PRECONDITION: dst must not alias a or b —
// each cell sums over a full row of a and column of b, so aliasing corrupts it.
template <typename Ta, typename Ea, typename La, typename Ca, typename Tb, typename Eb,
          typename Lb, typename Cb, typename Td, typename Ed, typename Ld, typename Cd>
  requires(Ea::rank() == 2 && Eb::rank() == 2 && Ed::rank() == 2)
constexpr auto multiply(const MdArray<Ta, Ea, La, Ca>& a, const MdArray<Tb, Eb, Lb, Cb>& b,
                        MdArray<Td, Ed, Ld, Cd>& dst) -> MdArray<Td, Ed, Ld, Cd>& {
  static_assert(dimsCompatible(Ea::static_extent(1), Eb::static_extent(0)),
                "inner dimensions must match: A columns == B rows");
  static_assert(dimsCompatible(Ed::static_extent(0), Ea::static_extent(0)), "dst rows mismatch");
  static_assert(dimsCompatible(Ed::static_extent(1), Eb::static_extent(1)), "dst columns mismatch");
  assert(a.extent(1) == b.extent(0));
  assert(dst.extent(0) == a.extent(0) && dst.extent(1) == b.extent(1));
  assert(!sameStorage(dst, a) && !sameStorage(dst, b));

  using Acc = std::common_type_t<Ta, Tb>;
  const std::size_t m = a.extent(0);
  const std::size_t k = a.extent(1);
  const std::size_t n = b.extent(1);
  for (std::size_t i = 0; i < m; ++i)
    for (std::size_t j = 0; j < n; ++j) {
      Acc sum{};
      for (std::size_t p = 0; p < k; ++p) sum += static_cast<Acc>(a[i, p]) * static_cast<Acc>(b[p, j]);
      dst[i, j] = static_cast<Td>(sum);
    }
  return dst;
}

// dst += a·b. Same no-alias precondition as multiply.
template <typename Ta, typename Ea, typename La, typename Ca, typename Tb, typename Eb,
          typename Lb, typename Cb, typename Td, typename Ed, typename Ld, typename Cd>
  requires(Ea::rank() == 2 && Eb::rank() == 2 && Ed::rank() == 2)
constexpr auto multiplyAdd(const MdArray<Ta, Ea, La, Ca>& a, const MdArray<Tb, Eb, Lb, Cb>& b,
                           MdArray<Td, Ed, Ld, Cd>& dst) -> MdArray<Td, Ed, Ld, Cd>& {
  static_assert(dimsCompatible(Ea::static_extent(1), Eb::static_extent(0)),
                "inner dimensions must match: A columns == B rows");
  static_assert(dimsCompatible(Ed::static_extent(0), Ea::static_extent(0)), "dst rows mismatch");
  static_assert(dimsCompatible(Ed::static_extent(1), Eb::static_extent(1)), "dst columns mismatch");
  assert(a.extent(1) == b.extent(0));
  assert(dst.extent(0) == a.extent(0) && dst.extent(1) == b.extent(1));
  assert(!sameStorage(dst, a) && !sameStorage(dst, b));

  using Acc = std::common_type_t<Ta, Tb>;
  const std::size_t m = a.extent(0);
  const std::size_t k = a.extent(1);
  const std::size_t n = b.extent(1);
  for (std::size_t i = 0; i < m; ++i)
    for (std::size_t j = 0; j < n; ++j) {
      Acc sum{};
      for (std::size_t p = 0; p < k; ++p) sum += static_cast<Acc>(a[i, p]) * static_cast<Acc>(b[p, j]);
      dst[i, j] = static_cast<Td>(dst[i, j] + sum);
    }
  return dst;
}

template <typename Ta, typename Ea, typename La, typename Ca, typename Tb, typename Eb,
          typename Lb, typename Cb>
  requires(Ea::rank() == 2 && Eb::rank() == 2)
constexpr auto add(const MdArray<Ta, Ea, La, Ca>& a, const MdArray<Tb, Eb, Lb, Cb>& b) {
  ResultMatrix<std::common_type_t<Ta, Tb>, SumExtents<Ea, Eb>> c(a.extent(0), a.extent(1));
  add(a, b, c);
  return c;
}
template <typename Ta, typename Ea, typename La, typename Ca, typename Tb, typename Eb,
          typename Lb, typename Cb>
  requires(Ea::rank() == 2 && Eb::rank() == 2)
constexpr auto multiply(const MdArray<Ta, Ea, La, Ca>& a, const MdArray<Tb, Eb, Lb, Cb>& b) {
  ResultMatrix<std::common_type_t<Ta, Tb>, ProductExtents<Ea, Eb>> c(a.extent(0), b.extent(1));
  multiply(a, b, c);
  return c;
}

template <typename Ta, typename Ea, typename La, typename Ca, typename Tb, typename Eb,
          typename Lb, typename Cb>
  requires(Ea::rank() == 2 && Eb::rank() == 2)
constexpr auto operator+(const MdArray<Ta, Ea, La, Ca>& a, const MdArray<Tb, Eb, Lb, Cb>& b) {
  return add(a, b);
}
template <typename Ta, typename Ea, typename La, typename Ca, typename Tb, typename Eb,
          typename Lb, typename Cb>
  requires(Ea::rank() == 2 && Eb::rank() == 2)
constexpr auto operator*(const MdArray<Ta, Ea, La, Ca>& a, const MdArray<Tb, Eb, Lb, Cb>& b) {
  return multiply(a, b);
}

// add is alias-safe, so a is its own destination.
template <typename Ta, typename Ea, typename La, typename Ca, typename Tb, typename Eb,
          typename Lb, typename Cb>
  requires(Ea::rank() == 2 && Eb::rank() == 2)
constexpr auto operator+=(MdArray<Ta, Ea, La, Ca>& a, const MdArray<Tb, Eb, Lb, Cb>& b)
    -> MdArray<Ta, Ea, La, Ca>& {
  return add(a, b, a);
}

// a ← a·b. Matmul can't write into its own input and a·b's type may differ from
// a, so compute then copy back — a's type and storage are preserved.
template <typename Ta, typename Ea, typename La, typename Ca, typename Tb, typename Eb,
          typename Lb, typename Cb>
  requires(Ea::rank() == 2 && Eb::rank() == 2)
constexpr auto operator*=(MdArray<Ta, Ea, La, Ca>& a, const MdArray<Tb, Eb, Lb, Cb>& b)
    -> MdArray<Ta, Ea, La, Ca>& {
  static_assert(dimsCompatible(Eb::static_extent(0), Eb::static_extent(1)), "b must be square");
  static_assert(dimsCompatible(Ea::static_extent(1), Eb::static_extent(0)), "b side != a columns");
  const auto product = multiply(a, b);
  assert(product.extent(0) == a.extent(0) && product.extent(1) == a.extent(1));
  const std::size_t rows = a.extent(0);
  const std::size_t cols = a.extent(1);
  for (std::size_t i = 0; i < rows; ++i)
    for (std::size_t j = 0; j < cols; ++j) a[i, j] = static_cast<Ta>(product[i, j]);
  return a;
}

}  // namespace tempura
