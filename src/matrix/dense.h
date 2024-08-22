#pragma once
#include <span>
#include <utility>
#include <vector>

#include "matrix/matrix.h"

namespace tempura::matrix {

template <typename Scalar, RowCol extent,
          IndexOrder order = IndexOrder::kColMajor>
class Dense final : public Matrix<extent> {
 public:
  Dense() : shape_({.row = (extent.row == kDynamic) ? 0 : extent.row,
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
    requires(((extent.row == kDynamic) or (sizeof...(Sizes) + 1 == extent.row)) and
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

  explicit Dense(RowCol shape, std::span<Scalar> data)
      : shape_(shape), data_(data) {
    CHECK(verifyShape(*this));
  }

  explicit Dense(RowCol shape, std::vector<Scalar>&& data)
      : shape_(shape), data_(data) {
    CHECK(verifyShape(*this));
  }

  auto data() const -> const std::vector<Scalar>& { return data_; }

  auto data() && -> std::vector<Scalar>& { return data_; }

 private:
  friend Matrix<extent>;

  auto shapeImpl() const -> RowCol { return shape_; }

  auto getImpl(this auto&& self, RowCol index) -> decltype(auto) {
    if constexpr (order == IndexOrder::kRowMajor) {
      return self.data_[index.row * self.shape_.col + index.col];
    } else {
      return self.data_[index.col * self.shape_.row + index.row];
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
