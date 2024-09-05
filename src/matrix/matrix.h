#pragma once

#include <cassert>
#include <cstddef>
#include <cstdlib>
#include <format>
#include <iostream>
#include <limits>
#include <source_location>

namespace tempura::matrix {

void check(bool condition, std::string_view message,
           std::source_location loc = std::source_location::current()) {
  if (!condition) {
    std::cout << std::format("FATAL {}:{}:{}: {}\n", loc.file_name(),
                             loc.line(), loc.column(), message)
              << std::flush;
    std::terminate();
  }
}
#define CHECK(condition) ::tempura::matrix::check(condition, #condition)

// Size of a matrix to be determined at runtime
inline static constexpr size_t kDynamic = std::numeric_limits<size_t>::max();

struct RowCol {
  size_t row;
  size_t col;
  auto operator<=>(const RowCol&) const = default;

  auto operator+=(const RowCol& rhs) -> RowCol& {
    row += rhs.row;
    col += rhs.col;
    return *this;
  }

  auto operator-=(const RowCol& rhs) -> RowCol& {
    row -= rhs.row;
    col -= rhs.col;
    return *this;
  }

  auto operator+(const RowCol& rhs) const -> RowCol {
    return {.row = row + rhs.row, .col = col + rhs.col};
  }

  auto operator-(const RowCol& rhs) const -> RowCol {
    return {.row = row + rhs.row, .col = col + rhs.col};
  }

  auto operator*(int n) const -> RowCol {
    return {.row = n * row, .col = n * col};
  }
};

constexpr auto matchExtent(RowCol lhs, RowCol rhs) -> bool {
  const bool match_row =
      (lhs.row == kDynamic) or (rhs.row == kDynamic) or (lhs.row == rhs.row);
  const bool match_col =
      (lhs.col == kDynamic) or (rhs.col == kDynamic) or (lhs.col == rhs.col);
  return match_row and match_col;
}

enum class IndexOrder {
  kNone,
  kRowMajor,
  kColMajor,
};

enum class Pivoting {
  None,
  Partial
  // Full
};

// Subclasses must define the following functions
// - get(size_t row, size_t col)
//   Return a reference like object to the given index.
//   May return any kind of object for this const version.
// - shapeImpl()
//   Return the current shape of the matrix.
template <RowCol extent, IndexOrder index_order = IndexOrder::kColMajor>
class Matrix {
 public:
  inline static constexpr RowCol kExtent = extent;
  inline static constexpr IndexOrder kIndexOrder = index_order;

  auto shape(this auto&& self) -> RowCol { return self.shapeImpl(); }

  inline auto operator[](this auto&& self, size_t row,
                         size_t col) -> decltype(auto) {
    return self.getImpl(row, col);
  }

  auto get(this auto&& self, RowCol index) -> decltype(auto) {
    return self.getImpl(index.row, index.col);
  }

  // Vector accessors
  auto operator[](this auto&& self, size_t col) -> decltype(auto)
    requires(kExtent.row == 1)
  {
    return self[0, col];
  }

  auto operator[](this auto&& self, size_t row) -> decltype(auto)
    requires(kExtent.col == 1)
  {
    return self[row, 0];
  }

  // Iteration
  auto begin(this auto& self) requires(!std::is_const_v<decltype(self)>){ return self.beginImpl(); }

  auto end(this auto& self) { return self.endImpl(); }

  auto cbegin(this auto&& self) { return self.cbeginImpl(); }

  auto cend(this auto&& self) { return self.cendImpl(); }

  auto rows(this auto&& self) { return self.rowsImpl(); }

  auto cols(this auto&& self) { return self.colsImpl(); }
};

template <typename M>
concept MatrixT = std::derived_from<M, Matrix<M::kExtent>>;

template <typename M, RowCol extent>
concept SizedMatrixT = MatrixT<M> and matchExtent(M::kExtent, extent);

// When copying or moving data from a matrix with dynamic size to one
// with fixed size. We must ensure that the dynamic size matches the
// size in the type.
template <MatrixT M>
constexpr auto verifyShape(const M& m) -> bool {
  const bool row_match =
      (M::kExtent.row == kDynamic) or (M::kExtent.row == m.shape().row);
  const bool col_match =
      (M::kExtent.col == kDynamic) or (M::kExtent.col == m.shape().col);
  return row_match and col_match;
}

template<MatrixT Lhs, MatrixT Rhs>
constexpr auto operator==(const Lhs& lhs, const Rhs& rhs) -> bool {
  if (lhs.shape() != rhs.shape()) {
    return false;
  }
  for (size_t i = 0; i < lhs.shape().row; ++i) {
    for (size_t j = 0; j < lhs.shape().col; ++j) {
      if (lhs[i, j] != rhs[i, j]) {
        return false;
      }
    }
  }
  return true;
}

}  // namespace tempura::matrix
