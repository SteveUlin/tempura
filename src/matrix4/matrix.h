#pragma once

#include <array>
#include <cassert>
#include <concepts>
#include <cstddef>
#include <mdspan>
#include <span>
#include <type_traits>
#include <utility>
#include <vector>

namespace tempura {

// Dense — the owning matrix/tensor: a Container of elements + a layout_right
// mapping, exposing a std::mdspan via toMdspan() (the seam the view-based kernels
// compute over). Storage is always row-major; transposed/permuted/strided are VIEW
// concerns (mdspan layouts), never owner concerns, so there is no Layout parameter.
template <typename T, typename Extents, typename Container = std::vector<T>>
class Dense {
 public:
  using extents_type = Extents;
  using layout_type = std::layout_right;
  using container_type = Container;
  using mapping_type = typename layout_type::template mapping<extents_type>;
  using element_type = T;
  using value_type = std::remove_cv_t<T>;
  using index_type = typename extents_type::index_type;
  using size_type = typename extents_type::size_type;
  using rank_type = typename extents_type::rank_type;
  using reference = typename container_type::reference;
  using const_reference = typename container_type::const_reference;

  constexpr Dense() : Dense(extents_type{}) {}
  constexpr Dense(const Dense&) = default;
  constexpr Dense(Dense&&) = default;
  constexpr auto operator=(const Dense&) -> Dense& = default;
  constexpr auto operator=(Dense&&) -> Dense& = default;

  // Shape ctor: Dense(3, 4). Constrained to integers — a floating-point extent is
  // a bug, not something to silently truncate — and to either the full rank or the
  // dynamic dims only.
  template <std::integral... IndexTypes>
    requires(sizeof...(IndexTypes) == extents_type::rank() ||
             sizeof...(IndexTypes) == extents_type::rank_dynamic())
  constexpr explicit Dense(IndexTypes... exts)
      : Dense(extents_type{static_cast<index_type>(exts)...}) {}

  // Shape only — elements value-initialize (zeros).
  constexpr explicit Dense(const extents_type& exts)
      : mapping_{exts}, container_{makeContainer(mapping_.required_span_size())} {}

  // Adopt a ready-made container.
  constexpr Dense(const extents_type& exts, container_type c)
      : mapping_{exts}, container_{std::move(c)} {
    assert(container_.size() == static_cast<size_type>(mapping_.required_span_size()));
  }

  // Materialize from any mdspan (e.g. a permuted/transposed view): copies through
  // the source's layout into owned, contiguous, row-major storage.
  template <typename OT, typename OE, typename OL, typename OA>
    requires std::is_constructible_v<extents_type, OE> && (OE::rank() == extents_type::rank())
  explicit(!std::is_convertible_v<OE, extents_type>) constexpr Dense(
      const std::mdspan<OT, OE, OL, OA>& src)
      : mapping_{extents_type(src.extents())},
        container_{makeContainer(mapping_.required_span_size())} {
    copyFrom(src);
  }

  // Row-literal init: Dense{{1, 2}, {3, 4}}. Enabled only for a rank-2, fully
  // static, array-backed Dense (i.e. an InlineMatrix) — each braced row is a
  // C-array whose length must equal the column count. CTAD (guide below) infers
  // the shape, so `Dense{{1., 2.}, {3., 4.}}` is an InlineMatrix<double, 2, 2>.
  template <typename U, std::size_t... Cols>
    requires(extents_type::rank() == 2 && std::convertible_to<U, T> &&
             sizeof...(Cols) == extents_type::static_extent(0) &&
             ((Cols == extents_type::static_extent(1)) && ...))
  constexpr explicit Dense(const U (&... rows)[Cols]) : Dense() {  // NOLINT(*-avoid-c-arrays)
    fillRows(std::index_sequence_for<decltype(rows)...>{}, rows...);
  }

  template <std::integral... Indices>
    requires(sizeof...(Indices) == extents_type::rank())
  constexpr auto operator[](Indices... idxs) -> reference {
    return container_[mapping_(static_cast<index_type>(idxs)...)];
  }
  template <std::integral... Indices>
    requires(sizeof...(Indices) == extents_type::rank())
  constexpr auto operator[](Indices... idxs) const -> const_reference {
    return container_[mapping_(static_cast<index_type>(idxs)...)];
  }
  template <typename OtherIndex>
    requires std::convertible_to<const OtherIndex&, index_type>
  constexpr auto operator[](const std::array<OtherIndex, extents_type::rank()>& idxs) -> reference {
    return container_[offsetOf(idxs, std::make_index_sequence<extents_type::rank()>{})];
  }
  template <typename OtherIndex>
    requires std::convertible_to<const OtherIndex&, index_type>
  constexpr auto operator[](const std::array<OtherIndex, extents_type::rank()>& idxs) const
      -> const_reference {
    return container_[offsetOf(idxs, std::make_index_sequence<extents_type::rank()>{})];
  }

  constexpr auto extents() const -> const extents_type& { return mapping_.extents(); }
  constexpr auto extent(rank_type r) const -> index_type { return extents().extent(r); }
  static constexpr auto rank() -> rank_type { return extents_type::rank(); }
  static constexpr auto rankDynamic() -> rank_type { return extents_type::rank_dynamic(); }
  static constexpr auto staticExtent(rank_type r) -> std::size_t {
    return extents_type::static_extent(r);
  }
  constexpr auto size() const -> size_type {
    size_type n = 1;
    for (rank_type r = 0; r < extents_type::rank(); ++r) n *= static_cast<size_type>(extent(r));
    return n;
  }
  constexpr auto empty() const -> bool { return size() == 0; }
  constexpr auto container() const -> const container_type& { return container_; }
  // Raw storage pointer; with extents() it's all the free toMdspan() (below) needs.
  constexpr auto dataHandle() -> element_type* { return container_.data(); }
  constexpr auto dataHandle() const -> const element_type* { return container_.data(); }

 private:
  static constexpr auto makeContainer(index_type span_size) -> container_type {
    if constexpr (requires { container_type(static_cast<size_type>(span_size)); }) {
      return container_type(static_cast<size_type>(span_size));
    } else {
      return container_type{};
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
    for (std::size_t j = 0; j < Cols; ++j) (*this)[i, j] = static_cast<T>(row[j]);
  }

  mapping_type mapping_;
  container_type container_;
};

// CTAD for row-literal init: Dense{{1, 2}, {3, 4}} → a static array-backed Dense
// (an InlineMatrix). Rows give the row count; the first row gives the column count.
template <typename U, std::size_t First, std::size_t... Rest>
Dense(const U (&first)[First], const U (&... rest)[Rest])  // NOLINT(*-avoid-c-arrays)
    -> Dense<U, std::extents<std::size_t, sizeof...(Rest) + 1, First>,
             std::array<U, (sizeof...(Rest) + 1) * First>>;

// Conversion to a std::mdspan over the owner's storage — non-member by design, so
// Dense stays pure storage+access and every operation (view() included) is free.
// std::mdspan{ptr, extents} builds the layout_right mapping itself.
template <typename T, typename E, typename C>
constexpr auto toMdspan(Dense<T, E, C>& m) {
  return std::mdspan<T, E>{m.dataHandle(), m.extents()};
}
template <typename T, typename E, typename C>
constexpr auto toMdspan(const Dense<T, E, C>& m) {
  return std::mdspan<const T, E>{m.dataHandle(), m.extents()};
}

// Heap (std::vector) with optionally-static dims: storage and dim-staticness are
// independent axes, so a large matrix can still carry a compile-time-checked shape.
template <typename T, std::size_t Rows = std::dynamic_extent, std::size_t Cols = std::dynamic_extent>
using Matrix = Dense<T, std::extents<std::size_t, Rows, Cols>, std::vector<T>>;

template <typename T, std::size_t Rows, std::size_t Cols>
using InlineMatrix = Dense<T, std::extents<std::size_t, Rows, Cols>, std::array<T, Rows * Cols>>;

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
using HeapResult = Dense<T, Ext, std::vector<T>>;

// Kernels compute over views, not owners, so one definition serves owners,
// sub-blocks, foreign buffers, and transposed/strided/permuted views. view()
// passes an mdspan through unchanged; any type with a free toMdspan() (Dense, hence
// Matrix/Vec/…, and future types) becomes viewable structurally — no per-type
// overload. M deduces constness, so one overload serves mutable and const owners.
template <typename T, typename E, typename L, typename A>
constexpr auto view(std::mdspan<T, E, L, A> m) {
  return m;
}
template <typename M>
  requires requires(M& m) { toMdspan(m); }
constexpr auto view(M& m) {
  return toMdspan(m);
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
  HeapResult<Value, SumExtents<Ea, Eb>> c(va.extent(0), va.extent(1));
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
  HeapResult<Value, ProductExtents<Ea, Eb>> c(va.extent(0), vb.extent(1));
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
