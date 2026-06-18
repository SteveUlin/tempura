#pragma once

#include <array>
#include <cassert>
#include <concepts>
#include <cstddef>
#include <initializer_list>
#include <mdspan>
#include <memory>
#include <span>
#include <type_traits>
#include <utility>
#include <vector>

namespace tempura {

// Shorthand for a runtime extent, so a dynamic axis reads as `Dense<double, dyn, dyn>`
// rather than spelling std::dynamic_extent at each one.
inline constexpr std::size_t dyn = std::dynamic_extent;

// MdArray — an owning, multidimensional array: the owning analog of std::mdspan,
// following P1684R5 (std::mdarray, deferred to C++29). It stores a {mapping,
// container} pair and SYNTHESIZES an mdspan on demand via toMdspan(); it never
// stores a live mdspan, whose data_handle would dangle on copy (the vector
// reallocates) or move (an array relocates) — so every special member defaults.
//
// tempura adaptations from the paper: camelCase methods; no allocator-taking ctors
// (the codebase is allocator-free); the default ctor sizes storage for ALL ranks
// (the paper restricts it to dynamic rank), so a fully-static MdArray default-
// constructs to a correctly-sized, zeroed buffer.
template <typename ElementType, typename Extents, typename LayoutPolicy = std::layout_right,
          typename Container = std::vector<ElementType>>
class MdArray {
 public:
  using extents_type = Extents;
  using layout_type = LayoutPolicy;
  using container_type = Container;
  using mapping_type = typename layout_type::template mapping<extents_type>;
  using element_type = ElementType;
  using mdspan_type = std::mdspan<element_type, extents_type, layout_type>;
  using const_mdspan_type = std::mdspan<const element_type, extents_type, layout_type>;
  using value_type = std::remove_cv_t<element_type>;
  using index_type = typename extents_type::index_type;
  using size_type = typename extents_type::size_type;
  using rank_type = typename extents_type::rank_type;
  using pointer = decltype(std::to_address(std::declval<container_type&>().begin()));
  using reference = typename container_type::reference;
  using const_pointer = decltype(std::to_address(std::declval<const container_type&>().begin()));
  using const_reference = typename container_type::const_reference;

  // Fixed-size storage (std::array) can't be sized at runtime, so a dynamic extent
  // would silently leave the buffer the wrong size. Forbid the pairing up front.
  static_assert(extents_type::rank_dynamic() == 0 ||
                    requires(size_type n) { container_type(n); },
                "dynamic extents require size-constructible (resizable) storage; "
                "fixed-size storage like std::array needs all-static extents");

  constexpr MdArray() : MdArray(extents_type{}) {}
  constexpr MdArray(const MdArray&) = default;
  constexpr MdArray(MdArray&&) = default;
  constexpr auto operator=(const MdArray&) -> MdArray& = default;
  constexpr auto operator=(MdArray&&) -> MdArray& = default;

  // ── from a shape: MdArray(3, 4), or the full rank, or only the dynamic dims ──
  template <std::integral... OtherIndexTypes>
    requires((sizeof...(OtherIndexTypes) == extents_type::rank() ||
              sizeof...(OtherIndexTypes) == extents_type::rank_dynamic()) &&
             // A fully-static length-1 vector: `{5}` is a one-element value, not a
             // shape — yield to the value-list ctor (only here are the two ambiguous;
             // a static length-N>1 vector still constructs by its size via this ctor).
             !(extents_type::rank() == 1 && extents_type::rank_dynamic() == 0 &&
               sizeof...(OtherIndexTypes) == 1 && extents_type::static_extent(0) == 1))
  constexpr explicit MdArray(OtherIndexTypes... exts)
      : MdArray(extents_type{static_cast<index_type>(exts)...}) {}

  constexpr explicit MdArray(const extents_type& ext) : MdArray(mapping_type{ext}) {}
  constexpr explicit MdArray(const mapping_type& m)
      : mapping_{m}, container_{makeContainer(m.required_span_size())} {}

  // ── from a shape + fill value ──
  constexpr MdArray(const extents_type& ext, const value_type& val)
      : MdArray(mapping_type{ext}, val) {}
  constexpr MdArray(const mapping_type& m, const value_type& val)
      : mapping_{m}, container_{makeContainer(m.required_span_size(), val)} {}

  // ── adopt a ready-made container (copy or move) ──
  constexpr MdArray(const extents_type& ext, const container_type& c)
      : MdArray(mapping_type{ext}, c) {}
  constexpr MdArray(const mapping_type& m, const container_type& c) : mapping_{m}, container_{c} {
    assert(containerSize() >= static_cast<size_type>(m.required_span_size()));
  }
  constexpr MdArray(const extents_type& ext, container_type&& c)
      : MdArray(mapping_type{ext}, std::move(c)) {}
  constexpr MdArray(const mapping_type& m, container_type&& c)
      : mapping_{m}, container_{std::move(c)} {
    assert(containerSize() >= static_cast<size_type>(m.required_span_size()));
  }

  // ── converting copy from another MdArray (different element/extents/container) ──
  template <typename OT, typename OE, typename OL, typename OC>
    requires std::is_constructible_v<mapping_type, const typename OL::template mapping<OE>&> &&
             std::is_constructible_v<container_type, const OC&>
  explicit(!std::is_convertible_v<OE, extents_type>) constexpr MdArray(
      const MdArray<OT, OE, OL, OC>& other)
      : mapping_{other.mapping()}, container_{other.container()} {
    // The source may be moved-from or otherwise undersized; adopting its buffer with a
    // too-small container is silent OOB. (Per-axis extent agreement is the inherited
    // mdspan mapping-conversion precondition: a dynamic→static narrowing must match.)
    assert(containerSize() >= static_cast<size_type>(mapping_.required_span_size()));
  }

  // ── materialize from any mdspan (e.g. a transposed/permuted view): copies through
  // the source's layout into owned, contiguous storage ──
  template <typename OT, typename OE, typename OL, typename OA>
    requires std::is_constructible_v<extents_type, OE> && (OE::rank() == extents_type::rank())
  explicit(!std::is_convertible_v<OE, extents_type>) constexpr MdArray(
      const std::mdspan<OT, OE, OL, OA>& src)
      : mapping_{extents_type(src.extents())},
        container_{makeContainer(mapping_.required_span_size())} {
    copyFrom(src);
  }

  // ── tempura extra: row-literal init `MdArray{{1,2},{3,4}}` → InlineDense (CTAD
  // below). Each braced row is a C-array whose length must equal the column count. ──
  template <typename U, std::size_t... Cols>
    requires(extents_type::rank() == 2 && std::convertible_to<U, element_type> &&
             sizeof...(Cols) == extents_type::static_extent(0) &&
             ((Cols == extents_type::static_extent(1)) && ...))
  constexpr explicit MdArray(const U (&... rows)[Cols]) : MdArray() {  // NOLINT(*-avoid-c-arrays)
    fillRows(std::index_sequence_for<decltype(rows)...>{}, rows...);
  }

  // ── rank-1 value-list: `InlineVec<double, 3>{1, 2, 3}` (CTAD guide below → InlineVec).
  // Variadic over values — `{1, 2, 3}` passes three arguments, not one braced array — so
  // the count must equal the (static) length. ──
  template <typename... Us>
    requires(extents_type::rank() == 1 && sizeof...(Us) >= 1 &&
             sizeof...(Us) == extents_type::static_extent(0) &&
             (std::is_convertible_v<Us, element_type> && ...))
  constexpr explicit MdArray(Us... vals) : MdArray() {
    std::size_t i = 0;
    (((*this)[i++] = static_cast<element_type>(vals)), ...);
  }

  // ── dynamic value init: braces mean values for dynamic owners too, with the shape
  // inferred from the literal (the static C-array/value-list ctors above check it at
  // compile time). Mirrors std::vector: `{…}` fills, `(n)`/`(r, c)` makes a zero shape.
  // initializer-list ctors so list-init prefers them — `Vec<int, dyn>{5}` is [5], never
  // a size-5 vector. ──
  constexpr MdArray(std::initializer_list<element_type> vals)
    requires(extents_type::rank() == 1 && extents_type::rank_dynamic() >= 1)
      : MdArray(extents_type{static_cast<index_type>(vals.size())}) {
    std::size_t i = 0;
    for (const element_type& v : vals) (*this)[i++] = v;
  }
  constexpr MdArray(std::initializer_list<std::initializer_list<element_type>> rows)
    requires(extents_type::rank() == 2 && extents_type::rank_dynamic() >= 1)
      // A static dim takes its compile-time value (never the literal count) so the
      // mapping is well-formed even on a partially-static owner; the literal must then
      // match (asserted below). Only genuinely dynamic dims are inferred from the literal.
      : MdArray(extents_type{
            static_cast<index_type>(extents_type::static_extent(0) == dyn ? rows.size()
                                                                          : extents_type::static_extent(0)),
            static_cast<index_type>(extents_type::static_extent(1) == dyn
                                        ? (rows.size() == 0 ? 0 : rows.begin()->size())
                                        : extents_type::static_extent(1))}) {
    assert(rows.size() == static_cast<std::size_t>(extent(0)));  // literal rows match the shape
    std::size_t i = 0;
    for (const std::initializer_list<element_type>& row : rows) {
      assert(row.size() == static_cast<std::size_t>(extent(1)));  // rectangular / matches static cols
      std::size_t j = 0;
      for (const element_type& v : row) (*this)[i, j++] = v;
      ++i;
    }
  }

  // ── subscript: forward to the mapping, index the container ──
  template <std::integral... OtherIndexTypes>
    requires(sizeof...(OtherIndexTypes) == extents_type::rank())
  constexpr auto operator[](OtherIndexTypes... indices) -> reference {
    return container_[mapping_(static_cast<index_type>(indices)...)];
  }
  template <std::integral... OtherIndexTypes>
    requires(sizeof...(OtherIndexTypes) == extents_type::rank())
  constexpr auto operator[](OtherIndexTypes... indices) const -> const_reference {
    return container_[mapping_(static_cast<index_type>(indices)...)];
  }
  template <typename OtherIndexType>
    requires std::convertible_to<const OtherIndexType&, index_type>
  constexpr auto operator[](std::span<OtherIndexType, extents_type::rank()> indices) -> reference {
    return container_[offsetOf(indices, std::make_index_sequence<extents_type::rank()>{})];
  }
  template <typename OtherIndexType>
    requires std::convertible_to<const OtherIndexType&, index_type>
  constexpr auto operator[](std::span<OtherIndexType, extents_type::rank()> indices) const
      -> const_reference {
    return container_[offsetOf(indices, std::make_index_sequence<extents_type::rank()>{})];
  }
  template <typename OtherIndexType>
    requires std::convertible_to<const OtherIndexType&, index_type>
  constexpr auto operator[](const std::array<OtherIndexType, extents_type::rank()>& indices)
      -> reference {
    return container_[offsetOf(indices, std::make_index_sequence<extents_type::rank()>{})];
  }
  template <typename OtherIndexType>
    requires std::convertible_to<const OtherIndexType&, index_type>
  constexpr auto operator[](const std::array<OtherIndexType, extents_type::rank()>& indices) const
      -> const_reference {
    return container_[offsetOf(indices, std::make_index_sequence<extents_type::rank()>{})];
  }

  // ── size / extent info ──
  static constexpr auto rank() -> rank_type { return extents_type::rank(); }
  static constexpr auto rankDynamic() -> rank_type { return extents_type::rank_dynamic(); }
  static constexpr auto staticExtent(rank_type r) -> std::size_t {
    return extents_type::static_extent(r);
  }
  constexpr auto extents() const -> const extents_type& { return mapping_.extents(); }
  constexpr auto extent(rank_type r) const -> index_type { return extents().extent(r); }
  constexpr auto size() const -> size_type {
    size_type n = 1;
    for (rank_type r = 0; r < extents_type::rank(); ++r) n *= static_cast<size_type>(extent(r));
    return n;
  }
  [[nodiscard]] constexpr auto empty() const -> bool { return size() == 0; }

  // ── container access ──
  constexpr auto containerSize() const -> size_type {
    return static_cast<size_type>(container_.size());
  }
  constexpr auto containerData() -> pointer { return std::to_address(container_.begin()); }
  constexpr auto containerData() const -> const_pointer { return std::to_address(container_.begin()); }
  constexpr auto container() const -> const container_type& { return container_; }
  constexpr auto extractContainer() && -> container_type&& { return std::move(container_); }

  // ── layout / mapping ──
  constexpr auto mapping() const -> const mapping_type& { return mapping_; }
  constexpr auto stride(rank_type r) const -> index_type { return mapping_.stride(r); }
  static constexpr auto isAlwaysUnique() -> bool { return mapping_type::is_always_unique(); }
  static constexpr auto isAlwaysExhaustive() -> bool { return mapping_type::is_always_exhaustive(); }
  static constexpr auto isAlwaysStrided() -> bool { return mapping_type::is_always_strided(); }
  constexpr auto isUnique() const -> bool { return mapping_.is_unique(); }
  constexpr auto isExhaustive() const -> bool { return mapping_.is_exhaustive(); }
  constexpr auto isStrided() const -> bool { return mapping_.is_strided(); }

  // ── the seam: a view over the owned storage, synthesized on demand (never stored).
  // Non-const yields a writable mdspan, const yields an mdspan<const T> — write-
  // through is visible in the owner since both share containerData(). ──
  constexpr auto toMdspan() -> mdspan_type { return mdspan_type{containerData(), mapping_}; }
  constexpr auto toMdspan() const -> const_mdspan_type {
    return const_mdspan_type{containerData(), mapping_};
  }

  friend constexpr void swap(MdArray& x, MdArray& y) noexcept {
    using std::swap;
    swap(x.mapping_, y.mapping_);
    swap(x.container_, y.container_);
  }

 private:
  static constexpr auto makeContainer(index_type span_size) -> container_type {
    if constexpr (requires { container_type(static_cast<size_type>(span_size)); }) {
      return container_type(static_cast<size_type>(span_size));
    } else {
      return container_type{};
    }
  }
  static constexpr auto makeContainer(index_type span_size, const value_type& val) -> container_type {
    if constexpr (requires { container_type(static_cast<size_type>(span_size), val); }) {
      return container_type(static_cast<size_type>(span_size), val);
    } else {
      container_type c{};
      c.fill(val);  // std::array path
      return c;
    }
  }
  template <typename Indexable, std::size_t... Is>
  constexpr auto offsetOf(const Indexable& idxs, std::index_sequence<Is...>) const -> size_type {
    return static_cast<size_type>(mapping_(static_cast<index_type>(idxs[Is])...));
  }
  template <typename Src>
  constexpr void copyFrom(const Src& src) {
    std::array<index_type, extents_type::rank()> idx{};
    const size_type total = size();
    for (size_type linear = 0; linear < total; ++linear) {
      (*this)[idx] = src[idx];
      for (rank_type d = extents_type::rank(); d-- > 0;) {
        if (static_cast<size_type>(++idx[d]) < static_cast<size_type>(extent(d))) break;
        idx[d] = 0;
      }
    }
  }
  template <std::size_t... Is, typename... Rows>
  constexpr void fillRows(std::index_sequence<Is...> /*unused*/, const Rows&... rows) {
    (fillRow(Is, rows), ...);
  }
  template <typename U, std::size_t Cols>
  constexpr void fillRow(std::size_t i, const U (&row)[Cols]) {  // NOLINT(*-avoid-c-arrays)
    for (std::size_t j = 0; j < Cols; ++j) (*this)[i, j] = static_cast<element_type>(row[j]);
  }

  mapping_type mapping_;
  container_type container_;
};

// CTAD: row-literal init `MdArray{{1, 2}, {3, 4}}` → a static array-backed MdArray
// (an InlineDense). Rows give the row count; the first row gives the column count.
template <typename U, std::size_t First, std::size_t... Rest>
MdArray(const U (&first)[First], const U (&... rest)[Rest])  // NOLINT(*-avoid-c-arrays)
    -> MdArray<U, std::extents<std::size_t, sizeof...(Rest) + 1, First>, std::layout_right,
               std::array<U, (sizeof...(Rest) + 1) * First>>;

// Rank-1 value-list CTAD: `MdArray{1., 2., 3.}` → InlineVec<double, 3>. All args share
// the first's type. A value list is never a shape — shapes are spelled with the type,
// e.g. `Dense<double, dyn, dyn>(rows, cols)`. (Braced rows `{{..},{..}}` can't deduce U
// here — non-deduced context — so they fall to the rank-2 guide above.)
template <typename U, typename... Us>
  requires(std::is_same_v<U, Us> && ...)
MdArray(U, Us...)
    -> MdArray<U, std::extents<std::size_t, 1 + sizeof...(Us)>, std::layout_right,
               std::array<U, 1 + sizeof...(Us)>>;

// Deduction guides from P1684R5 — shape/mapping + container infer element, extents,
// and layout from the container and the shape source.
template <typename IndexType, std::size_t... Exts, typename Container>
MdArray(const std::extents<IndexType, Exts...>&, const Container&)
    -> MdArray<typename Container::value_type, std::extents<IndexType, Exts...>, std::layout_right,
               Container>;
template <typename IndexType, std::size_t... Exts, typename Container>
MdArray(const std::extents<IndexType, Exts...>&, Container&&)
    -> MdArray<typename Container::value_type, std::extents<IndexType, Exts...>, std::layout_right,
               Container>;
template <typename ElementType, typename Ext, typename Layout, typename Accessor>
MdArray(const std::mdspan<ElementType, Ext, Layout, Accessor>&)
    -> MdArray<std::remove_cv_t<ElementType>, Ext, Layout>;

// The rank-2 owners: Dense on the heap (std::vector), InlineDense on the stack
// (std::array, fully constexpr). Extents are explicit and per-axis static-or-dynamic —
// `Dense<double, dyn, dyn>` is a fully dynamic matrix, `Dense<double, 3, 4>` fixes the
// shape on the heap, `InlineDense<double, 3, 4>` on the stack. No defaulted dims: a
// heap matrix always states its shape (use `dyn` for a runtime axis). Other ranks use
// Vec/InlineVec (vec.h) or MdArray directly.
template <typename T, std::size_t Rows, std::size_t Cols>
using Dense = MdArray<T, std::extents<std::size_t, Rows, Cols>, std::layout_right, std::vector<T>>;

template <typename T, std::size_t Rows, std::size_t Cols>
using InlineDense =
    MdArray<T, std::extents<std::size_t, Rows, Cols>, std::layout_right, std::array<T, Rows * Cols>>;

}  // namespace tempura
