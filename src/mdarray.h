#pragma once

#include <cstddef>
#include <mdspan>
#include <type_traits>
#include <vector>

#include "scalar.h"

namespace tempura {

// Owning N-dimensional array — the storage-owning twin of std::mdspan.
//
// std::mdspan views memory it does not own; std::mdarray (P1684, adopted into
// C++26 but not yet in libstdc++) owns it. We reimplement the owning half on
// top of the std primitives we DO have: std::extents carries the shape,
// Layout::mapping turns a multi-index into a linear offset, and Container holds
// the elements. The value tempura adds is exactly this owning layer plus the
// ergonomic seam to mdspan — never a wrapper over mdspan/extents/layout, which
// we reuse verbatim.
//
// Container is the heap/stack seam: std::vector<T> is the heap happy-path,
// a fixed std::array<T, N> the stack variant. The shape lives in Extents
// (mix static and dynamic per dimension via std::extents); Layout picks the
// memory order (layout_right = row-major, layout_left = column-major).
template <typename T, typename Extents, typename Layout = std::layout_right,
          typename Container = std::vector<T>>
class MdArray {
 public:
  using extents_type = Extents;
  using layout_type = Layout;
  using container_type = Container;
  using mapping_type = typename layout_type::template mapping<extents_type>;
  using element_type = T;
  using value_type = std::remove_cv_t<T>;
  using index_type = typename extents_type::index_type;
  using size_type = typename extents_type::size_type;
  using rank_type = typename extents_type::rank_type;
  using pointer = typename container_type::pointer;
  using reference = typename container_type::reference;
  using const_pointer = typename container_type::const_pointer;
  using const_reference = typename container_type::const_reference;

  using mdspan_type = std::mdspan<element_type, extents_type, layout_type>;
  using const_mdspan_type = std::mdspan<const element_type, extents_type, layout_type>;

  // Default: a fully-static shape allocates its static size; an all-dynamic
  // shape starts empty (its dynamic extents default to 0).
  constexpr MdArray() : MdArray(extents_type{}) {}

  // The shape determines the allocation; elements value-initialize (zeros).
  constexpr explicit MdArray(const extents_type& exts)
      : mapping_{exts}, container_{makeContainer(mapping_.required_span_size())} {}

  constexpr explicit MdArray(const mapping_type& mapping)
      : mapping_{mapping}, container_{makeContainer(mapping_.required_span_size())} {}

  // Ergonomic shape constructor: pass the sizes directly, e.g. MdArray(3, 4).
  // Accepts either every extent or only the dynamic ones — std::extents'
  // constructor sorts out which is which.
  template <typename... IndexTypes>
    requires(sizeof...(IndexTypes) > 0) &&
            (sizeof...(IndexTypes) == extents_type::rank() ||
             sizeof...(IndexTypes) == extents_type::rank_dynamic()) &&
            (std::convertible_to<IndexTypes, index_type> && ...)
  constexpr explicit MdArray(IndexTypes... exts)
      : MdArray(extents_type{static_cast<index_type>(exts)...}) {}

  // Multidimensional element access (C++23 multi-arg subscript). The mapping
  // owns the stride arithmetic; we only index into flat storage.
  template <typename... IndexTypes>
    requires(sizeof...(IndexTypes) == extents_type::rank()) &&
            (std::convertible_to<IndexTypes, index_type> && ...)
  constexpr reference operator[](IndexTypes... idxs) {
    return container_[mapping_(static_cast<index_type>(idxs)...)];
  }

  template <typename... IndexTypes>
    requires(sizeof...(IndexTypes) == extents_type::rank()) &&
            (std::convertible_to<IndexTypes, index_type> && ...)
  constexpr const_reference operator[](IndexTypes... idxs) const {
    return container_[mapping_(static_cast<index_type>(idxs)...)];
  }

  // Shape observers — rank/static_extent are properties of the type, so static.
  static constexpr auto rank() -> rank_type { return extents_type::rank(); }
  static constexpr auto rank_dynamic() -> rank_type { return extents_type::rank_dynamic(); }
  static constexpr auto static_extent(rank_type r) -> std::size_t {
    return extents_type::static_extent(r);
  }

  constexpr auto extents() const -> const extents_type& { return mapping_.extents(); }
  constexpr auto extent(rank_type r) const -> index_type { return extents().extent(r); }
  constexpr auto stride(rank_type r) const -> index_type { return mapping_.stride(r); }
  constexpr auto mapping() const -> const mapping_type& { return mapping_; }

  // Logical element count = product of extents (mdspan semantics), which can be
  // less than the container's span for a padded/strided layout.
  constexpr auto size() const -> size_type {
    size_type n = 1;
    for (rank_type r = 0; r < rank(); ++r) n *= static_cast<size_type>(extent(r));
    return n;
  }
  constexpr auto empty() const -> bool { return size() == 0; }

  // Raw storage escape hatches.
  constexpr auto data() -> pointer { return container_.data(); }
  constexpr auto data() const -> const_pointer { return container_.data(); }
  constexpr auto container() -> container_type& { return container_; }
  constexpr auto container() const -> const container_type& { return container_; }

  // The seam to the view world: hand out an mdspan over our own storage so
  // mdspan-based algorithms (e.g. std::linalg) consume an owner with no
  // explicit .view() at the call site.
  constexpr auto toMdspan() -> mdspan_type { return mdspan_type{container_.data(), mapping_}; }
  constexpr auto toMdspan() const -> const_mdspan_type {
    return const_mdspan_type{container_.data(), mapping_};
  }
  constexpr operator mdspan_type() { return toMdspan(); }
  constexpr operator const_mdspan_type() const { return toMdspan(); }

 private:
  // A resizable container (vector) takes the span size; a fixed container
  // (array) is already sized by its type, so default-construct it.
  static constexpr auto makeContainer(index_type span_size) -> container_type {
    if constexpr (requires { container_type(static_cast<size_type>(span_size)); }) {
      return container_type(static_cast<size_type>(span_size));
    } else {
      return container_type{};
    }
  }

  mapping_type mapping_;
  container_type container_;
};

}  // namespace tempura
