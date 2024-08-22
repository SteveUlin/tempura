#pragma once
#include "matrix/matrix.h"

namespace tempura::matrix {

class IteratorOpMixin {
public:
  auto operator++(this auto&& self, int) {
    auto copy = self;
    ++self;
    return copy;
  }
  auto operator--(this auto&& self, int) {
    auto copy = self;
    --self;
    return copy;
  }
  auto operator<=>(this const auto& self, const decltype(self)& iter) {
    return self.index_ <=> iter.index_;
  }
  auto operator!=(this const auto& self, const decltype(self)& iter) {
    return !(self.index_ == iter.index_);
  }
};

template <MatrixT M>
class IterElements {
public:
  class Iterator : public IteratorOpMixin {
  public:
    Iterator() = default;
    Iterator(const Iterator&) = default;
    Iterator(Iterator&&) = default;
    auto operator=(const Iterator&) -> Iterator& = default;
    auto operator=(Iterator&&) -> Iterator& = default;

    Iterator(RowCol index, M& parent) : index_(index), matrix_(parent) {}

    auto operator*() -> decltype(auto) {
      return matrix_.get(index_);
    }
    auto operator++() -> Iterator& {
      if (++index_.col == matrix_.shape().col) {
        ++index_.row;
        index_.col = 0;
      }
      return *this;
    }
    auto operator--() -> Iterator& {
      if (--index_.col == -1) {
        --index_.row;
        index_.col = matrix_.shape().row - 1;
      }
      return *this;
    }
  private:
    friend class IteratorOpMixin;
    RowCol index_;
    M& matrix_;
  };

  IterElements(M& parent) : parent_(parent) {}

  auto begin() -> Iterator {
    return Iterator({0, 0}, parent_);
  }

  auto end() -> Iterator {
    return Iterator({parent_.shape().row, 0}, parent_);
  }

private:
  M& parent_;
};

template <RowCol extent, MatrixT M>
  requires((extent.row == kDynamic or M::extnet == kDynamic or
            extent.row <= M::kExtent.row) and
           (extent.col == kDynamic or M::extnet == kDynamic or
            extent.col <= M::kExtent.col))
class Slice : public Matrix<extent> {
 public:
  Slice(RowCol offset, M& mat)
    requires((extent.row != kDynamic) and (extent.col != kDynamic))
      : shape_(extent), offset_(offset), parent_(mat) {}

  Slice(RowCol shape, RowCol offset, M&& mat)
      : shape_(shape), offset_(offset), parent_(mat) {
    CHECK(verifyShape(*this));
  }

 private:
  friend class Matrix<extent>;

  auto getImpl(this auto&& self, RowCol index) -> decltype(auto) {
    return self.parent_.get(index + self.offset_);
  }

  auto shapeImpl(this auto&& self) -> RowCol {
    return self.shape_;
  }

  RowCol shape_;
  RowCol offset_;
  M& parent_;
};

template <RowCol extent>
struct Slicer {
  template <MatrixT M>
  static auto at(RowCol offset, M& mat) -> Slice<extent, M> {
    return Slice<extent, M>{offset, mat};
  }
};

template <MatrixT M>
class IterRows {
public:
  class Iterator : public IteratorOpMixin {
  public:
    Iterator() = default;
    Iterator(const Iterator&) = default;
    Iterator(Iterator&&) = default;
    auto operator=(const Iterator&) -> Iterator& = default;
    auto operator=(Iterator&&) -> Iterator& = default;

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
  private:
    friend class IteratorOpMixin;

    size_t index_;
    M& matrix_;
  };

  IterRows(M& matrix) : matrix_(matrix) {}

  auto begin() -> Iterator {
    return Iterator{0, matrix_};
  }
  auto end() -> Iterator {
    return Iterator{matrix_.shape().row, matrix_};
  }
private:
  M& matrix_;
};

template <MatrixT M>
class IterCols {
public:
  class Iterator : public IteratorOpMixin {
  public:
    Iterator() = default;
    Iterator(const Iterator&) = default;
    Iterator(Iterator&&) = default;
    auto operator=(const Iterator&) -> Iterator& = default;
    auto operator=(Iterator&&) -> Iterator& = default;

    Iterator(size_t index, M& parent) : index_(index), matrix_(parent) {}

    auto operator*() -> Slice<{M::kExtent.row, 1}, M> {
      return Slicer<{ M::kExtent.row, 1}>::at(RowCol{0, index_}, matrix_);
    }
    auto operator++() -> Iterator& {
      index_++;
      return *this;
    }
    auto operator--() -> Iterator& {
      index_--;
      return *this;
    }
  private:
    friend class IteratorOpMixin;

    size_t index_;
    M& matrix_;
  };

  IterCols(M& matrix) : matrix_(matrix) {}

  auto begin() -> Iterator {
    return Iterator{0, matrix_};
  }
  auto end() -> Iterator {
    return Iterator{matrix_.shape().col, matrix_};
  }
private:
  M& matrix_;
};

}  // namespace tempura::matrix
