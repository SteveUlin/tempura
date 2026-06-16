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

// Heap-backed matrix, storage is std::vector. Dimensions default to dynamic
// (resolved at runtime); pass static Rows/Cols to bake the shape into the type
// for compile-time shape checks while keeping the data on the heap — the right
// choice for a large matrix whose size is nonetheless known.
template <typename T, std::size_t Rows = std::dynamic_extent, std::size_t Cols = std::dynamic_extent>
using Matrix = MdArray<T, std::extents<std::size_t, Rows, Cols>, std::layout_right, std::vector<T>>;

// Stack-backed matrix: static extents held inline in a std::array. For small,
// fixed sizes where avoiding heap allocation matters — a large size overflows
// the stack, so reach for a static-shape Matrix instead.
template <typename T, std::size_t Rows, std::size_t Cols>
using InlineMatrix = MdArray<T, std::extents<std::size_t, Rows, Cols>, std::layout_right,
                             std::array<T, Rows * Cols>>;

// ── result-type plumbing ─────────────────────────────────────────────────────
// dynamic_extent is the "size unknown until runtime" sentinel; keep whichever
// operand pins a dimension so a static shape survives mixing with a dynamic one.
constexpr auto mergeExtent(std::size_t a, std::size_t b) -> std::size_t {
  return a != std::dynamic_extent ? a : b;
}

// A+B keeps static extents from either side; A*B takes A's rows and B's cols.
template <typename ExtA, typename ExtB>
using SumExtents =
    std::extents<std::size_t, mergeExtent(ExtA::static_extent(0), ExtB::static_extent(0)),
                 mergeExtent(ExtA::static_extent(1), ExtB::static_extent(1))>;
template <typename ExtA, typename ExtB>
using ProductExtents = std::extents<std::size_t, ExtA::static_extent(0), ExtB::static_extent(1)>;

// Value forms always own heap storage. Result size is not bounded by operand
// size — an outer product of two small stack matrices can be megabytes — so
// stack storage cannot be safely inferred from the operands. Static extents are
// kept (for compile-time shape checks); storage is std::vector. Stack output is
// opt-in via the destination form: multiply(A, B, dst) into an InlineMatrix.
template <typename T, typename Ext>
using ResultMatrix = MdArray<T, Ext, std::layout_right, std::vector<T>>;

// ── operations ───────────────────────────────────────────────────────────────
// Element-wise sum. The result is static in any dimension either operand pins,
// so add(InlineMatrix, Matrix) recovers a stack-allocated InlineMatrix.
template <typename T, typename ExtA, typename LA, typename CA, typename U, typename ExtB,
          typename LB, typename CB>
  requires(ExtA::rank() == 2 && ExtB::rank() == 2)
constexpr auto add(const MdArray<T, ExtA, LA, CA>& a, const MdArray<U, ExtB, LB, CB>& b) {
  using Value = std::common_type_t<T, U>;
  if constexpr (ExtA::static_extent(0) != std::dynamic_extent &&
                ExtB::static_extent(0) != std::dynamic_extent) {
    static_assert(ExtA::static_extent(0) == ExtB::static_extent(0), "row counts differ");
  }
  if constexpr (ExtA::static_extent(1) != std::dynamic_extent &&
                ExtB::static_extent(1) != std::dynamic_extent) {
    static_assert(ExtA::static_extent(1) == ExtB::static_extent(1), "column counts differ");
  }
  assert(a.extent(0) == b.extent(0) && a.extent(1) == b.extent(1));

  const std::size_t rows = a.extent(0);
  const std::size_t cols = a.extent(1);
  ResultMatrix<Value, SumExtents<ExtA, ExtB>> c(rows, cols);
  for (std::size_t i = 0; i < rows; ++i)
    for (std::size_t j = 0; j < cols; ++j)
      c[i, j] = static_cast<Value>(a[i, j]) + static_cast<Value>(b[i, j]);
  return c;
}

// Matrix product (linear-algebra product, not the element-wise Hadamard one):
// (m×k)·(k×n) = (m×n) via the textbook triple loop, no blocking or SIMD.
template <typename T, typename ExtA, typename LA, typename CA, typename U, typename ExtB,
          typename LB, typename CB>
  requires(ExtA::rank() == 2 && ExtB::rank() == 2)
constexpr auto multiply(const MdArray<T, ExtA, LA, CA>& a, const MdArray<U, ExtB, LB, CB>& b) {
  using Value = std::common_type_t<T, U>;
  if constexpr (ExtA::static_extent(1) != std::dynamic_extent &&
                ExtB::static_extent(0) != std::dynamic_extent) {
    static_assert(ExtA::static_extent(1) == ExtB::static_extent(0),
                  "inner dimensions must match: A columns == B rows");
  }
  assert(a.extent(1) == b.extent(0));

  const std::size_t m = a.extent(0);
  const std::size_t k = a.extent(1);
  const std::size_t n = b.extent(1);
  ResultMatrix<Value, ProductExtents<ExtA, ExtB>> c(m, n);
  for (std::size_t i = 0; i < m; ++i)
    for (std::size_t j = 0; j < n; ++j) {
      Value sum{};
      for (std::size_t p = 0; p < k; ++p) sum += static_cast<Value>(a[i, p]) * static_cast<Value>(b[p, j]);
      c[i, j] = sum;
    }
  return c;
}

// Operators are pure spellings of the named functions; `*` is the matrix product.
template <typename T, typename ExtA, typename LA, typename CA, typename U, typename ExtB,
          typename LB, typename CB>
  requires(ExtA::rank() == 2 && ExtB::rank() == 2)
constexpr auto operator+(const MdArray<T, ExtA, LA, CA>& a, const MdArray<U, ExtB, LB, CB>& b) {
  return add(a, b);
}
template <typename T, typename ExtA, typename LA, typename CA, typename U, typename ExtB,
          typename LB, typename CB>
  requires(ExtA::rank() == 2 && ExtB::rank() == 2)
constexpr auto operator*(const MdArray<T, ExtA, LA, CA>& a, const MdArray<U, ExtB, LB, CB>& b) {
  return multiply(a, b);
}

}  // namespace tempura
