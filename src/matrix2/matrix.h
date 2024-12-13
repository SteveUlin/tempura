#pragma once

#include <cassert>
#include <compare>
#include <concepts>
#include <cstdint>
#include <print>
#include <source_location>
#include <string_view>

namespace tempura::matrix {

constexpr void check(
    bool condition, std::string_view message,
    std::source_location loc = std::source_location::current()) {
  if (!condition) [[unlikely]] {
    std::print("FATAL {}:{}:{}: {}\n", loc.file_name(), loc.line(),
               loc.column(), message);
    std::terminate();
  }
}
#define CHECK(condition) ::tempura::matrix::check(condition, #condition)

// Extent represents the size of a dimension known at compile time.
using Extent = int64_t;
// Set to the max value. This simplifies validation as the following is always
// true for a valid index 0 <= index <= Extent;
inline static constexpr Extent kDynamic = std::numeric_limits<Extent>::max();

struct RowCol {
  int64_t row;
  int64_t col;

  constexpr auto operator<=>(const RowCol&) const = default;

  constexpr auto operator+=(RowCol rhs) -> RowCol& {
    row += rhs.row;
    col += rhs.col;
    return *this;
  }

  constexpr auto operator-=(RowCol rhs) -> RowCol& {
    row -= rhs.row;
    col -= rhs.col;
    return *this;
  }

  constexpr auto operator+(RowCol rhs) const -> RowCol {
    return {.row = row + rhs.row, .col = col + rhs.col};
  }

  constexpr auto operator-(RowCol rhs) const -> RowCol {
    return {.row = row + rhs.row, .col = col + rhs.col};
  }

  constexpr auto operator*(auto n) const -> RowCol {
    return {.row = n * row, .col = n * col};
  }

  constexpr auto operator/(auto n) const -> RowCol {
    return {.row = row / n, .col = col / n};
  }
};

constexpr auto operator*(auto n, RowCol rhs) -> RowCol {
  return {.row = n * rhs.row, .col = n * rhs.col};
}

enum class IndexOrder : uint8_t {
  kNone,
  kRowMajor,
  kColMajor,
};
inline static constexpr auto kRowMajor = IndexOrder::kRowMajor;
inline static constexpr auto kColMajor = IndexOrder::kColMajor;

template <typename T>
concept MatrixT = requires(T matrix, int64_t row, int64_t col) {
  { matrix[row, col] } -> std::convertible_to<typename T::ValueType>;
  { matrix.shape() } -> std::same_as<RowCol>;
  requires std::same_as<decltype(T::kRow), const Extent>;
  requires std::same_as<decltype(T::kCol), const Extent>;
};

template <typename T, typename U>
concept MatchingExtent =
    (T::kRow == U::kRow or T::kRow == kDynamic or U::kRow == kDynamic) and
    (T::kCol == U::kCol or T::kCol == kDynamic or U::kCol == kDynamic);

template <typename T>
concept HasDynamicExtent = (T::kRow == kDynamic or T::kCol == kDynamic);

// Checks the shape field if there are dynamic extents
//
// This also imposes a constraint that the extents must match, so technically
// you don't need to explicitly add the MatchingExtent requires clause to
// functions that call this check as it will be applied implicitly.
template <typename Lhs, typename Rhs>
  requires MatchingExtent<Lhs, Rhs>
constexpr void checkSameShape(const Lhs& lhs, const Rhs& rhs) {
  if constexpr (HasDynamicExtent<Lhs> or HasDynamicExtent<Rhs>) {
    CHECK(lhs.shape() == rhs.shape());
  }
}

// Default Equality
template <MatrixT Lhs, MatrixT Rhs>
constexpr auto operator==(const Lhs& lhs, const Rhs& rhs) -> bool {
  checkSameShape(lhs, rhs);
  for (int64_t row = 0; row < lhs.shape().row; ++row) {
    for (int64_t col = 0; col < lhs.shape().col; ++col) {
      if (lhs[row, col] != rhs[row, col]) {
        return false;
      }
    }
  }
  return true;
}

template <auto delta = 0.0001, MatrixT Lhs, MatrixT Rhs>
  requires MatchingExtent<Lhs, Rhs>
constexpr auto approxEqual(const Lhs& lhs, const Rhs& rhs) -> bool {
  checkSameShape(lhs, rhs);
  for (int64_t row = 0; row < lhs.shape().row; ++row) {
    for (int64_t col = 0; col < lhs.shape().col; ++col) {
      if (std::abs(lhs[row, col] - rhs[row, col]) > delta) {
        return false;
      }
    }
  }
  return true;
}

enum class Pivot : uint8_t {
  kNone,
  kRow,
  kRowImplicit,
  kFull,
};

}  // namespace tempura::matrix
