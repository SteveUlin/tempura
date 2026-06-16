#pragma once

#include <algorithm>
#include <bit>
#include <cmath>
#include <concepts>
#include <cstdint>
#include <limits>
#include <ranges>

namespace tempura {

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

constexpr auto isNan(double x) -> bool { return x != x; }
constexpr auto isInf(double x) -> bool {
  return x == std::numeric_limits<double>::infinity() ||
         x == -std::numeric_limits<double>::infinity();
}
constexpr auto isFinite(double x) -> bool { return !isNan(x) && !isInf(x); }

// Compare against whichever tolerance is LESS strict.
struct Tolerance {
  // Relative Tolerance
  double rtol = 0.0;
  // Absolute Tolerance
  double atol = 0.0;
};

// |a - b| <= max(atol, rtol * max(|a|, |b|)).
// Non-finite values are close only when exactly equal (same-sign infinity), so
// opposite infinities and any NaN are never close.
template <std::floating_point T, std::floating_point U>
constexpr auto isClose(const T& lhs, const U& rhs, Tolerance tol) -> bool {
  if (lhs == rhs) {
    return true;
  }
  if (!isFinite(lhs) || !isFinite(rhs)) {
    return false;
  }
  const auto diff = std::abs(lhs - rhs);
  const auto scale = std::max(std::abs(lhs), std::abs(rhs));
  return diff <= std::max(tol.atol, tol.rtol * scale);
}

// Map a double to a sign-magnitude-corrected integer so adjacent representables
// differ by 1 — the basis for ULP distance. The ordering is total across the
// sign boundary.
constexpr auto orderedKey(double x) -> std::uint64_t {
  const auto bits = std::bit_cast<std::uint64_t>(x);
  constexpr std::uint64_t sign_bit = 0x8000000000000000ULL;
  return (bits & sign_bit) != 0 ? ~bits : (bits | sign_bit);
}

// Distance in units-in-the-last-place: 0 for equal values, 1 for adjacent
// representables. The "as accurate as the hardware allows" check for
// well-conditioned closed forms. NaN inputs yield a meaningless result —
// screen with isNan first.
constexpr auto ulpDistance(double a, double b) -> std::uint64_t {
  if (a == b) {
    return 0;  // exact equality, including +0.0 == -0.0
  }
  const auto ka = orderedKey(a);
  const auto kb = orderedKey(b);
  return ka > kb ? ka - kb : kb - ka;
}

// Range closeness with size checking, element-wise isClose.
template <std::ranges::input_range R1, std::ranges::input_range R2>
  requires std::floating_point<std::ranges::range_value_t<R1>> &&
           std::floating_point<std::ranges::range_value_t<R2>>
constexpr auto rangesClose(R1&& lhs, R2&& rhs, Tolerance tol) -> bool {
  if constexpr (std::ranges::sized_range<R1> && std::ranges::sized_range<R2>) {
    if (std::ranges::size(lhs) != std::ranges::size(rhs)) {
      return false;
    }
  }
  auto it1 = std::ranges::begin(lhs);
  auto it2 = std::ranges::begin(rhs);
  const auto end1 = std::ranges::end(lhs);
  const auto end2 = std::ranges::end(rhs);
  while (it1 != end1 && it2 != end2) {
    if (!isClose(*it1, *it2, tol)) {
      return false;
    }
    ++it1;
    ++it2;
  }
  return it1 == end1 && it2 == end2;
}

}  // namespace tempura
