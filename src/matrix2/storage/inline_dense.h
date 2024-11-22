#pragma once

#include <array>
#include <cstdint>
#include <utility>

#include "matrix2/matrix.h"

namespace tempura::matrix {

template <typename T, int64_t Row, int64_t Col, IndexOrder order = kColMajor>
class InlineDense {
 public:
  using ValueType = T;
  static constexpr IndexOrder kIndexOrder = order;
  static constexpr int64_t kRow = Row;
  static constexpr int64_t kCol = Col;

  // Declare that the default constructor is constexpr
  constexpr InlineDense() = default;
  constexpr InlineDense(const InlineDense&) = default;
  constexpr auto operator=(const InlineDense&) -> InlineDense& = default;

  constexpr InlineDense(std::array<T, Row * Col> data) : data_{data} {}

  template <MatrixT M>
    requires(((std::is_same_v<decltype(M::kRow), DynamicExtent>) or
              (M::kRow == Row)) and
             ((std::is_same_v<decltype(M::kCol), DynamicExtent>) or
              (M::kCol == Col)))
  constexpr InlineDense(const M& other) {
    if constexpr (std::is_same_v<decltype(M::kRow), DynamicExtent>) {
      CHECK(other.shape().row == Row);
    }
    if constexpr (std::is_same_v<decltype(M::kCol), DynamicExtent>) {
      CHECK(other.shape().col == Col);
    }
    for (int64_t i = 0; i < Row; ++i) {
      for (int64_t j = 0; j < Col; ++j) {
        (*this)[i, j] = other[i, j];
      }
    }
  }

  // Array initialization
  // auto m = matrix::InlineDense{{0., 1.}, {2., 3.}};
  //
  // Use C-Arrays to get the nice syntax
  //
  // "First" is split from the rest of the sizes to make the deduction guide
  // easier to write.
  // NOLINTBEGIN(*-avoid-c-arrays)
  template <int64_t First, int64_t... Sizes>
    requires((sizeof...(Sizes) + 1 == Row) and (First == Col) and
             ((Sizes == Col) and ...))
  constexpr InlineDense(const T (&first)[First], const T (&... rows)[Sizes]) {
    for (int64_t j = 0; j < First; ++j) {
      (*this)[0, j] = first[j];
    }
    [&, this]<int64_t... I>(std::integer_sequence<int64_t, I...> /*unused*/) {
      (
          [&, this]() {
            for (int64_t j = 0; j < Sizes; ++j) {
              (*this)[I + 1, j] = rows[j];
            }
          }(),
          ...);
    }(std::make_integer_sequence<int64_t, sizeof...(rows)>());
  }
  // NOLINTEND(*-avoid-c-arrays)

  constexpr auto shape() const -> RowCol { return {.row = Row, .col = Col}; }

  constexpr auto operator[](this auto&& self, int64_t row, int64_t col)
      -> decltype(auto) {
    if (std::is_constant_evaluated()) {
      CHECK(0 <= row and row < kRow);
      CHECK(0 <= col and col < kCol);
    }
    if constexpr (order == IndexOrder::kRowMajor) {
      return self.data_[(row * Col) + col];
    } else if constexpr (order == IndexOrder::kColMajor) {
      return self.data_[(col * Row) + row];
    } else {
      static_assert(false, "Not implented for Index Order");
    }
  }

  constexpr auto operator[](this auto&& self, int64_t index) -> decltype(auto)
    requires(Row == 1 or Col == 1)
  {
    if (std::is_constant_evaluated()) {
      CHECK(0 <= index and index < Row * Col);
    }
    return self.data_[index];
  }

  constexpr auto data() const -> const std::array<T, Row * Col>& {
    return data_;
  }

  constexpr auto operator==(const InlineDense& other) const -> bool {
    return data_ == other.data_;
  }

  constexpr auto begin(this auto&& self) { return self.data_.begin(); }

  constexpr auto end(this auto&& self) { return self.data_.end(); }

 private:
  std::array<T, Row * Col> data_ = {};
};

static_assert(MatrixT<InlineDense<double, 2, 2>>);

// NOLINTBEGIN(*-avoid-c-arrays)
template <typename T, int64_t First, int64_t... Sizes>
explicit InlineDense(const T (&)[First], const T (&... rows)[Sizes])
    -> InlineDense<T, sizeof...(Sizes) + 1, First>;

template <typename T, int64_t Row, int64_t Col>
class InlineDenseRowMajor : public InlineDense<T, Row, Col, kRowMajor> {
 public:
  constexpr InlineDenseRowMajor() = default;

  constexpr InlineDenseRowMajor(std::array<T, Row * Col> data)
      : InlineDense<T, Row, Col, kRowMajor>{data} {}

  template <int64_t First, int64_t... Sizes>
    requires((sizeof...(Sizes) + 1 == Row) and (First == Col) and
             ((Sizes == Col) and ...))
  constexpr InlineDenseRowMajor(const T (&first)[First],
                                const T (&... rows)[Sizes])
      : InlineDense<T, Row, Col, kRowMajor>{first, rows...} {}
};

template <typename T, int64_t First, int64_t... Sizes>
explicit InlineDenseRowMajor(const T (&)[First], const T (&... rows)[Sizes])
    -> InlineDenseRowMajor<T, sizeof...(Sizes) + 1, First>;

template <typename T, int64_t Row, int64_t Col>
class InlineDenseColMajor : public InlineDense<T, Row, Col, kColMajor> {
 public:
  constexpr InlineDenseColMajor() = default;

  constexpr InlineDenseColMajor(std::array<T, Row * Col> data)
      : InlineDense<T, Row, Col, kColMajor>{data} {}

  template <int64_t First, int64_t... Sizes>
    requires((sizeof...(Sizes) + 1 == Row) and (First == Col) and
             ((Sizes == Col) and ...))
  constexpr InlineDenseColMajor(const T (&first)[First],
                                const T (&... rows)[Sizes])
      : InlineDense<T, Row, Col, kColMajor>{first, rows...} {}
};

template <typename T, int64_t First, int64_t... Sizes>
explicit InlineDenseColMajor(const T (&)[First], const T (&... rows)[Sizes])
    -> InlineDenseColMajor<T, sizeof...(Sizes) + 1, First>;
// NOLINTEND(*-avoid-c-arrays)

}  // namespace tempura::matrix
