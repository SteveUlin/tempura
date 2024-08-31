#pragma once

#include <utility>
#include <vector>

#include "matrix/matrix.h"
#include "matrix/slice.h"

namespace tempura::matrix {

template <typename Scalar, RowCol extent,
          IndexOrder order = IndexOrder::kColMajor>
class Dense final : public Matrix<extent> {
 public:
  using ScalarT = Scalar;

  Dense()
      : shape_({.row = (extent.row == kDynamic) ? 0 : extent.row,
                .col = (extent.col == kDynamic) ? 0 : extent.row}),
        data_(shape_.row * shape_.col) {}

  Dense(const Dense& other)
      : shape_{.row = other.shape().row, .col = other.shape().col},
        data_(other.data_) {}

  Dense(Dense&& other) noexcept
      : shape_{.row = other.shape().row, .col = other.shape().col},
        data_(std::move(other.data_)) {}

  auto operator=(const Dense& other) -> Dense& {
    shape_ = other.shape_;
    data_ = other.data_;
    return *this;
  }

  auto operator=(Dense&& other) noexcept -> Dense& {
    shape_ = other.shape_;
    data_ = std::move(other.data_);
    return *this;
  }

  template <RowCol other_extent>
    requires(matchExtent(extent, other_extent))
  Dense(const Dense<Scalar, other_extent, order>& other)
      : shape_{other.shape()}, data_(other.data()) {
    CHECK(verifyShape(*this));
  }
  template <RowCol other_extent>
    requires(matchExtent(extent, other_extent))
  Dense(Dense<Scalar, other_extent, order>&& other)
      : shape_{other.shape()}, data_(other.data()) {
    CHECK(verifyShape(*this));
  }

  template <RowCol other_extent>
    requires(matchExtent(extent, other_extent))
  auto operator=(const Dense<Scalar, other_extent, order>& other) -> Dense& {
    shape_ = other.shape();
    data_ = other.data();
    CHECK(verifyShape(*this));
    return *this;
  }

  template <RowCol other_extent>
    requires(matchExtent(extent, other_extent))
  auto operator=(Dense<Scalar, other_extent, order>&& other) -> Dense& {
    shape_ = other.shape();
    data_ = std::move(other.data());
    CHECK(verifyShape(*this));
    return *this;
  }

  // Array initialization
  // auto m = matrix::Dense{{0., 1.}, {2., 3.}};
  //
  // Use C-Arrays to get the nice syntax
  // NOLINTBEGIN(*-avoid-c-arrays)
  template <size_t First, size_t... Sizes>
    requires(((extent.row == kDynamic) or
              (sizeof...(Sizes) + 1 == extent.row)) and
             ((extent.col == kDynamic) or (First == extent.col)) and
             ((First == Sizes) and ...))
  Dense(const Scalar (&first)[First], const Scalar (&... rows)[Sizes])
      : shape_{.row = (sizeof...(Sizes) + 1), .col = First},
        data_(shape_.row * shape_.col) {
    for (size_t j = 0; j < First; ++j) {
      (*this)[0, j] = first[j];
    }
    size_t i = 1;
    (
        [this, &rows, &i]() {
          for (size_t j = 0; j < shape_.row; ++j) {
            (*this)[i, j] = rows[j];
          }
          ++i;
        }(),
        ...);
  }
  // NOLINTEND(*-avoid-c-arrays)

  Dense(const SizedMatrixT<extent> auto& other)
      : shape_{other.shape()}, data_(shape_.row * shape_.col) {
    CHECK(verifyShape(*this));
    for (size_t i = 0; i < shape_.row; ++i) {
      for (size_t j = 0; j < shape_.col; ++j) {
        (*this)[i, j] = other[i, j];
      }
    }
  }

  Dense(SizedMatrixT<extent> auto&& other)
      : shape_{other.shape()}, data_(shape_.row * shape_.col) {
    CHECK(verifyShape(*this));
    for (size_t i = 0; i < shape_.row; ++i) {
      for (size_t j = 0; j < shape_.col; ++j) {
        (*this)[i, j] = std::move(other[i, j]);
      }
    }
  }

  auto operator=(const SizedMatrixT<extent> auto& other) -> Dense& {
    shape_ = other.shape();
    data_.resize(shape_.row * shape_.col);
    CHECK(verifyShape(*this));
    for (size_t i = 0; i < shape_.row; ++i) {
      for (size_t j = 0; j < shape_.col; ++j) {
        (*this)[i, j] = other[i, j];
      }
    }
    return *this;
  }

  auto operator=(SizedMatrixT<extent> auto&& other) -> Dense& {
    shape_ = other.shape();
    data_.resize(shape_.row * shape_.col);
    CHECK(verifyShape(*this));
    for (size_t i = 0; i < shape_.row; ++i) {
      for (size_t j = 0; j < shape_.col; ++j) {
        (*this)[i, j] = std::move(other[i, j]);
      }
    }
    return *this;
  }

  explicit Dense(RowCol shape) : shape_{shape}, data_(shape_.row * shape_.col) {
    CHECK(verifyShape(*this));
  }

  explicit Dense(std::ranges::input_range auto&& data)
    requires(extent.row != kDynamic and extent.col != kDynamic)
      : shape_(extent), data_({}) {
    data_.reserve(shape_.row * shape_.col);
    std::ranges::copy(data, std::back_inserter(data_));
    CHECK(verifyShape(*this));
  }

  explicit Dense(RowCol shape, std::ranges::input_range auto&& data)
      : shape_(shape), data_({}) {
    data_.reserve(shape_.row * shape_.col);
    std::ranges::copy(data, std::back_inserter(data_));
    CHECK(verifyShape(*this));
  }

  struct Sentinel {};

  template <typename Mat>
  class Iterator {
   public:
    Iterator(Mat& parent, RowCol location)
        : parent_(parent), location_(location), ptr_(&parent_.get(location)) {}

    auto operator*() -> decltype(auto) { return *ptr_; }

    auto operator++() -> Iterator& {
      if constexpr (order == IndexOrder::kRowMajor) {
        return incCol();
      } else {
        return incRow();
      }
    }

    template <RowCol delta>
    auto update() -> Iterator& {
      location_ += delta;
      if constexpr (order == IndexOrder::kColMajor) {
        if constexpr (extent.row != kDynamic) {
          ptr_ += delta.row + extent.row * delta.col;
        } else {
          ptr_ += delta.row + parent_.shape().row * delta.col;
        }
      } else if constexpr (order == IndexOrder::kRowMajor) {
        if constexpr (extent.row != kDynamic) {
          ptr_ += extent.col * delta.row + delta.col;
        } else {
          ptr_ += parent_.shape().col * delta.row + delta.col;
        }
      } else {
        static_assert(false, "Not implented for Index Order");
      }
      return *this;
    }

    auto incRow() -> Iterator& {
      return update<{1, 0}>();
    }

    auto incCol() -> Iterator& {
      return update<{0, 1}>();
    }

    auto location() const -> RowCol { return location_; }
    auto operator!=(const Iterator& iter) { return !(ptr_ == iter.ptr_); }
    auto operator!=(Sentinel /*unused*/) {
      return ptr_ < &*parent_.data().end();
    }

   private:
    Mat& parent_;
    RowCol location_;
    decltype(&parent_.get(std::declval<RowCol>())) ptr_;
  };

  template <typename Mat>
  Iterator(Mat& parent, RowCol location) -> Iterator<Mat&>;

  auto beginImpl(this auto&& self) { return Iterator{self, {0, 0}}; }

  auto endImpl() const -> Sentinel { return {}; }

  auto rowsImpl(this auto&& self) { return IterRows(self); }

  auto colsImpl(this auto&& self) { return IterCols(self); }

  auto data() const -> const std::vector<Scalar>& { return data_; }

  auto data() && -> std::vector<Scalar>& { return data_; }

 private:
  friend Matrix<extent>;

  auto shapeImpl() const -> RowCol { return shape_; }

  auto getImpl(this auto&& self, size_t row, size_t col) -> decltype(auto) {
    if constexpr (order == IndexOrder::kRowMajor) {
      return self.data_[row * self.shape_.col + col];
    } else {
      return self.data_[col * self.shape_.row + row];
    }
  }

  RowCol shape_;
  std::vector<Scalar> data_;
};

// NOLINTBEGIN(*-avoid-c-arrays)
template <typename Scalar, size_t First, size_t... Sizes>
explicit Dense(const Scalar (&)[First], const Scalar (&... rows)[Sizes])
    -> Dense<Scalar, {sizeof...(Sizes) + 1, First}>;
// NOLINTEND(*-avoid-c-arrays)

}  // namespace tempura::matrix

