#pragma once

#include <cassert>
#include <cstddef>
#include <mdspan>
#include <span>

#include "matrix.h"       // view(), Viewable, MatrixView
#include "permutation.h"  // Permutation

namespace tempura {

// An mdspan layout that GATHERS rows through an index map: logical (i, j) reads
// physical (order[i], j) of a base mapping. Because it composes over any base
// layout (layout_right, strided, transposed), a row-permuted matrix is just an
// mdspan and flows through every kernel (add/multiply/…) unchanged — the same
// trick std::linalg uses for transposed(). The index map is BORROWED (a non-owning
// span into a Permutation's storage), so that Permutation must outlive the view.
template <typename BaseLayout>
struct layout_row_permuted {
  template <typename Extents>
  class mapping {
   public:
    using extents_type = Extents;
    using index_type = typename extents_type::index_type;
    using size_type = typename extents_type::size_type;
    using rank_type = typename extents_type::rank_type;
    using layout_type = layout_row_permuted;
    using base_mapping = typename BaseLayout::template mapping<extents_type>;

    static_assert(extents_type::rank() == 2, "row-permuted layout is 2D");

    constexpr mapping(const base_mapping& base, std::span<const std::size_t> order) noexcept
        : base_{base}, order_{order} {}

    constexpr auto extents() const noexcept -> const extents_type& { return base_.extents(); }

    constexpr auto operator()(index_type i, index_type j) const noexcept -> index_type {
      return base_(static_cast<index_type>(order_[static_cast<std::size_t>(i)]), j);
    }

    constexpr auto required_span_size() const noexcept -> index_type {
      return base_.required_span_size();
    }

    static constexpr auto is_always_unique() noexcept -> bool { return true; }
    static constexpr auto is_always_exhaustive() noexcept -> bool { return false; }
    static constexpr auto is_always_strided() noexcept -> bool { return false; }
    constexpr auto is_unique() const noexcept -> bool { return true; }
    constexpr auto is_exhaustive() const noexcept -> bool { return false; }
    constexpr auto is_strided() const noexcept -> bool { return false; }

    friend constexpr auto operator==(const mapping& a, const mapping& b) noexcept -> bool {
      return a.base_ == b.base_ && a.order_.data() == b.order_.data() &&
             a.order_.size() == b.order_.size();
    }

   private:
    base_mapping base_;
    std::span<const std::size_t> order_;
  };
};

// Lazy view: row i of the result is row order[i] of m (NumPy `m[order]` gather),
// moving no data. Pass it straight to a kernel for reorder-without-copy, or
// `Dense r = permutedRows(...)` to MATERIALIZE into contiguous storage when you
// will iterate it in a hot loop (the order[i] indirection is cache-hostile there).
// PRECONDITION: `order` (and m's storage) must outlive the returned view.
template <Viewable M, std::size_t S>
constexpr auto permutedRows(M& m, const Permutation<S>& order) {
  auto base = view(m);
  using V = decltype(base);
  using E = typename V::extents_type;
  static_assert(E::rank() == 2, "permutedRows needs a 2D matrix");
  using T = typename V::element_type;
  using L = typename V::layout_type;
  using A = typename V::accessor_type;
  assert(static_cast<std::size_t>(base.extent(0)) == order.size());
  using Layout = layout_row_permuted<L>;
  typename Layout::template mapping<E> permuted{base.mapping(), order.span()};
  return std::mdspan<T, E, Layout, A>(base.data_handle(), permuted);
}

}  // namespace tempura
