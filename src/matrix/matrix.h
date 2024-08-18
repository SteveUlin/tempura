#include <cassert>
#include <cstddef>
#include <cstdlib>
#include <span>
#include <vector>

namespace tempura::matrix {

// Size of a matrix to be determined at runtime
struct Extent {
  constexpr Extent(size_t value) : has_value(true), value(value) {}
  constexpr Extent(bool has_value, size_t value)
      : has_value(has_value), value(value) {}

  bool has_value;
  size_t value;
};

static constexpr Extent kDynamic = Extent{false, 0};

constexpr auto operator==(Extent lhs, Extent rhs) -> bool {
  return lhs.has_value == rhs.has_value and lhs.value == rhs.value;
}
constexpr auto match(std::pair<Extent, Extent> lhs,
                     std::pair<Extent, Extent> rhs) -> bool {
  bool lhs_math =
      lhs.first == kDynamic or rhs.first == kDynamic or lhs.first == rhs.first;
  bool rhs_math = lhs.second == kDynamic or rhs.second == kDynamic or
                  lhs.second == rhs.second;
  return lhs_math and rhs_math;
}

struct Shape {
  size_t row;
  size_t col;
};

constexpr auto operator==(Shape lhs, Shape rhs) -> bool {
  return lhs.row == rhs.row and lhs.col == rhs.col;
}

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

// Crash if a dynamic size is set and the shape is different from the
// size defined in the type.
template <typename M>
auto verifyShape(const M& m) -> void {
  if (M::kRow.has_value and M::kRow.value != m.shape().row) {
    abort();
  }
  if (M::kCol.has_value and M::kCol.value != m.shape().col) {
    abort();
  }
}


template <typename Scalar, Extent Row, Extent Col,
          IndexOrder Order = IndexOrder::kColMajor>
class Dense : public Matrix<Row, Col> {
 public:
  Dense() : row_(Row.value), col_(Col.value), data_(row_ * col_) {}

  Dense(const Dense& other)
      : row_(other.row_), col_(other.col_), data_(other.data_) {}
  Dense(Dense&& other) noexcept
      : row_(other.row_), col_(other.col_), data_(std::move(other.data_)) {}
  auto operator=(const Dense& other) -> Dense& {
    row_ = other.row_;
    col_ = other.col_;
    data_ = other.data_;
    return *this;
  }
  auto operator=(Dense&& other) noexcept -> Dense& {
    row_ = other.row_;
    col_ = other.col_;
    data_ = std::move(other.data_);
    return *this;
  }

  template <Extent OtherRow, Extent OtherCol>
    requires(match({Row, Col}, {OtherRow, OtherCol}))
  Dense(const Dense<Scalar, OtherRow, OtherCol, Order>& other)
      : row_(other.shape().row), col_(other.shape().col), data_(other.data()) {
    verifyShape(*this);
  }
  template <Extent OtherRow, Extent OtherCol>
    requires(match({Row, Col}, {OtherRow, OtherCol}))
  Dense(Dense<Scalar, OtherRow, OtherCol, Order>&& other)
      : row_(other.shape().col), col_(other.shape().col), data_(other.data()) {
    verifyShape(*this);
  }

  template <Extent OtherRow, Extent OtherCol>
    requires(match({Row, Col}, {OtherRow, OtherCol}))
  auto operator=(const Dense<Scalar, OtherRow, OtherCol, Order>& other)
      -> Dense& {
    row_ = other.shape().row;
    col_ = other.shape().col;
    data_ = other.data();
    verifyShape(*this);
    return *this;
  }

  template <Extent OtherRow, Extent OtherCol>
    requires(match({Row, Col}, {OtherRow, OtherCol}))
  auto operator=(Dense<Scalar, OtherRow, OtherCol, Order>&& other) -> Dense& {
    row_ = other.shape().row;
    col_ = other.shape().col;
    data_ = std::move(other.data());
    verifyShape(*this);
    return *this;
  }

  // Array initialization
  // auto m = matrix::Dense{{0., 1.}, {2., 3.}};
  //
  // Use C-Arrays to get the nice syntax
  // NOLINTBEGIN(*-avoid-c-arrays)
  template <size_t First, size_t... Sizes>
    requires((!Row.has_value or (sizeof...(Sizes) + 1 == Row)) and
             (!Col.has_value or (First == Col)) and ((First == Sizes) and ...))
  Dense(const Scalar (&first)[First], const Scalar (&... rows)[Sizes])
      : row_(sizeof...(Sizes) + 1), col_(First), data_(row_ * col_) {
    for (size_t j = 0; j < First; ++j) {
      (*this)[0, j] = first[j];
    }
    size_t i = 1;
    (
        [this, &rows, &i]() {
          for (size_t j = 0; j < col_; ++j) {
            (*this)[i, j] = rows[j];
          }
          ++i;
        }(),
        ...);
  }
  // NOLINTEND(*-avoid-c-arrays)

  template <typename M>
    requires(std::derived_from<M, Matrix<M::kRow, M::kCol>> and
             match({Row, Col}, {M::kRow, M::kCol}))
  Dense(const M& other)
      : row_(other.shape().row), col_(other.shape().col), data_(row_ * col_) {
    verifyShape(*this);
    for (size_t i = 0; i < row_; ++i) {
      for (size_t j = 0; j < col_; ++j) {
        (*this)[i, j] = other[i, j];
      }
    }
  }

  template <typename M>
    requires(std::derived_from<M, Matrix<M::kRow, M::kCol>> and
             match({Row, Col}, {M::kRow, M::kCol}))
  Dense(M&& other)
      : row_(other.shape().row), col_(other.shape().col), data_(row_ * col_) {
    verifyShape(*this);
    for (size_t i = 0; i < row_; ++i) {
      for (size_t j = 0; j < col_; ++j) {
        (*this)[i, j] = std::move(other[i, j]);
      }
    }
  }

  template <typename M>
    requires(std::derived_from<M, Matrix<M::kRow, M::kCol>> and
             match({Row, Col}, {M::kRow, M::kCol}))
  auto operator=(const M& other) -> Dense& {
    row_ = other.shape().row;
    col_ = other.shape().col;
    data_.resize(row_ * col_);
    verifyShape(*this);
    for (size_t i = 0; i < row_; ++i) {
      for (size_t j = 0; j < col_; ++j) {
        (*this)[i, j] = other[i, j];
      }
    }
    return *this;
  }

  template <typename M>
    requires(std::derived_from<M, Matrix<M::kRow, M::kCol>> and
             match({Row, Col}, {M::kRow, M::kCol}))
  auto operator=(M&& other) -> Dense& {
    row_ = other.shape().row;
    col_ = other.shape().col;
    data_.resize(row_ * col_);
    verifyShape(*this);
    for (size_t i = 0; i < row_; ++i) {
      for (size_t j = 0; j < col_; ++j) {
        (*this)[i, j] = std::move(other[i, j]);
      }
    }
    return *this;
  }

  explicit Dense(Shape shape)
      : row_(shape.row), col_(shape.col), data_(row_ * col_) {
    verifyShape(*this);
  }

  explicit Dense(Shape shape, std::span<Scalar> data)
      : row_(shape.row), col_(shape.col), data_(data) {
    verifyShape(*this);
  }

  explicit Dense(Shape shape, std::vector<Scalar>&& data)
      : row_(shape.row), col_(shape.col), data_(data) {
    verifyShape(*this);
  }

  auto data() const -> const std::vector<Scalar>& { return data_; }

  auto data() && -> std::vector<Scalar>& { return data_; }

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

// NOLINTBEGIN(*-avoid-c-arrays)
template <typename Scalar, size_t First, size_t... Sizes>
explicit Dense(const Scalar (&)[First], const Scalar (&... rows)[Sizes])
    -> Dense<Scalar, sizeof...(Sizes) + 1, First>;
// NOLINTEND(*-avoid-c-arrays)

}  // namespace tempura::matrix
