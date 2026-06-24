#pragma once

#include <array>
#include <cassert>
#include <cstddef>
#include <mdspan>
#include <span>
#include <type_traits>
#include <vector>

#include "dyn.h"
#include "mdarray.h"

namespace tempura {

// The rank-2 owners: Dense on the heap (std::vector), InlineDense on the stack
// (std::array, fully constexpr). Extents are explicit and per-axis static-or-dynamic —
// `Dense<double, Dyn, Dyn>` is a fully dynamic matrix, `Dense<double, 3, 4>` fixes the
// shape on the heap, `InlineDense<double, 3, 4>` on the stack. No defaulted dims: a heap
// matrix always states its shape (use `Dyn` for a runtime axis). ColMajor/InlineColMajor
// mirror them in column-major (layout_left). Other ranks use Vec/InlineVec (vec.h) or
// MdArray directly.
template <typename T, std::size_t Rows, std::size_t Cols>
using Dense = MdArray<T, std::extents<std::size_t, Rows, Cols>, std::layout_right, std::vector<T>>;

template <typename T, std::size_t Rows, std::size_t Cols>
using InlineDense =
    MdArray<T, std::extents<std::size_t, Rows, Cols>, std::layout_right, std::array<T, Rows * Cols>>;

template <typename T, std::size_t Rows, std::size_t Cols>
using ColMajor = MdArray<T, std::extents<std::size_t, Rows, Cols>, std::layout_left, std::vector<T>>;

template <typename T, std::size_t Rows, std::size_t Cols>
using InlineColMajor =
    MdArray<T, std::extents<std::size_t, Rows, Cols>, std::layout_left, std::array<T, Rows * Cols>>;

// Merge two extents of one axis for a result type: a known dim wins, a dynamic dim
// defers to the other. Conflicting *static* dims are a bug — assert it (a failed assert
// during constant evaluation is a hard compile error, so this catches it where used).
constexpr auto mergeExtent(std::size_t a, std::size_t b) -> std::size_t {
  assert(a == std::dynamic_extent || b == std::dynamic_extent || a == b);
  return a != std::dynamic_extent ? a : b;
}

// A dynamic (unknown) dim defers the check to runtime, so a static_assert on this
// fails only when both dims are known and unequal.
constexpr auto dimsCompatible(std::size_t a, std::size_t b) -> bool {
  return a == std::dynamic_extent || b == std::dynamic_extent || a == b;
}

// Aliasing guard for the forbid-by-contract kernels: catches the common bug — dst
// sharing a base pointer with an input, e.g. multiply(A, B, A). Pointer EQUALITY is
// constexpr-safe across allocations (yields false); relational sub-block overlap is
// not, so partial overlap stays an unchecked precondition (std::linalg-style).
constexpr auto sharesStorage(const void* a, const void* b) -> bool { return a == b; }

// Reduction accumulator: the operands' (and destination's) common type, but float is
// promoted to double so a long matmul/dot sum doesn't shed 3–4 digits. Integer and
// wider floating types pass through unchanged. Folding the destination type in means
// `Dense<double> = multiply(intA, intB)` accumulates in double, not int (no overflow).
template <typename... Ts>
using Accumulator = std::conditional_t<std::is_same_v<std::common_type_t<Ts...>, float>,
                                       double, std::common_type_t<Ts...>>;

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
using HeapResult = MdArray<T, Ext, std::layout_right, std::vector<T>>;

// Kernels compute over views, not owners, so one definition serves owners,
// sub-blocks, foreign buffers, and transposed/strided/permuted views. view()
// passes an mdspan through unchanged; any type with a member toMdspan() (MdArray,
// hence Dense/Vec/…, and future owners) becomes viewable structurally — no per-type
// overload. M deduces constness, so one overload serves mutable and const owners.
template <typename T, typename E, typename L, typename A>
constexpr auto view(std::mdspan<T, E, L, A> m) {
  return m;
}
template <typename M>
  requires requires(M& m) { m.toMdspan(); }
constexpr auto view(M& m) {
  return m.toMdspan();
}

template <typename> inline constexpr bool isMdspan = false;
template <typename T, typename E, typename L, typename A>
inline constexpr bool isMdspan<std::mdspan<T, E, L, A>> = true;

// view(m) must yield a std::mdspan; ViewOf<M> names that mdspan type.
template <typename M> using ViewOf = decltype(view(std::declval<M&>()));
template <typename M>
concept Viewable = requires(M& m) { view(m); } && isMdspan<ViewOf<M>>;
template <typename M, std::size_t Rank>
concept ViewableRank = Viewable<M> && (ViewOf<M>::extents_type::rank() == Rank);

// A kernel operand viewable as a rank-2 mdspan — the matrix kernels' constraint.
template <typename M>
concept MatrixView = ViewableRank<M, 2>;

// The eager/lazy distinction made checkable: a View borrows (a std::mdspan — what the
// structural adaptors transposed/permutedRows/submdspan return, lazily), an Owning value
// owns its buffer (an MdArray — what the arithmetic kernels return, eagerly). A View is
// lifetime-bound to its source; an Owning value is safe to keep.
template <typename M>
concept View = Viewable<M> && isMdspan<std::remove_cvref_t<M>>;
template <typename M>
concept Owning = Viewable<M> && !isMdspan<std::remove_cvref_t<M>>;

template <typename E>
constexpr auto staticExtentProduct() -> std::size_t {
  std::size_t p = 1;
  for (std::size_t r = 0; r < E::rank(); ++r) p *= E::static_extent(r);
  return p;
}

enum class Storage { Heap, Stack };

// Eagerly copy a view (or owner) into a FRESH contiguous row-major owner — the explicit
// bridge from a lazy View to an Owning value. Heap (Dense/vector) by default; Stack
// (InlineDense/array) on request, which requires fully-static extents. Static extents
// are preserved, so `materialize(transposed(a))` of a 2×3 is a Dense<T, 3, 2>.
template <Storage Where = Storage::Heap, Viewable V>
constexpr auto materialize(const V& v) {
  auto s = view(v);
  using T = typename decltype(s)::value_type;
  using E = typename decltype(s)::extents_type;
  if constexpr (Where == Storage::Stack) {
    static_assert(E::rank_dynamic() == 0, "materialize<Storage::Stack> needs fully-static extents");
    return MdArray<T, E, std::layout_right, std::array<T, staticExtentProduct<E>()>>(s);
  } else {
    return HeapResult<T, E>(s);
  }
}

// dst ← a + b, element-wise. Aliasing: dst may be exactly a or b (in-place);
// partial/shifted overlap is undefined.
template <MatrixView A, MatrixView B, MatrixView D>
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
// aliasing corrupts the result. The common dst==a/dst==b case is asserted; partial
// sub-block overlap stays an unchecked contract.
template <MatrixView A, MatrixView B, MatrixView D>
constexpr auto multiply(const A& a, const B& b, D& dst) -> D& {
  auto va = view(a);
  auto vb = view(b);
  auto vd = view(dst);
  using Ea = typename decltype(va)::extents_type;
  using Eb = typename decltype(vb)::extents_type;
  using Ed = typename decltype(vd)::extents_type;
  using Td = typename decltype(vd)::value_type;
  using Acc = Accumulator<typename decltype(va)::value_type, typename decltype(vb)::value_type, Td>;
  static_assert(dimsCompatible(Ea::static_extent(1), Eb::static_extent(0)),
                "inner dimensions must match: A columns == B rows");
  static_assert(dimsCompatible(Ed::static_extent(0), Ea::static_extent(0)), "dst rows mismatch");
  static_assert(dimsCompatible(Ed::static_extent(1), Eb::static_extent(1)), "dst columns mismatch");
  assert(va.extent(1) == vb.extent(0));
  assert(vd.extent(0) == va.extent(0) && vd.extent(1) == vb.extent(1));
  assert(!sharesStorage(vd.data_handle(), va.data_handle()) &&
         !sharesStorage(vd.data_handle(), vb.data_handle()));

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

// dst ← dst + a · b (the "updating" form). dst is read-and-written by design, but
// must not alias a or b — those are summed across full rows/columns, so sharing
// storage with the output corrupts the result. Asserted, like multiply.
template <MatrixView A, MatrixView B, MatrixView D>
constexpr auto multiplyAdd(const A& a, const B& b, D& dst) -> D& {
  auto va = view(a);
  auto vb = view(b);
  auto vd = view(dst);
  using Ea = typename decltype(va)::extents_type;
  using Eb = typename decltype(vb)::extents_type;
  using Ed = typename decltype(vd)::extents_type;
  using Td = typename decltype(vd)::value_type;
  using Acc = Accumulator<typename decltype(va)::value_type, typename decltype(vb)::value_type, Td>;
  static_assert(dimsCompatible(Ea::static_extent(1), Eb::static_extent(0)),
                "inner dimensions must match: A columns == B rows");
  static_assert(dimsCompatible(Ed::static_extent(0), Ea::static_extent(0)), "dst rows mismatch");
  static_assert(dimsCompatible(Ed::static_extent(1), Eb::static_extent(1)), "dst columns mismatch");
  assert(va.extent(1) == vb.extent(0));
  assert(vd.extent(0) == va.extent(0) && vd.extent(1) == vb.extent(1));
  assert(!sharesStorage(vd.data_handle(), va.data_handle()) &&
         !sharesStorage(vd.data_handle(), vb.data_handle()));

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

// dst ← v everywhere (element-wise fill).
template <MatrixView D, typename T>
constexpr auto fill(D& dst, const T& v) -> D& {
  auto vd = view(dst);
  using Td = typename decltype(vd)::value_type;
  const std::size_t rows = vd.extent(0);
  const std::size_t cols = vd.extent(1);
  for (std::size_t i = 0; i < rows; ++i)
    for (std::size_t j = 0; j < cols; ++j) vd[i, j] = static_cast<Td>(v);
  return dst;
}

// dst ← identity: 1 on the main diagonal, 0 elsewhere. dst must be square.
template <MatrixView D>
constexpr auto identity(D& dst) -> D& {
  auto vd = view(dst);
  using Ed = typename decltype(vd)::extents_type;
  using Td = typename decltype(vd)::value_type;
  static_assert(dimsCompatible(Ed::static_extent(0), Ed::static_extent(1)), "identity needs a square matrix");
  assert(vd.extent(0) == vd.extent(1));
  fill(dst, Td{0});
  const std::size_t n = vd.extent(0);
  for (std::size_t i = 0; i < n; ++i) vd[i, i] = Td{1};
  return dst;
}

// Value form: an N×N identity. Static size on the heap (`eye<double, 3>()`), or runtime
// (`eye<double>(n)`) — never a dynamic template N (which would size SIZE_MAX × SIZE_MAX).
template <typename T, std::size_t N>
constexpr auto eye() {
  static_assert(N != std::dynamic_extent,
                "eye<T, N>() needs a static N; for a runtime identity use eye<T>(n)");
  HeapResult<T, std::extents<std::size_t, N, N>> e{};
  identity(e);
  return e;
}
template <typename T>
constexpr auto eye(std::size_t n) {
  Dense<T, Dyn, Dyn> e(dims(n, n));
  identity(e);
  return e;
}

template <MatrixView A, MatrixView B>
constexpr auto add(const A& a, const B& b) {
  auto va = view(a);
  auto vb = view(b);
  using Ea = typename decltype(va)::extents_type;
  using Eb = typename decltype(vb)::extents_type;
  using Value = std::common_type_t<typename decltype(va)::value_type, typename decltype(vb)::value_type>;
  HeapResult<Value, SumExtents<Ea, Eb>> c(dims(va.extent(0), va.extent(1)));
  add(va, vb, c);
  return c;
}
template <MatrixView A, MatrixView B>
constexpr auto multiply(const A& a, const B& b) {
  auto va = view(a);
  auto vb = view(b);
  using Ea = typename decltype(va)::extents_type;
  using Eb = typename decltype(vb)::extents_type;
  using Value = std::common_type_t<typename decltype(va)::value_type, typename decltype(vb)::value_type>;
  HeapResult<Value, ProductExtents<Ea, Eb>> c(dims(va.extent(0), vb.extent(1)));
  multiply(va, vb, c);
  return c;
}

template <MatrixView A, MatrixView B>
constexpr auto operator+(const A& a, const B& b) {
  return add(a, b);
}
template <MatrixView A, MatrixView B>
constexpr auto operator*(const A& a, const B& b) {
  return multiply(a, b);
}

// add is alias-safe, so a is its own destination.
template <MatrixView A, MatrixView B>
constexpr auto operator+=(A& a, const B& b) -> A& {
  add(a, b, a);
  return a;
}

// a ← a·b. Matmul can't write into its own input and a·b's type may differ from
// a, so compute then copy back — a's type and storage are preserved.
template <MatrixView A, MatrixView B>
constexpr auto operator*=(A& a, const B& b) -> A& {
  auto va = view(a);
  auto vb = view(b);
  using Ea = typename decltype(va)::extents_type;
  using Eb = typename decltype(vb)::extents_type;
  using Ta = typename decltype(va)::value_type;
  static_assert(dimsCompatible(Eb::static_extent(0), Eb::static_extent(1)), "b must be square");
  static_assert(dimsCompatible(Ea::static_extent(1), Eb::static_extent(0)), "b side != a columns");
  assert(vb.extent(0) == vb.extent(1) && "in-place A*=B requires square B");
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
