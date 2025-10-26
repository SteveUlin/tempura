#pragma once

#include <algorithm>
#include <cmath>
#include <concepts>
#include <ranges>

namespace tempura {

// ============================================================================
// Constexpr Comparison Helpers
// ============================================================================
// General-purpose utilities for comparisons that can be used in static_assert,
// library code, and tests. They return bool and have no side effects.

// Absolute tolerance: |a - b| < delta
template <typename T, typename U, typename Delta>
  requires std::floating_point<T> && std::floating_point<U>
constexpr auto isNear(const T& lhs, const U& rhs, const Delta& delta) -> bool {
  return std::abs(lhs - rhs) < delta;
}

// Relative tolerance: |a - b| / max(|a|, |b|, 1) < epsilon
template <typename T, typename U, typename Epsilon>
  requires std::floating_point<T> && std::floating_point<U>
constexpr auto isWithinRel(const T& lhs, const U& rhs,
                            const Epsilon& epsilon) -> bool {
  const auto abs_diff = std::abs(lhs - rhs);
  const auto max_magnitude =
      std::max({std::abs(lhs), std::abs(rhs), T{1}});
  return abs_diff < epsilon * max_magnitude;
}

// Range equality with size checking
template <std::ranges::input_range R1, std::ranges::input_range R2>
  requires std::equality_comparable_with<std::ranges::range_value_t<R1>,
                                         std::ranges::range_value_t<R2>>
constexpr auto rangesEqual(R1&& lhs, R2&& rhs) -> bool {
  if constexpr (std::ranges::sized_range<R1> && std::ranges::sized_range<R2>) {
    if (std::ranges::size(lhs) != std::ranges::size(rhs)) {
      return false;
    }
  }
  return std::ranges::equal(lhs, rhs);
}

// Range approximate equality with absolute tolerance
template <std::ranges::input_range R1, std::ranges::input_range R2,
          typename Delta>
  requires std::floating_point<std::ranges::range_value_t<R1>> &&
           std::floating_point<std::ranges::range_value_t<R2>>
constexpr auto rangesNear(R1&& lhs, R2&& rhs, const Delta& delta) -> bool {
  if constexpr (std::ranges::sized_range<R1> && std::ranges::sized_range<R2>) {
    if (std::ranges::size(lhs) != std::ranges::size(rhs)) {
      return false;
    }
  }

  auto it1 = std::ranges::begin(lhs);
  auto it2 = std::ranges::begin(rhs);
  auto end1 = std::ranges::end(lhs);
  auto end2 = std::ranges::end(rhs);

  while (it1 != end1 && it2 != end2) {
    if (!isNear(*it1, *it2, delta)) {
      return false;
    }
    ++it1;
    ++it2;
  }

  // If one range is longer, they're not equal
  return it1 == end1 && it2 == end2;
}

}  // namespace tempura
