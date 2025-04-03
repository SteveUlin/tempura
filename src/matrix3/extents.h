#pragma once

#include <array>
#include <cassert>
#include <limits>
#include <type_traits>

namespace tempura::matrix3 {

// Indicate that the dimension is only known at runtime.
inline std::size_t constexpr kDynamic = std::numeric_limits<std::size_t>::max();

// A multidimensional extent is a tuple of dimensions.The dimensions can be
// either static or dynamic. A static dimension is known at compile time and
// cannot be changed. A dynamic dimension is known at runtime on object
// construction.
template <typename IndexT, std::size_t... Ns>
class Extents {
 public:
  using IndexType = IndexT;

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
    std::size_t index = 0;
    for (int i = 0; i < rank(); ++i) {
      if (staticExtent(i) == kDynamic) {
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
             (std::is_convertible_v<OtherIndexTypes, IndexType> && ...) &&
             (std::is_nothrow_constructible_v<OtherIndexTypes, IndexType> &&
              ...))
  constexpr explicit Extents(OtherIndexTypes... args) noexcept {
    if constexpr (sizeof...(args) == rank()) {
      std::size_t index = 0;
#pragma clang diagnostic ignored "-Wunused-value"
      (((Ns == kDynamic) ? (extents_[index++] = args, true) : false), ...);
    } else {
      extents_ = {args...};
    }
  }

  // Returns the number of dimensions
  static constexpr auto rank() -> std::size_t { return sizeof...(Ns); }

  // Returns the number of dimensions that are dynamic
  static constexpr auto rankDynamic() -> std::size_t {
    return (std::size_t{0} + ... + (Ns == kDynamic ? 1 : 0));
  }

  // Returns the extent of the dimension i as known at compile time
  static constexpr auto staticExtent(std::size_t i) -> std::size_t {
    assert(i < rank());
    std::size_t ans;

#pragma clang diagnostic ignored "-Wunused-value"
    (((i-- == 0) ? (ans = Ns, true) : false) || ...);
    return ans;
  }

  // Returns the extent of the dimension i as known at runtime
  constexpr auto extent(std::size_t i) const -> IndexType {
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
  std::array<IndexType, rankDynamic()> extents_ = {};
};

template <typename T, template <typename, std::size_t...> typename U>
concept IsSpecializationOf =
    std::invocable<decltype([]<typename IndexTypeT, std::size_t... Ns>(
                                U<IndexTypeT, Ns...> const&) {}),
                   T>;

template <typename V>
concept IsExtentsT = IsSpecializationOf<V, Extents>;

}  // namespace tempura::matrix3
