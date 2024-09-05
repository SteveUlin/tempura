#pragma once
#include "matrix/matrix.h"

namespace tempura::matrix {

template <RowCol extent, MatrixT M>
  requires((extent.row == kDynamic or M::extnet == kDynamic or
            extent.row <= M::kExtent.row) and
           (extent.col == kDynamic or M::extnet == kDynamic or
            extent.col <= M::kExtent.col))
class Slice : public Matrix<extent> {
 public:
  using ScalarT = M::ScalarT;
  Slice(RowCol offset, M& mat)
    requires((extent.row != kDynamic) and (extent.col != kDynamic))
      : shape_(extent), offset_(offset), parent_(mat) {}

  Slice(RowCol shape, RowCol offset, M& mat)
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

    auto operator++() -> Iterator& requires (extent.row == 1) {
        parent_iter_.incCol();
        return *this;
    }

    auto operator++() -> Iterator& requires (extent.col == 1) {
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
    return Iterator{typename M::Iterator{self.parent_, self.offset_}};
  }

  auto endImpl(this auto&& self) {
    if constexpr (extent.row == 1) {
      return Iterator{typename M::Iterator{
          self.parent_, self.offset_ + RowCol{0, self.shape_.col}}};
    } else if (extent.col == 1) {
      return Iterator{typename M::Iterator{
          self.parent_, self.offset_ + RowCol{self.shape_.row, 0}}};
    }
  }

 private:
  friend class Matrix<extent>;

  auto getImpl(this auto&& self, size_t row, size_t col) -> decltype(auto) {
    return self.parent_[(row + self.offset_.row), (col + self.offset_.col)];
  }

  auto shapeImpl(this auto&& self) -> RowCol { return self.shape_; }

  RowCol shape_;
  RowCol offset_;
  M& parent_;
};
template <MatrixT M>
Slice(RowCol shape, RowCol offset, M& mat) -> Slice<{kDynamic, kDynamic}, M>;

template <RowCol extent>
struct Slicer {
  template <RowCol other_extent, MatrixT M>
  static auto at(RowCol offset,
                 Slice<other_extent, M>& mat) -> Slice<extent, M> {
    return Slice<extent, M>{offset, mat};
  }
  template <MatrixT M>
  static auto at(RowCol offset, M& mat) -> Slice<extent, M> {
    return Slice<extent, M>{offset, mat};
  }
};

template <MatrixT M>
class IterRows {
 public:
  IterRows(M& matrix) : matrix_(matrix) {}

  class Iterator {
   public:
    Iterator(size_t index, M& parent) : index_(index), matrix_(parent) {}

    auto operator*() -> Slice<{1, M::kExtent.col}, M> {
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
    size_t index_;
    M& matrix_;
  };

  auto begin(this auto&& self) {
    return Iterator{0, self.matrix_};
  }
  auto end(this auto&& self) {
    return Iterator{self.matrix_.shape().row, self.matrix_};
  }

 private:
  M& matrix_;
};

template <MatrixT M>
class IterCols {
 public:
  class Iterator {
   public:
    Iterator() = default;
    Iterator(const Iterator&) = default;
    Iterator(Iterator&&) = default;
    auto operator=(const Iterator&) -> Iterator& = default;
    auto operator=(Iterator&&) -> Iterator& = default;

    Iterator(size_t index, M& parent) : index_(index), matrix_(parent) {}

    auto operator*() -> Slice<{M::kExtent.row, 1}, M> {
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
    size_t index_;
    M& matrix_;
  };

  IterCols(M& matrix) : matrix_(matrix) {}

  auto begin() -> Iterator { return Iterator{0, matrix_}; }
  auto end() -> Iterator { return Iterator{matrix_.shape().col, matrix_}; }

 private:
  M& matrix_;
};

}  // namespace tempura::matrix
