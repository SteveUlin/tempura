#pragma once

#include <array>
#include <cassert>
#include <cstddef>
#include <limits>
#include <span>
#include <type_traits>

// GCC libstdc++ does not support <mdspan> yet. The following is based on
// mdspan but is:
//   - simplified
//   - explicitly supports matrix owning their underlying data
//   - uses the c++26 multidimensional bracket notation
//   - uses Google naming conventions
//
// The goal here is not to recreate mdspan but to provide a simple interface for
// exploring matrix algebra.

namespace tempura::matrix3 {

inline std::size_t constexpr kDynamic = std::numeric_limits<std::size_t>::max();

template <typename IndexTypeT, std::size_t... Ns>
class Extents {
 public:
  using IndexType = IndexTypeT;
  using SizeType = std::make_unsigned_t<IndexType>;

  constexpr Extents() = default;

  // Extents(other);
  //
  // Extents(other) is marked as explicit if either:
  //   - There is a reduction in IndexTypeT's max value
  //   - A dynamic extent is converted to a static extent
  //
  // The behavior is undefined if:
  //  extent(r) != kDynamic && other.extent(r) != extent(r)
  template <typename OtherIndexTypeT, std::size_t... OtherNs>
    requires((sizeof...(Ns) == sizeof...(OtherNs)) &&
             ((Ns == OtherNs || Ns == kDynamic || OtherNs == kDynamic) && ...))
  constexpr explicit(((Ns != kDynamic && OtherNs == kDynamic) || ...) ||
                     (std::numeric_limits<IndexType>::max() <
                      std::numeric_limits<OtherIndexTypeT>::max()))
      Extents(const Extents<OtherIndexTypeT, OtherNs...>& other) noexcept {
    size_t index = 0;
    for (int i = 0; i < rank(); ++i) {
      if (static_extent(i) == kDynamic) {
        extents_[index++] = other.extent(i);
      }
    }
  }

  // Extents(a, b, c)
  //
  // Extents(a, b, c) is either:
  //   - a list of all dynamic extents
  //   - a list of all extents, if there are any static extents, they must
  //     match the static extents of the current object otherwise it is
  //     undefined behavior
  template <typename... OtherIndexTypes>
    requires((sizeof...(Ns) > 0) &&
             ((sizeof...(OtherIndexTypes) == sizeof...(Ns)) ||
              (sizeof...(OtherIndexTypes) == (0 + ... + (Ns == kDynamic)))) &&
             (std::is_convertible_v<OtherIndexTypes, IndexTypeT> && ...) &&
             (std::is_nothrow_constructible_v<OtherIndexTypes, IndexTypeT> &&
              ...))
  constexpr explicit Extents(OtherIndexTypes... args) noexcept {
    if constexpr (sizeof...(args) == rank()) {
      size_t index = 0;
#pragma clang diagnostic ignored "-Wunused-value"
      (((Ns == kDynamic) ? (extents_[index++] = args, true) : false), ...);
    } else {
      extents_ = {args...};
    }
  }

  // Returns the number of dimensions
  static constexpr auto rank() -> std::size_t { return sizeof...(Ns); }

  // Returns the number of dimensions that are dynamic
  static constexpr auto rank_dynamic() -> std::size_t {
    return (std::size_t{0} + ... + (Ns == kDynamic ? 1 : 0));
  }

  // Returns the extent of the dimension i as known at compile time
  static constexpr auto static_extent(std::size_t i) -> std::size_t {
    assert(i < rank());
    std::size_t ans;

#pragma clang diagnostic ignored "-Wunused-value"
    (((i-- == 0) ? (ans = Ns, true) : false) || ...);
    return ans;
  }

  // Returns the extent of the dimension i as known at runtime
  constexpr auto extent(size_t i) const -> IndexType {
    assert(i < rank());
    std::size_t dynamic_index = 0;
    std::size_t ans;

#pragma clang diagnostic ignored "-Wunused-value"
    (((i-- == 0) ? (ans = Ns, true)
                 : (dynamic_index += static_cast<std::size_t>(Ns == kDynamic),
                    false)) ||
     ...);
    if (ans == kDynamic) ans = extents_[dynamic_index];
    return ans;
  }

 private:
  std::array<IndexType, rank_dynamic()> extents_ = {};
};

}  // namespace tempura::matrix3
