#pragma once

#include <array>
#include <mdspan>
#include <type_traits>

#include "matrix.h"  // view, MatrixView

namespace tempura {

// A zero-copy transpose VIEW: logical (i, j) reads physical (j, i). Both extents and
// strides swap, so the result is a std::mdspan over m's existing storage — no copy —
// that flows through every MatrixView kernel unchanged (the std::linalg::transposed
// idiom). Static extents are preserved, swapped, so compile-time shape checks survive
// (`multiply(transposed(a), a)` deduces a static Gram matrix).
//
// A forwarding reference so a temporary view composes (`transposed(transposed(m))`).
// A mutable m yields a writable transpose; a const m yields mdspan<const T>. Materialize
// into owned row-major storage with `Dense<T, dyn, dyn> r = transposed(m);`.
//
// PRECONDITIONS: m's storage outlives the view; m's view must be STRIDED — true for
// owners and strided/transposed views, NOT for a gather view (permutedRows), whose
// mapping has no stride() to read. conjTransposed is deferred until complex support
// lands (it rides on a custom accessor, not the layout; for real T it equals transposed).
template <typename M>
  requires MatrixView<std::remove_reference_t<M>>
constexpr auto transposed(M&& m) {
  auto v = view(m);
  using V = decltype(v);
  static_assert(V::is_always_strided(),
                "transposed requires a strided view; a gather view (permutedRows) has no stride()");
  using E = typename V::extents_type;
  using Index = typename E::index_type;
  using TE = std::extents<Index, E::static_extent(1), E::static_extent(0)>;
  std::layout_stride::mapping<TE> mp{
      TE{static_cast<Index>(v.extent(1)), static_cast<Index>(v.extent(0))},
      std::array<Index, 2>{v.stride(1), v.stride(0)}};
  return std::mdspan<typename V::element_type, TE, std::layout_stride, typename V::accessor_type>(
      v.data_handle(), mp);
}

}  // namespace tempura
