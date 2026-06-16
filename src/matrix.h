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

// Two static extents conform; an unknown (dynamic) one defers the check to a
// runtime assert. A single static_assert(dimsCompatible(..)) thus fails only when
// both dims are known and unequal — replacing a verbose `if constexpr` guard.
constexpr auto dimsCompatible(std::size_t a, std::size_t b) -> bool {
  return a == std::dynamic_extent || b == std::dynamic_extent || a == b;
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

// Owning matrices share storage only by being the same object, so address
// identity is a sound, constexpr-evaluable aliasing test.
constexpr auto sameStorage(const auto& a, const auto& b) -> bool {
  return static_cast<const void*>(&a) == static_cast<const void*>(&b);
}

// ── destination kernels (the primitives) ─────────────────────────────────────
// Everything else is a thin wrapper over these. They write into a caller-owned
// destination and allocate nothing — the only place the stack-vs-heap choice is
// made, by the caller's choice of dst type.

// dst ← A + B, element-wise. Alias-safe: cell (i,j) reads only (i,j), so dst may
// be A or B (that is exactly what operator+= relies on).
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

// dst ← A · B, the linear-algebra product (not the element-wise Hadamard one):
// (m×k)·(k×n) = (m×n) via the textbook triple loop, no blocking or SIMD.
// PRECONDITION: dst aliases neither operand — each dst cell accumulates over a
// whole row of A and column of B, so overwriting an input mid-product corrupts
// it. Forbidden and asserted, never silently worked around.
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

// dst ← A · B + dst, the "updating" (gemm β=1) form. Same aliasing contract.
// Split from multiply by name, not a runtime flag, so intent is explicit.
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

// ── value forms ──────────────────────────────────────────────────────────────
// Allocate the heap result (merged static extents kept for checks) and delegate
// to the kernel. One implementation of the math; this just owns the buffer.
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

// ── operator spellings ───────────────────────────────────────────────────────
// `*` is the matrix product; `+` is element-wise. Value operators alias the
// value functions; compound operators mutate the LHS.
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

// a += b mutates a in place — alias-safe element-wise, so dst is a itself.
template <typename Ta, typename Ea, typename La, typename Ca, typename Tb, typename Eb,
          typename Lb, typename Cb>
  requires(Ea::rank() == 2 && Eb::rank() == 2)
constexpr auto operator+=(MdArray<Ta, Ea, La, Ca>& a, const MdArray<Tb, Eb, Lb, Cb>& b)
    -> MdArray<Ta, Ea, La, Ca>& {
  return add(a, b, a);
}

// a *= b is a·b assigned back to a. Matrix product cannot alias its output, so
// route through the allocating value form and copy back — the temporary is
// explicit, never hidden. Requires b square with side == a columns (so the
// product keeps a's shape); a's type and storage are preserved.
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
