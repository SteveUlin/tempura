#pragma once

#include <concepts>
#include <cstddef>
#include <mdspan>
#include <span>

namespace tempura {

// Shorthand for a runtime extent, so a dynamic axis reads as `Dense<double, Dyn, Dyn>`
// rather than spelling std::dynamic_extent at each one.
inline constexpr std::size_t Dyn = std::dynamic_extent;

// Build a shape from its lengths: `dims(3, 4)` is the 3×4 shape handed to a shaped ctor —
// the explicit "these are dimensions, not values" marker (numpy's `(3, 4)` tuple). Always
// all-dynamic; a fixed-extent owner validates its static axes when it adopts the shape.
template <std::integral... Lengths>
constexpr auto dims(Lengths... lengths) {
  return std::dextents<std::size_t, sizeof...(Lengths)>{static_cast<std::size_t>(lengths)...};
}

}  // namespace tempura
