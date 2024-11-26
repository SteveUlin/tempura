#pragma once

#include <array>
#include <cstdint>
#include <utility>

#include "matrix2/matrix.h"

namespace tempura::matrix {

// InlineCoordinateList stores matrix elements in three arrays:
// - Row indicies
// - Column indicies
// - Values
//
// The SortOrder template parameter, as its name implies, determines the order
// in which elements are sorted. The behavior of this class changes depending
// on the SortOrder.
//
// When SortOrder is kNone:
// Elements are stored in the order in which they are added and will always be
// inserted in constant time. Old values are not removed or overwritten, rather
// we read from the back of the array to find the most recently store value for
// that index. You may call `compress()` to remove duplicates.
//
// When SortOrder is kRowMajor or kColMajor:
// Elements are stored in sorted order and will be added in logarithmic time.
// If you insert an element that already exists, the value will be overwritten.
//
// This class assumes that T has a default constructor and we can safely
// allocate a stack std::array of Ts.
// TODO: Use std::inplace_vector when available.
// https://www.open-std.org/jtc1/sc22/wg21/docs/pacpers/2024/p0843r14.html
template <typename T, Extent Row, Extent Col, int64_t Capacity = Row * Col / 4,
          IndexOrder SortOrder = IndexOrder::kNone>
class InlineCoordinateList;

template <typename T, Extent Row, Extent Col, int64_t Capacity>
  requires(Row != kDynamic) and (Col != kDynamic) and (Row > 1) and (Col > 1)
class InlineCoordinateList<T, Row, Col, Capacity, IndexOrder::kNone> {
 public:
  using ValueType = T;
  static constexpr int64_t kRow = Row;
  static constexpr int64_t kCol = Col;

  struct Triplet {
    int64_t i;
    int64_t j;
    T value;
  };

  template <size_t N>
  constexpr InlineCoordinateList(std::array<int64_t, N> row_indicies,
                                 std::array<int64_t, N> col_indicies,
                                 std::array<T, N> values)
      : size_(N),
        row_indicies_{row_indicies},
        col_indicies_{col_indicies},
        values_{values} {}

  // Constructor from a range of triplets.
  constexpr explicit InlineCoordinateList(
      std::ranges::input_range auto&& triplets)
      : InlineCoordinateList() {
    for (auto&& triplet : triplets) {
      insert(triplet);
    }
  }

  constexpr auto operator[](int64_t row, int64_t col) const -> ValueType {
    if (std::is_constant_evaluated()) {
      CHECK(0 <= row and row < kRow);
      CHECK(0 <= col and col < kCol);
    }
    int64_t index = size_;
    while (--index >= 0) {
      if (row_indicies_[index] == row and col_indicies_[index] == col) {
        return values_[index];
      }
    }
    return kZero_;
  }

  constexpr auto shape() const -> RowCol { return {.row = kRow, .col = kCol}; }

  // Insert a new element into the coordinate list.
  constexpr auto insert(Triplet triplet) -> bool {
    if (size_ >= Capacity) {
      return false;
    }
    row_indicies_[size_] = triplet.i;
    col_indicies_[size_] = triplet.j;
    values_[size_] = std::move(triplet.value);
    ++size_;
    return true;
  }

  // Insert a new element into the coordinate list.
  constexpr auto insert(int64_t row, int64_t col, T value) -> bool {
    return insert({row, col, std::move(value)});
  }

  constexpr auto size() const -> int64_t { return size_; }

 private:
  static constexpr T kZero_ = ValueType{};

  int64_t size_ = 0;
  std::array<int64_t, Capacity> row_indicies_ = {};
  std::array<int64_t, Capacity> col_indicies_ = {};
  std::array<T, Capacity> values_ = {};
};
static_assert(MatrixT<InlineCoordinateList<int, 2, 3>>);

// Deduction guides aren't possible since we need to specify the Row and Col
// of the matrix. Instead, we provide a factory function.
template <int64_t Row, int64_t Col, typename T, size_t N>
constexpr auto makeInlineCoordinateList(std::array<int64_t, N> row_indicies,
                                        std::array<int64_t, N> col_indicies,
                                        std::array<T, N> values) {
  return InlineCoordinateList<T, Row, Col, N>{row_indicies, col_indicies,
                                              values};
}

}  // namespace tempura::matrix
