#pragma once

#include <cassert>
#include <compare>
#include <concepts>
#include <cstdint>
#include <print>
#include <source_location>
#include <string_view>
#include <type_traits>

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

struct DynamicExtent {};
inline static constexpr auto kDynamic = DynamicExtent{};

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
  requires std::same_as<decltype(T::kRow), const int64_t> or
               std::same_as<decltype(T::kRow), const DynamicExtent>;
  requires std::same_as<decltype(T::kCol), const int64_t> or
               std::same_as<decltype(T::kCol), const DynamicExtent>;
};

template <typename T, typename U>
concept MatchingExtent =
    (std::same_as<decltype(T::kRow), const DynamicExtent> or
     std::same_as<decltype(U::kRow), const DynamicExtent> or
     T::kRow == U::kRow) and
    (std::same_as<decltype(T::kCol), const DynamicExtent> or
     std::same_as<decltype(U::kCol), const DynamicExtent> or
     T::kCol == U::kCol);

template <typename T>
concept HasDynamicExtent =
    std::same_as<decltype(T::kRow), const DynamicExtent> or
    std::same_as<decltype(T::kCol), const DynamicExtent>;

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

}  // namespace tempura::matrix
