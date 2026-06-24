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

// An owning multidimensional array
template <typename T, typename Extents, typename Layout = std::layout_right,
          typename Container = std::vector<T>>
class MdArray {
 public:
  using Mapping = typename Layout::template mapping<Extents>;
  using View = std::mdspan<T, Extents, Layout>;
  using ConstView = std::mdspan<const T, Extents, Layout>;
  using Index = typename Extents::index_type;

  static_assert(
      Extents::rank_dynamic() == 0 || requires(std::size_t n) { Container(n); },
      "dynamic extents require size-constructible (resizable) storage; "
      "fixed-size storage like std::array needs all-static extents");

  constexpr MdArray() : MdArray(Extents{}) {}
  constexpr MdArray(const MdArray&) = default;
  constexpr MdArray(MdArray&&) = default;
  constexpr auto operator=(const MdArray&) -> MdArray& = default;
  constexpr auto operator=(MdArray&&) -> MdArray& = default;

  // From a shape — `MdArray(dims(3, 4))`. A shape is an extents OBJECT, never bare
  // dimensions, so it can never collide with the value-init ctors. Any same-rank extents
  // works (e.g. `dims(n, 1080, 1920)` for a mixed owner): it converts to this owner's exact
  // extents, validating the static axes.
  template <typename IndexT, std::size_t... Es>
    requires(sizeof...(Es) == Extents::rank())
  constexpr explicit MdArray(const std::extents<IndexT, Es...>& shape)
      : mapping_{Extents(shape)}, container_{makeContainer(mapping_.required_span_size())} {}

  // …with a fill value.
  template <typename IndexT, std::size_t... Es>
    requires(sizeof...(Es) == Extents::rank())
  constexpr MdArray(const std::extents<IndexT, Es...>& shape, const T& val)
      : mapping_{Extents(shape)}, container_{makeContainer(mapping_.required_span_size(), val)} {}

  // …adopting a ready-made container (copy / move). Precondition: c is large enough.
  template <typename IndexT, std::size_t... Es>
    requires(sizeof...(Es) == Extents::rank())
  constexpr MdArray(const std::extents<IndexT, Es...>& shape, const Container& c)
      : mapping_{Extents(shape)}, container_{c} {
    assert(containerSize() >= static_cast<std::size_t>(mapping_.required_span_size()));
  }
  template <typename IndexT, std::size_t... Es>
    requires(sizeof...(Es) == Extents::rank())
  constexpr MdArray(const std::extents<IndexT, Es...>& shape, Container&& c)
      : mapping_{Extents(shape)}, container_{std::move(c)} {
    assert(containerSize() >= static_cast<std::size_t>(mapping_.required_span_size()));
  }

  template <typename OT, typename OE, typename OL, typename OC>
    requires std::is_constructible_v<Mapping, const typename OL::template mapping<OE>&> &&
             std::is_constructible_v<Container, const OC&>
  explicit(!std::is_convertible_v<OE, Extents>) constexpr MdArray(
      const MdArray<OT, OE, OL, OC>& other)
      : mapping_{other.mapping()}, container_{other.container()} {
    // A moved-from or undersized source would adopt a too-small buffer → silent OOB.
    // (Per-axis extent agreement is the mdspan mapping-conversion precondition.)
    assert(containerSize() >= static_cast<std::size_t>(mapping_.required_span_size()));
  }

  // Materialize any mdspan (e.g. a transposed/permuted view) into owned contiguous storage.
  template <typename OT, typename OE, typename OL, typename OA>
    requires std::is_constructible_v<Extents, OE> && (OE::rank() == Extents::rank())
  explicit(!std::is_convertible_v<OE, Extents>) constexpr MdArray(
      const std::mdspan<OT, OE, OL, OA>& src)
      : mapping_{Extents(src.extents())},
        container_{makeContainer(mapping_.required_span_size())} {
    copyFrom(src);
  }

  // Row-literal init `MdArray{{1,2},{3,4}}` → InlineDense (CTAD below). Each braced row is
  // a C-array whose length must equal the column count.
  template <typename U, std::size_t... Cols>
    requires(Extents::rank() == 2 && std::convertible_to<U, T> &&
             sizeof...(Cols) == Extents::static_extent(0) &&
             ((Cols == Extents::static_extent(1)) && ...))
  constexpr explicit MdArray(const U (&... rows)[Cols]) : MdArray() {  // NOLINT(*-avoid-c-arrays)
    fillRows(std::index_sequence_for<decltype(rows)...>{}, rows...);
  }

  // Rank-1 value-list `InlineVec<double, 3>{1, 2, 3}` (CTAD → InlineVec). Variadic over
  // values, so the count must equal the static length.
  template <typename... Us>
    requires(Extents::rank() == 1 && sizeof...(Us) >= 1 &&
             sizeof...(Us) == Extents::static_extent(0) &&
             (std::is_convertible_v<Us, T> && ...))
  constexpr explicit MdArray(Us... vals) : MdArray() {
    std::size_t i = 0;
    (((*this)[i++] = static_cast<T>(vals)), ...);
  }

  // Dynamic owners take values by braces too, the shape inferred from the literal; `(n)` /
  // `(r, c)` still makes a zeroed shape. These are initializer_list ctors so list-init
  // prefers them — `Vec<int, Dyn>{5}` is [5], not a size-5 vector.
  constexpr MdArray(std::initializer_list<T> vals)
    requires(Extents::rank() == 1 && Extents::rank_dynamic() >= 1)
      : MdArray(Extents{static_cast<Index>(vals.size())}) {
    std::size_t i = 0;
    for (const T& v : vals) (*this)[i++] = v;
  }
  constexpr MdArray(std::initializer_list<std::initializer_list<T>> rows)
    requires(Extents::rank() == 2 && Extents::rank_dynamic() >= 1)
      : MdArray(Extents{literalDim(0, rows.size()),
                        literalDim(1, rows.size() == 0 ? 0 : rows.begin()->size())}) {
    assert(rows.size() == static_cast<std::size_t>(extent(0)));  // literal rows match the shape
    std::size_t i = 0;
    for (const std::initializer_list<T>& row : rows) {
      assert(row.size() == static_cast<std::size_t>(extent(1)));  // rectangular / matches static cols
      std::size_t j = 0;
      for (const T& v : row) (*this)[i, j++] = v;
      ++i;
    }
  }

  // Subscript forwards to the synthesized view, which owns the index math.
  template <std::integral... OtherIndexTypes>
    requires(sizeof...(OtherIndexTypes) == Extents::rank())
  constexpr auto operator[](OtherIndexTypes... indices) -> T& {
    return toMdspan()[static_cast<Index>(indices)...];
  }
  template <std::integral... OtherIndexTypes>
    requires(sizeof...(OtherIndexTypes) == Extents::rank())
  constexpr auto operator[](OtherIndexTypes... indices) const -> const T& {
    return toMdspan()[static_cast<Index>(indices)...];
  }
  template <typename OtherIndexType>
    requires std::convertible_to<const OtherIndexType&, Index>
  constexpr auto operator[](std::span<OtherIndexType, Extents::rank()> indices) -> T& {
    return toMdspan()[indices];
  }
  template <typename OtherIndexType>
    requires std::convertible_to<const OtherIndexType&, Index>
  constexpr auto operator[](std::span<OtherIndexType, Extents::rank()> indices) const -> const T& {
    return toMdspan()[indices];
  }
  template <typename OtherIndexType>
    requires std::convertible_to<const OtherIndexType&, Index>
  constexpr auto operator[](const std::array<OtherIndexType, Extents::rank()>& indices) -> T& {
    return toMdspan()[indices];
  }
  template <typename OtherIndexType>
    requires std::convertible_to<const OtherIndexType&, Index>
  constexpr auto operator[](const std::array<OtherIndexType, Extents::rank()>& indices) const
      -> const T& {
    return toMdspan()[indices];
  }

  static constexpr auto rank() -> std::size_t { return Extents::rank(); }
  static constexpr auto rankDynamic() -> std::size_t { return Extents::rank_dynamic(); }
  static constexpr auto staticExtent(std::size_t r) -> std::size_t {
    return Extents::static_extent(r);
  }
  constexpr auto extents() const -> const Extents& { return mapping_.extents(); }
  constexpr auto extent(std::size_t r) const -> Index { return extents().extent(r); }
  constexpr auto size() const -> std::size_t {
    std::size_t n = 1;
    for (std::size_t r = 0; r < Extents::rank(); ++r) n *= static_cast<std::size_t>(extent(r));
    return n;
  }
  [[nodiscard]] constexpr auto empty() const -> bool { return size() == 0; }

  constexpr auto containerSize() const -> std::size_t {
    return static_cast<std::size_t>(container_.size());
  }
  constexpr auto containerData() -> T* { return std::to_address(container_.begin()); }
  constexpr auto containerData() const -> const T* { return std::to_address(container_.begin()); }
  constexpr auto container() const -> const Container& { return container_; }
  constexpr auto extractContainer() && -> Container&& { return std::move(container_); }

  constexpr auto mapping() const -> const Mapping& { return mapping_; }
  constexpr auto stride(std::size_t r) const -> Index { return mapping_.stride(r); }
  static constexpr auto isAlwaysUnique() -> bool { return Mapping::is_always_unique(); }
  static constexpr auto isAlwaysExhaustive() -> bool { return Mapping::is_always_exhaustive(); }
  static constexpr auto isAlwaysStrided() -> bool { return Mapping::is_always_strided(); }
  constexpr auto isUnique() const -> bool { return mapping_.is_unique(); }
  constexpr auto isExhaustive() const -> bool { return mapping_.is_exhaustive(); }
  constexpr auto isStrided() const -> bool { return mapping_.is_strided(); }

  // Non-const synthesizes a writable mdspan, const an mdspan<const T>; both alias the owned
  // buffer, so writes show through.
  constexpr auto toMdspan() -> View { return View{containerData(), mapping_}; }
  constexpr auto toMdspan() const -> ConstView { return ConstView{containerData(), mapping_}; }

  friend constexpr void swap(MdArray& x, MdArray& y) noexcept {
    using std::swap;
    swap(x.mapping_, y.mapping_);
    swap(x.container_, y.container_);
  }

 private:
  // A static dim keeps its compile-time value; only a dynamic dim takes the literal count.
  static constexpr auto literalDim(std::size_t axis, std::size_t literal_count) -> Index {
    return static_cast<Index>(Extents::static_extent(axis) == std::dynamic_extent
                                  ? literal_count
                                  : Extents::static_extent(axis));
  }
  static constexpr auto makeContainer(Index span_size) -> Container {
    if constexpr (requires { Container(static_cast<std::size_t>(span_size)); }) {
      return Container(static_cast<std::size_t>(span_size));
    } else {
      return Container{};
    }
  }
  static constexpr auto makeContainer(Index span_size, const T& val) -> Container {
    if constexpr (requires { Container(static_cast<std::size_t>(span_size), val); }) {
      return Container(static_cast<std::size_t>(span_size), val);
    } else {
      Container c{};
      c.fill(val);  // std::array path
      return c;
    }
  }
  template <typename Src>
  constexpr void copyFrom(const Src& src) {
    std::array<Index, Extents::rank()> idx{};
    const std::size_t total = size();
    for (std::size_t linear = 0; linear < total; ++linear) {
      (*this)[idx] = src[idx];
      for (std::size_t d = Extents::rank(); d-- > 0;) {
        if (static_cast<std::size_t>(++idx[d]) < static_cast<std::size_t>(extent(d))) break;
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
    for (std::size_t j = 0; j < Cols; ++j) (*this)[i, j] = static_cast<T>(row[j]);
  }

  Mapping mapping_;
  Container container_;
};

// CTAD: row-literal `MdArray{{1, 2}, {3, 4}}` → a static array-backed MdArray (InlineDense).
// Rows give the row count; the first row gives the column count.
template <typename U, std::size_t First, std::size_t... Rest>
MdArray(const U (&first)[First], const U (&... rest)[Rest])  // NOLINT(*-avoid-c-arrays)
    -> MdArray<U, std::extents<std::size_t, sizeof...(Rest) + 1, First>, std::layout_right,
               std::array<U, (sizeof...(Rest) + 1) * First>>;

// Rank-1 value-list CTAD: `MdArray{1., 2., 3.}` → InlineVec<double, 3>. A value list is
// never a shape (shapes use the type: `Dense<double, Dyn, Dyn>(rows, cols)`); braced rows
// `{{..},{..}}` are a non-deduced context here, so they fall to the rank-2 guide above.
template <typename U, typename... Us>
  requires(std::is_same_v<U, Us> && ...)
MdArray(U, Us...)
    -> MdArray<U, std::extents<std::size_t, 1 + sizeof...(Us)>, std::layout_right,
               std::array<U, 1 + sizeof...(Us)>>;

// P1684R5 guides: a shape + container infer element, extents, and layout.
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

}  // namespace tempura
