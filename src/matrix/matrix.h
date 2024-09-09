#pragma once

#include <cassert>
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
inline static constexpr auto kDynamic = std::numeric_limits<int64_t>::max();

struct RowCol {
  // Fit a RowCol into a 64-bit integer for efficient indexing
  //
  // Use signed integers so users can express negative deltas
  int64_t row;
  int64_t col;

  auto operator<=>(const RowCol&) const = default;

  auto operator+=(RowCol rhs) -> RowCol& {
    row += rhs.row;
    col += rhs.col;
    return *this;
  }

  auto operator-=(RowCol rhs) -> RowCol& {
    row -= rhs.row;
    col -= rhs.col;
    return *this;
  }

  auto operator+(RowCol rhs) const -> RowCol {
    return {.row = row + rhs.row, .col = col + rhs.col};
  }

  auto operator-(RowCol rhs) const -> RowCol {
    return {.row = row + rhs.row, .col = col + rhs.col};
  }

  auto operator*(auto n) const -> RowCol {
    return {.row = n * row, .col = n * col};
  }

  auto operator/(auto n) const -> RowCol {
    return {.row = row / n, .col = col / n};
  }
};

auto operator*(auto n, RowCol rhs) -> RowCol {
  return {.row = n * rhs.row, .col = n * rhs.col};
}

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
constexpr auto kNone = IndexOrder::kNone;
constexpr auto kRowMajor = IndexOrder::kRowMajor;
constexpr auto kColMajor = IndexOrder::kColMajor;

enum class Pivoting {
  None,
  Partial
  // Full
};

template <RowCol extent>
class Matrix;

template <typename M>
concept MatrixT = std::derived_from<M, Matrix<M::kExtent>>;

template <MatrixT M>
class Rows;

template <MatrixT M>
class Cols;

// Subclasses must define the following functions
// - get(int64_t row, int64_t col)
//   Return a reference like object to the given index.
//   May return any kind of object for this const version.
// - shapeImpl()
//   Return the current shape of the matrix.
template <RowCol extent>
class Matrix {
 public:
  inline static constexpr RowCol kExtent = extent;

  auto shape(this auto&& self) -> RowCol { return self.shapeImpl(); }

  inline auto operator[](this auto&& self, int64_t row,
                         int64_t col) -> decltype(auto) {
    return self.getImpl(row, col);
  }

  auto get(this auto&& self, RowCol index) -> decltype(auto) {
    return self.getImpl(index.row, index.col);
  }

  // Vector accessors
  auto operator[](this auto&& self, int64_t col) -> decltype(auto)
    requires(kExtent.row == 1)
  {
    return self[0, col];
  }

  auto operator[](this auto&& self, int64_t row) -> decltype(auto)
    requires(kExtent.col == 1)
  {
    return self[row, 0];
  }

  // Iteration
  auto begin(this auto& self)
    requires(!std::is_const_v<decltype(self)>)
  {
    return self.beginImpl();
  }

  auto end(this auto& self) { return self.endImpl(); }

  auto cbegin(this auto&& self) { return self.cbeginImpl(); }

  auto cend(this auto&& self) { return self.cendImpl(); }

  auto rows(this auto&& self) { return Rows(self); }

  auto cols(this auto&& self) { return Cols(self); }
};

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

template <RowCol extent, typename M>
  requires(
      MatrixT<std::remove_cvref_t<M>> and
      (extent.row == kDynamic or std::remove_cvref_t<M>::kExtnet == kDynamic or
       extent.row <= std::remove_cvref_t<M>::kExtent.row) and
      (extent.col == kDynamic or std::remove_cvref_t<M>::kExtnet == kDynamic or
       extent.col <= std::remove_cvref_t<M>::kExtent.col))
class Slice : public Matrix<extent> {
 public:
  using MatT = std::remove_cvref_t<M>;

  Slice(RowCol offset, M&& mat)
    requires((extent.row != kDynamic) and (extent.col != kDynamic))
      : shape_(extent), offset_(offset), parent_(std::forward<M>(mat)) {}

  Slice(RowCol shape, RowCol offset, M&& mat)
      : shape_(shape), offset_(offset), parent_(mat) {
    CHECK(verifyShape(*this));
  }

  template <RowCol other_extent>
  Slice(RowCol offset, Slice<other_extent, M>& other)
    requires((extent.row != kDynamic) and (extent.col != kDynamic))
      : shape_(extent),
        offset_(offset + other.offset()),
        parent_(other.parent()) {}

  auto parent() -> M& { return parent_; }

  auto offset() const -> RowCol { return offset_; }

  template <typename ParentIterT>
  class Iterator {
   public:
    explicit Iterator(ParentIterT parent_iter) : parent_iter_(parent_iter) {}

    auto operator*() -> decltype(auto) { return *parent_iter_; }

    auto operator++() -> Iterator& requires(extent.row == 1) {
      parent_iter_.incCol();
      return *this;
    }

    auto operator++() -> Iterator& requires(extent.col == 1) {
      parent_iter_.incRow();
      return *this;
    }

    auto operator++() -> Iterator& {
      parent_iter_.incRow();
      return *this;
    }

    auto operator<=>(const Iterator& iter) {
      return parent_iter_ <=> iter.parent_iter_;
    }

    auto operator!=(const Iterator& iter) {
      return parent_iter_ != iter.parent_iter_;
    }

   private:
    ParentIterT parent_iter_;
  };

  auto beginImpl(this auto&& self) {
    return Iterator{typename MatT::Iterator{self.parent_, self.offset_}};
  }

  auto endImpl(this auto&& self) {
    if constexpr (extent.row == 1) {
      return Iterator{typename MatT::Iterator{
          self.parent_, self.offset_ + RowCol{0, self.shape_.col}}};
    } else if (extent.col == 1) {
      return Iterator{typename MatT::Iterator{
          self.parent_, self.offset_ + RowCol{self.shape_.row, 0}}};
    }
  }

 private:
  friend class Matrix<extent>;

  auto getImpl(this auto&& self, int64_t row, int64_t col) -> decltype(auto) {
    return self.parent_[(row + self.offset_.row), (col + self.offset_.col)];
  }

  auto shapeImpl(this auto&& self) -> RowCol { return self.shape_; }

  RowCol shape_;
  RowCol offset_;
  M parent_;
};

template <typename M>
Slice(RowCol shape, RowCol offset, M&& mat) -> Slice<{kDynamic, kDynamic}, M>;

template <RowCol extent>
struct Slicer {
  template <RowCol other_extent, MatrixT M>
  static auto at(RowCol offset,
                 Slice<other_extent, M>& mat) {
    return Slice{offset, mat};
  }
  template <typename M>
  static auto at(RowCol offset, M&& mat) {
    return Slice<extent, M>{offset, std::forward<M>(mat)};
  }
};

template <MatrixT M>
class Rows {
 public:
  Rows(M& matrix) : matrix_(matrix) {}

  auto operator[](int64_t index) {
    return Slicer<{1, M::kExtent.col}>::at(RowCol{index, 0}, matrix_);
  }

  class Iterator {
   public:
    Iterator(int64_t index, M& parent) : index_(index), matrix_(parent) {}

    auto operator*() {
      return Slicer<{1, M::kExtent.col}>::at(RowCol{index_, 0}, matrix_);
    }

    auto operator++() -> Iterator& {
      index_++;
      return *this;
    }

    auto operator--() -> Iterator& {
      index_--;
      return *this;
    }

    auto operator!=(const Iterator& iter) { return index_ != iter.index_; }

   private:
    int64_t index_;
    M& matrix_;
  };

  auto begin(this auto&& self) { return Iterator{0, self.matrix_}; }
  auto end(this auto&& self) {
    return Iterator{self.matrix_.shape().row, self.matrix_};
  }

 private:
  M& matrix_;
};

template <MatrixT M>
class Cols {
 public:
  class Iterator {
   public:
    Iterator() = default;
    Iterator(const Iterator&) = default;
    Iterator(Iterator&&) = default;
    auto operator=(const Iterator&) -> Iterator& = default;
    auto operator=(Iterator&&) -> Iterator& = default;

    Iterator(int64_t index, M& parent) : index_(index), matrix_(parent) {}

    auto operator*() {
      return Slicer<{M::kExtent.row, 1}>::at(RowCol{0, index_}, matrix_);
    }
    auto operator++() -> Iterator& {
      index_++;
      return *this;
    }
    auto operator--() -> Iterator& {
      index_--;
      return *this;
    }

    auto operator!=(const Iterator& iter) { return index_ != iter.index_; }

   private:
    int64_t index_;
    M& matrix_;
  };

  Cols(M& matrix) : matrix_(matrix) {}

  auto begin() -> Iterator { return Iterator{0, matrix_}; }
  auto end() -> Iterator { return Iterator{matrix_.shape().col, matrix_}; }

 private:
  M& matrix_;
};

template <MatrixT Lhs, MatrixT Rhs>
constexpr auto operator==(const Lhs& lhs, const Rhs& rhs) -> bool {
  if (lhs.shape() != rhs.shape()) {
    return false;
  }
  for (int64_t i = 0; i < lhs.shape().row; ++i) {
    for (int64_t j = 0; j < lhs.shape().col; ++j) {
      if (lhs[i, j] != rhs[i, j]) {
        return false;
      }
    }
  }
  return true;
}

}  // namespace tempura::matrix
