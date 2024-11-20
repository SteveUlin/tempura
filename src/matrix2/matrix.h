#pragma once

#include <array>
#include <cstdint>
#include <type_traits>
#include <utility>

namespace tempura::matrix {

struct DynamicExtent {};
inline static constexpr auto kDynamic = DynamicExtent{};

template <typename T>
concept ExtentT =
    std::is_same_v<T, DynamicExtent> or std::is_same_v<T, int64_t>;

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

// For tagged dispatch
template <typename>
struct Tag {};
template <auto>
struct ValueTag {};

template <typename Scalar, int64_t Row, int64_t Col,
          IndexOrder order = kColMajor>
class Dense {
 public:
  using ScalarT = Scalar;
  static constexpr IndexOrder kIndexOrder = order;

  // Declare that the default constructor is constexpr
  constexpr Dense() = default;

  constexpr Dense(std::array<Scalar, Row * Col> data) : data_{data} {}

  // Array initialization
  // auto m = matrix::Dense{{0., 1.}, {2., 3.}};
  //
  // Use C-Arrays to get the nice syntax
  //
  // "First" is split from the rest of the sizes to make the deduction guide
  // easier to write.
  // NOLINTBEGIN(*-avoid-c-arrays)
  template <int64_t First, int64_t... Sizes>
    requires((sizeof...(Sizes) + 1 == Row) and (First == Col) and
             ((Sizes == Col) and ...))
  constexpr Dense(const Scalar (&first)[First],
                  const Scalar (&... rows)[Sizes]) {
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
    return self.data_[index];
  }

  constexpr auto data() const -> const std::array<Scalar, Row * Col>& {
    return data_;
  }

  constexpr auto operator==(const Dense& other) const -> bool {
    return data_ == other.data_;
  }

  constexpr auto begin(this auto&& self) { return self.data_.begin(); }

  constexpr auto end(this auto&& self) { return self.data_.end(); }

 private:
  std::array<Scalar, Row * Col> data_ = {};
};

// NOLINTBEGIN(*-avoid-c-arrays)
template <typename Scalar, int64_t First, int64_t... Sizes>
explicit Dense(const Scalar (&)[First], const Scalar (&... rows)[Sizes])
    -> Dense<Scalar, sizeof...(Sizes) + 1, First>;

template <typename Scalar, int64_t Row, int64_t Col>
class DenseRowMajor : public Dense<Scalar, Row, Col, kRowMajor> {
 public:
  constexpr DenseRowMajor() = default;

  constexpr DenseRowMajor(std::array<Scalar, Row * Col> data)
      : Dense<Scalar, Row, Col, kRowMajor>{data} {}

  template <int64_t First, int64_t... Sizes>
    requires((sizeof...(Sizes) + 1 == Row) and (First == Col) and
             ((Sizes == Col) and ...))
  constexpr DenseRowMajor(const Scalar (&first)[First],
                          const Scalar (&... rows)[Sizes])
      : Dense<Scalar, Row, Col, kRowMajor>{first, rows...} {}
};

template <typename Scalar, int64_t First, int64_t... Sizes>
explicit DenseRowMajor(const Scalar (&)[First], const Scalar (&... rows)[Sizes])
    -> DenseRowMajor<Scalar, sizeof...(Sizes) + 1, First>;

template <typename Scalar, int64_t Row, int64_t Col>
class DenseColMajor : public Dense<Scalar, Row, Col, kColMajor> {
 public:
  constexpr DenseColMajor() = default;

  constexpr DenseColMajor(std::array<Scalar, Row * Col> data)
      : Dense<Scalar, Row, Col, kColMajor>{data} {}

  template <int64_t First, int64_t... Sizes>
    requires((sizeof...(Sizes) + 1 == Row) and (First == Col) and
             ((Sizes == Col) and ...))
  constexpr DenseColMajor(const Scalar (&first)[First],
                          const Scalar (&... rows)[Sizes])
      : Dense<Scalar, Row, Col, kColMajor>{first, rows...} {}
};

template <typename Scalar, int64_t First, int64_t... Sizes>
explicit DenseColMajor(const Scalar (&)[First], const Scalar (&... rows)[Sizes])
    -> DenseColMajor<Scalar, sizeof...(Sizes) + 1, First>;
// NOLINTEND(*-avoid-c-arrays)

}  // namespace tempura::matrix
