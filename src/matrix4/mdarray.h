#pragma once

#include <array>
#include <cassert>
#include <concepts>
#include <cstddef>
#include <mdspan>
#include <memory>
#include <span>
#include <type_traits>
#include <utility>
#include <vector>

namespace tempura {

// Owning N-dimensional array — the storage-owning twin of std::mdspan, and a
// stand-in for std::mdarray (P1684, adopted into C++26 but not yet in
// libstdc++).
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

  constexpr MdArray() : MdArray(extents_type{}) {}
  constexpr MdArray(const MdArray&) = default;
  constexpr MdArray(MdArray&&) = default;

  // Ergonomic shape constructor: MdArray(3, 4).
  template <typename... IndexTypes>
    requires(sizeof...(IndexTypes) > 0) &&
            (sizeof...(IndexTypes) == extents_type::rank() ||
             sizeof...(IndexTypes) == extents_type::rank_dynamic()) &&
            (std::convertible_to<IndexTypes, index_type> && ...)
  constexpr explicit MdArray(IndexTypes... exts)
      : MdArray(extents_type{static_cast<index_type>(exts)...}) {}

  // Shape only — elements value-initialize (zeros).
  constexpr explicit MdArray(const extents_type& exts)
      : mapping_{exts}, container_{makeContainer(mapping_.required_span_size())} {}

  constexpr explicit MdArray(const mapping_type& m)
      : mapping_{m}, container_{makeContainer(mapping_.required_span_size())} {}

  constexpr MdArray(const extents_type& exts, const container_type& c)
      : mapping_{exts}, container_{c} {
    assert(container_.size() == static_cast<size_type>(mapping_.required_span_size()));
  }
  constexpr MdArray(const extents_type& exts, container_type&& c)
      : mapping_{exts}, container_{std::move(c)} {
    assert(container_.size() == static_cast<size_type>(mapping_.required_span_size()));
  }
  constexpr MdArray(const mapping_type& m, const container_type& c)
      : mapping_{m}, container_{c} {
    assert(container_.size() == static_cast<size_type>(mapping_.required_span_size()));
  }
  constexpr MdArray(const mapping_type& m, container_type&& c)
      : mapping_{m}, container_{std::move(c)} {
    assert(container_.size() == static_cast<size_type>(mapping_.required_span_size()));
  }

  template <typename OtherT, typename OtherExtents, typename OtherLayout, typename OtherAccessor>
    requires std::is_constructible_v<extents_type, OtherExtents> &&
             (OtherExtents::rank() == extents_type::rank())
  explicit(!std::is_convertible_v<OtherExtents, extents_type>) constexpr MdArray(
      const std::mdspan<OtherT, OtherExtents, OtherLayout, OtherAccessor>& other)
      : mapping_{extents_type(other.extents())},
        container_{makeContainer(mapping_.required_span_size())} {
    assignFrom(other);
  }

  template <typename OtherT, typename OtherExtents, typename OtherLayout, typename OtherContainer>
    requires std::is_constructible_v<extents_type, OtherExtents> &&
             (OtherExtents::rank() == extents_type::rank())
  explicit(!std::is_convertible_v<OtherExtents, extents_type>) constexpr MdArray(
      const MdArray<OtherT, OtherExtents, OtherLayout, OtherContainer>& other)
      : mapping_{extents_type(other.extents())},
        container_{makeContainer(mapping_.required_span_size())} {
    assignFrom(other);
  }

  template <typename Alloc>
    requires std::uses_allocator_v<container_type, Alloc>
  constexpr MdArray(const extents_type& exts, const Alloc& a)
      : mapping_{exts}, container_{std::make_obj_using_allocator<container_type>(
                            a, static_cast<size_type>(mapping_.required_span_size()))} {}

  template <typename Alloc>
    requires std::uses_allocator_v<container_type, Alloc>
  constexpr MdArray(const mapping_type& m, const Alloc& a)
      : mapping_{m}, container_{std::make_obj_using_allocator<container_type>(
                         a, static_cast<size_type>(mapping_.required_span_size()))} {}

  template <typename Alloc>
    requires std::uses_allocator_v<container_type, Alloc>
  constexpr MdArray(const extents_type& exts, const container_type& c, const Alloc& a)
      : mapping_{exts}, container_{std::make_obj_using_allocator<container_type>(a, c)} {
    assert(container_.size() == static_cast<size_type>(mapping_.required_span_size()));
  }
  template <typename Alloc>
    requires std::uses_allocator_v<container_type, Alloc>
  constexpr MdArray(const extents_type& exts, container_type&& c, const Alloc& a)
      : mapping_{exts},
        container_{std::make_obj_using_allocator<container_type>(a, std::move(c))} {
    assert(container_.size() == static_cast<size_type>(mapping_.required_span_size()));
  }
  template <typename Alloc>
    requires std::uses_allocator_v<container_type, Alloc>
  constexpr MdArray(const mapping_type& m, const container_type& c, const Alloc& a)
      : mapping_{m}, container_{std::make_obj_using_allocator<container_type>(a, c)} {
    assert(container_.size() == static_cast<size_type>(mapping_.required_span_size()));
  }
  template <typename Alloc>
    requires std::uses_allocator_v<container_type, Alloc>
  constexpr MdArray(const mapping_type& m, container_type&& c, const Alloc& a)
      : mapping_{m},
        container_{std::make_obj_using_allocator<container_type>(a, std::move(c))} {
    assert(container_.size() == static_cast<size_type>(mapping_.required_span_size()));
  }
  template <typename OtherT, typename OtherExtents, typename OtherLayout,
            typename OtherAccessor, typename Alloc>
    requires std::uses_allocator_v<container_type, Alloc> &&
             std::is_constructible_v<extents_type, OtherExtents> &&
             (OtherExtents::rank() == extents_type::rank())
  explicit(!std::is_convertible_v<OtherExtents, extents_type>) constexpr MdArray(
      const std::mdspan<OtherT, OtherExtents, OtherLayout, OtherAccessor>& other, const Alloc& a)
      : mapping_{extents_type(other.extents())},
        container_{std::make_obj_using_allocator<container_type>(
            a, static_cast<size_type>(mapping_.required_span_size()))} {
    assignFrom(other);
  }
  template <typename OtherT, typename OtherExtents, typename OtherLayout,
            typename OtherContainer, typename Alloc>
    requires std::uses_allocator_v<container_type, Alloc> &&
             std::is_constructible_v<extents_type, OtherExtents> &&
             (OtherExtents::rank() == extents_type::rank())
  explicit(!std::is_convertible_v<OtherExtents, extents_type>) constexpr MdArray(
      const MdArray<OtherT, OtherExtents, OtherLayout, OtherContainer>& other, const Alloc& a)
      : mapping_{extents_type(other.extents())},
        container_{std::make_obj_using_allocator<container_type>(
            a, static_cast<size_type>(mapping_.required_span_size()))} {
    assignFrom(other);
  }

  constexpr auto operator=(const MdArray&) -> MdArray& = default;
  constexpr auto operator=(MdArray&&) -> MdArray& = default;

  template <typename... IndexTypes>
    requires(sizeof...(IndexTypes) == extents_type::rank()) &&
            (std::convertible_to<IndexTypes, index_type> && ...)
  constexpr auto operator[](IndexTypes... idxs) -> reference {
    return container_[mapping_(static_cast<index_type>(idxs)...)];
  }
  template <typename... IndexTypes>
    requires(sizeof...(IndexTypes) == extents_type::rank()) &&
            (std::convertible_to<IndexTypes, index_type> && ...)
  constexpr auto operator[](IndexTypes... idxs) const -> const_reference {
    return container_[mapping_(static_cast<index_type>(idxs)...)];
  }

  template <typename OtherIndexType>
    requires std::convertible_to<const OtherIndexType&, index_type>
  constexpr auto operator[](std::span<OtherIndexType, extents_type::rank()> idxs) -> reference {
    return container_[offsetOf(idxs, std::make_index_sequence<extents_type::rank()>{})];
  }
  template <typename OtherIndexType>
    requires std::convertible_to<const OtherIndexType&, index_type>
  constexpr auto operator[](std::span<OtherIndexType, extents_type::rank()> idxs) const
      -> const_reference {
    return container_[offsetOf(idxs, std::make_index_sequence<extents_type::rank()>{})];
  }
  template <typename OtherIndexType>
    requires std::convertible_to<const OtherIndexType&, index_type>
  constexpr auto operator[](const std::array<OtherIndexType, extents_type::rank()>& idxs)
      -> reference {
    return container_[offsetOf(idxs, std::make_index_sequence<extents_type::rank()>{})];
  }
  template <typename OtherIndexType>
    requires std::convertible_to<const OtherIndexType&, index_type>
  constexpr auto operator[](const std::array<OtherIndexType, extents_type::rank()>& idxs) const
      -> const_reference {
    return container_[offsetOf(idxs, std::make_index_sequence<extents_type::rank()>{})];
  }

  constexpr auto size() const -> size_type {
    size_type n = 1;
    for (rank_type r = 0; r < extents_type::rank(); ++r) n *= static_cast<size_type>(extent(r));
    return n;
  }
  constexpr auto empty() const -> bool { return size() == 0; }

  constexpr auto extents() const -> const extents_type& { return mapping_.extents(); }
  constexpr auto mapping() const -> const mapping_type& { return mapping_; }

  static constexpr auto rank() -> rank_type { return extents_type::rank(); }
  static constexpr auto rankDynamic() -> rank_type { return extents_type::rank_dynamic(); }
  static constexpr auto staticExtent(rank_type r) -> std::size_t {
    return extents_type::static_extent(r);
  }
  constexpr auto extent(rank_type r) const -> index_type { return extents().extent(r); }
  constexpr auto stride(rank_type r) const -> index_type { return mapping_.stride(r); }

  static constexpr auto isAlwaysUnique() -> bool { return mapping_type::is_always_unique(); }
  static constexpr auto isAlwaysExhaustive() -> bool { return mapping_type::is_always_exhaustive(); }
  static constexpr auto isAlwaysStrided() -> bool { return mapping_type::is_always_strided(); }
  constexpr auto isUnique() const -> bool { return mapping_.is_unique(); }
  constexpr auto isExhaustive() const -> bool { return mapping_.is_exhaustive(); }
  constexpr auto isStrided() const -> bool { return mapping_.is_strided(); }

  constexpr auto container() const -> const container_type& { return container_; }
  constexpr auto extractContainer() && -> container_type { return std::move(container_); }

  template <typename OtherAccessor = std::default_accessor<element_type>>
  constexpr auto toMdspan(const OtherAccessor& a = OtherAccessor())
      -> std::mdspan<element_type, extents_type, layout_type, OtherAccessor> {
    return {container_.data(), mapping_, a};
  }
  template <typename OtherAccessor = std::default_accessor<const element_type>>
  constexpr auto toMdspan(const OtherAccessor& a = OtherAccessor()) const
      -> std::mdspan<const element_type, extents_type, layout_type, OtherAccessor> {
    return {container_.data(), mapping_, a};
  }

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
  constexpr void assignFrom(const Src& src) {
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

  mapping_type mapping_;
  container_type container_;
};

}  // namespace tempura
