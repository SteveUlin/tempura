#pragma once

#include <concepts>
#include <type_traits>

namespace tempura {

// Membership is opt-in, not inferred: `int` has +−×/ but `int/int` truncates,
// and a requires-clause can't tell a field inverse from integer division.
// Floating-point built-ins qualify; other number-likes specialize this to true.
template <typename T>
inline constexpr bool enable_scalar = std::floating_point<T>;

// Ordered by default — most numerical methods branch (convergence, pivoting).
template <typename T>
concept Scalar = enable_scalar<std::remove_cvref_t<T>> && std::totally_ordered<T> &&
    requires(T a, T b) {
      { a + b } -> std::convertible_to<T>;
      { a - b } -> std::convertible_to<T>;
      { a * b } -> std::convertible_to<T>;
      { a / b } -> std::convertible_to<T>;
    };

}  // namespace tempura
