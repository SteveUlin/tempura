#include <cassert>
#include <cstddef>
#include <memory>
#include <vector>

namespace tempura::matrix {

// Size of a matrix to be determined at runtime
struct Extent {
  constexpr Extent(size_t value) : has_value(true), value(value) {}
  constexpr Extent(bool has_value, size_t value)
      : has_value(has_value), value(value) {}

  bool has_value = true;
  size_t value;
};

static constexpr Extent kDynamicExtent = Extent{false, 0};

constexpr bool operator==(Extent lhs, Extent rhs) {
  return lhs.has_value == rhs.has_value and lhs.value == rhs.value;
}
constexpr bool shapesMatch(std::pair<Extent, Extent> lhs,
                           std::pair<Extent, Extent> rhs) {
  bool lhs_math = lhs.first == kDynamicExtent or rhs.first == kDynamicExtent or
                  lhs.first == rhs.first;
  bool rhs_math = lhs.second == kDynamicExtent or
                  rhs.second == kDynamicExtent or lhs.second == rhs.second;
  return lhs_math and rhs_math;
}

struct Shape {
  size_t row;
  size_t col;
};

enum class IndexOrder {
  kRowMajor,
  kColMajor,
};

// Subclasses must define the following functions
// - get(size_t row, size_t col)
//   Return a reference like object to the given index.
//   May return any kind of object for this const version.
// - getShape()
//   Return the current shape of the matrix.
template <Extent Row, Extent Col>
class Matrix {
 public:
  inline static constexpr Extent kRow = Row;
  inline static constexpr Extent kCol = Col;

  auto shape(this auto&& self) -> Shape { return self.getShape(); }

  auto operator[](this auto&& self, size_t row, size_t col) -> decltype(auto) {
    return self.get(row, col);
  }

  // Vector accessors
  auto operator[](this auto&& self, size_t col) -> decltype(auto)
    requires(Row == 1)
  {
    return self.get(0, col);
  }
  auto operator[](this auto&& self, size_t row) -> decltype(auto)
    requires(Col == 1)
  {
    return self.get(row, 0);
  }
};

template <typename Scalar, Extent Row, Extent Col,
          IndexOrder Order = IndexOrder::kColMajor>
class Dense : public Matrix<Row, Col> {
 public:
  Dense()
    requires(Row.has_value and Col.has_value)
      : row_(Row.value), col_(Col.value), data_(row_ * col_) {}

  Dense(const Dense& other)
      : row_(other.row_), col_(other.col_), data_(other.data_) {}
  Dense(Dense&& other)
      : row_(other.row_), col_(other.col_), data_(std::move(other.data_)) {}
  auto operator=(const Dense& other) -> Dense& {
    row_ = other.row_;
    col_ = other.col_;
    data_ = other.data_;
    return *this;
  }
  auto operator=(Dense&& other) -> Dense& {
    row_ = other.row_;
    col_ = other.col_;
    data_ = std::move(other.data_);
    return *this;
  }

  auto verifyShape() const -> void {
    if (Row.has_value and Row.value != row_) {
      abort();
    }
    if (Col.has_value and Col.value != col_) {
      abort();
    }
  }

  template <Extent OtherRow, Extent OtherCol>
    requires(shapesMatch({Row, Col}, {OtherRow, OtherCol}))
  explicit Dense(
      const Dense<Scalar, OtherRow, OtherCol, Order>& other)
      : row_(other.row_), col_(other.col_), data_(other.data_) {
    verifyShape();
  }

  template <Extent OtherRow, Extent OtherCol>
    requires(shapesMatch({Row, Col}, {OtherRow, OtherCol}))
  explicit Dense(
      Dense<Scalar, OtherRow, OtherCol, Order>&& other)
      : row_(other.row_), col_(other.col_), data_(std::move(other.data_)) {
    verifyShape();
  }

  template <Extent OtherRow, Extent OtherCol>
    requires(shapesMatch({Row, Col}, {OtherRow, OtherCol}))
  auto operator=(const Dense<Scalar, OtherRow, OtherCol, Order>&
                     other) -> Dense& {
    row_ = other.row_;
    col_ = other.col_;
    data_ = other.data_;
    verifyShape();
    return *this;
  }

  template <Extent OtherRow, Extent OtherCol>
    requires(shapesMatch({Row, Col}, {OtherRow, OtherCol}))
  auto operator=(Dense<Scalar, OtherRow, OtherCol, Order>&&
                     other) -> Dense& {
    row_ = other.row_;
    col_ = other.col_;
    data_ = std::move(other.data_);
    verifyShape();
    return *this;
  }

  // Array initialization
  // auto m = matrix::Dense{{0., 1.}, {2., 3.}};
  template <size_t... Sizes>
    requires(Row.has_value and Col.has_value and (sizeof...(Sizes) == Row) and
             ((Sizes == Col) and ...))
  explicit Dense(const Scalar (&... rows)[Sizes])
      : row_(Row.value), col_(Col.value), data_(row_ * col_) {
    size_t i = 0;
    (
        [this, &rows, &i]() {
          for (size_t j = 0; j < col_; ++j) {
            (*this)[i, j] = rows[j];
          }
          ++i;
        }(),
        ...);
  }
  template <typename M>
    requires(std::derived_from<M, Matrix<M::kRow, M::kCol>> and
             shapesMatch({Row, Col}, {M::kRow, M::kCol}))
  Dense(const M& other)
      : row_(other.shape().row), col_(other.shape().col), data_(row_ * col_) {
    verifyShape();
    for (size_t i = 0; i < row_; ++i) {
      for (size_t j = 0; j < col_; ++j) {
        (*this)[i, j] = other[i, j];
      }
    }
  }

  template <typename M>
    requires(std::derived_from<M, Matrix<M::kRow, M::kCol>> and
             shapesMatch({Row, Col}, {M::kRow, M::kCol}))
  Dense(M&& other)
      : row_(other.shape().row), col_(other.shape().col), data_(row_ * col_) {
    verifyShape();
    for (size_t i = 0; i < row_; ++i) {
      for (size_t j = 0; j < col_; ++j) {
        (*this)[i, j] = std::move(other[i, j]);
      }
    }
  }

  template <typename M>
    requires(std::derived_from<M, Matrix<M::kRow, M::kCol>> and
             shapesMatch({Row, Col}, {M::kRow, M::kCol}))
  auto operator=(M&& other) -> Dense& {
    row_ = other.shape().row;
    col_ = other.shape().col;
    data_.resize(row_ * col_);
    verifyShape();
    for (size_t i = 0; i < row_; ++i) {
      for (size_t j = 0; j < col_; ++j) {
        (*this)[i, j] = std::move(other[i, j]);
      }
    }
    return *this;
  }


 private:
  friend Matrix<Row, Col>;

  auto getShape() const -> Shape { return {.row = row_, .col = col_}; }

  auto get(this auto&& self, size_t row, size_t col) -> decltype(auto) {
    if constexpr (Order == IndexOrder::kRowMajor) {
      return self.data_[row * self.col_ + col];
    } else {
      return self.data_[col * self.row_ + row];
    }
  }

  size_t row_;
  size_t col_;
  std::vector<Scalar> data_;
};

template <typename Scalar, size_t First, size_t... Sizes>
explicit Dense(const Scalar (&)[First], const Scalar (&... rows)[Sizes])
    -> Dense<Scalar, sizeof...(Sizes) + 1, First>;

}  // namespace tempura::matrix
