#pragma once

#include <cassert>
#include <cstdint>
#include <tuple>
#include <type_traits>
#include <utility>

#include "matrix3/matrix_traits.h"

namespace tempura::matrix3 {

// BlockRow horizontally concatenates matrices into a single logical row.
//
// Example: BlockRow of [2x3] + [2x2] + [2x1] = [2x6]
//   ⎡ A B C | D E | F ⎤
//   ⎣ G H I | J K | L ⎦
//
// All blocks must:
// - Share the same ValueType
// - Have the same number of rows
//
// Element access uses short-circuit fold to route to the correct block.
template <typename First, typename... Rest>
  requires(
      std::is_same_v<typename MatrixTraits<First>::ValueType,
                     typename MatrixTraits<Rest>::ValueType> and
      ...) and ((MatrixTraits<First>::kRows == MatrixTraits<Rest>::kRows) and
                ...)
class BlockRow {
 public:
  using ValueType = typename MatrixTraits<First>::ValueType;

  static constexpr int64_t kRows = MatrixTraits<First>::kRows;
  static constexpr int64_t kCols =
      (MatrixTraits<First>::kCols + ... + MatrixTraits<Rest>::kCols);

  constexpr explicit BlockRow(auto&&... data)
      : data_{std::forward<decltype(data)>(data)...} {}

  // Access element (i, j) by routing to the correct block
  // Uses short-circuit fold to find the block containing column j
  template <typename... Indices>
    requires(sizeof...(Indices) == 2)
  constexpr auto operator[](this auto&& self, Indices... indices)
      -> decltype(auto) {
    auto [i, j] = std::tuple{indices...};
    if (std::is_constant_evaluated()) {
      assert(i >= 0 && i < kRows && "row index out of bounds");
      assert(j >= 0 && j < kCols && "col index out of bounds");
    }
    return std::apply(
        [&](auto&... mats) -> decltype(auto) {
          int64_t offset = 0;
          std::conditional_t<
              std::is_const_v<std::remove_reference_t<decltype(self)>>,
              const ValueType*, ValueType*>
              result = nullptr;
          // Short-circuit fold: try each block, stop when found
          ((j < offset + MatrixTraits<std::remove_cvref_t<decltype(mats)>>::kCols
                ? (result = &mats[i, j - offset], true)
                : (offset += MatrixTraits<std::remove_cvref_t<decltype(mats)>>::kCols, false)) or
           ...);
          assert(result != nullptr && "failed to find block for column index");
          return *result;
        },
        self.data_);
  }

  static constexpr auto rows() -> int64_t { return kRows; }
  static constexpr auto cols() -> int64_t { return kCols; }

 private:
  std::tuple<First, Rest...> data_;
};

// Deduction guide
template <typename... Ts>
BlockRow(Ts...) -> BlockRow<std::remove_cvref_t<Ts>...>;

// BlockCol vertically concatenates matrices into a single logical column.
//
// Example: BlockCol of [2x3] + [1x3] + [2x3] = [5x3]
//   ⎡ A B C ⎤
//   ⎢ D E F ⎥
//   ⎢-------⎥
//   ⎢ G H I ⎥
//   ⎢-------⎥
//   ⎢ J K L ⎥
//   ⎣ M N O ⎦
//
// All blocks must:
// - Share the same ValueType
// - Have the same number of columns
//
// Element access uses short-circuit fold to route to the correct block.
template <typename First, typename... Rest>
  requires(
      std::is_same_v<typename MatrixTraits<First>::ValueType,
                     typename MatrixTraits<Rest>::ValueType> and
      ...) and ((MatrixTraits<First>::kCols == MatrixTraits<Rest>::kCols) and
                ...)
class BlockCol {
 public:
  using ValueType = typename MatrixTraits<First>::ValueType;

  static constexpr int64_t kRows =
      (MatrixTraits<First>::kRows + ... + MatrixTraits<Rest>::kRows);
  static constexpr int64_t kCols = MatrixTraits<First>::kCols;

  constexpr explicit BlockCol(auto&&... data)
      : data_{std::forward<decltype(data)>(data)...} {}

  // Access element (i, j) by routing to the correct block
  // Uses short-circuit fold to find the block containing row i
  template <typename... Indices>
    requires(sizeof...(Indices) == 2)
  constexpr auto operator[](this auto&& self, Indices... indices)
      -> decltype(auto) {
    auto [i, j] = std::tuple{indices...};
    if (std::is_constant_evaluated()) {
      assert(i >= 0 && i < kRows && "row index out of bounds");
      assert(j >= 0 && j < kCols && "col index out of bounds");
    }
    return std::apply(
        [&](auto&... mats) -> decltype(auto) {
          int64_t offset = 0;
          std::conditional_t<
              std::is_const_v<std::remove_reference_t<decltype(self)>>,
              const ValueType*, ValueType*>
              result = nullptr;
          // Short-circuit fold: try each block, stop when found
          ((i < offset + MatrixTraits<std::remove_cvref_t<decltype(mats)>>::kRows
                ? (result = &mats[i - offset, j], true)
                : (offset += MatrixTraits<std::remove_cvref_t<decltype(mats)>>::kRows, false)) or
           ...);
          assert(result != nullptr && "failed to find block for row index");
          return *result;
        },
        self.data_);
  }

  static constexpr auto rows() -> int64_t { return kRows; }
  static constexpr auto cols() -> int64_t { return kCols; }

 private:
  std::tuple<First, Rest...> data_;
};

// Deduction guide
template <typename... Ts>
BlockCol(Ts...) -> BlockCol<std::remove_cvref_t<Ts>...>;

}  // namespace tempura::matrix3
