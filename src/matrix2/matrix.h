#pragma once

#include <cassert>
#include <compare>
#include <concepts>
#include <cstdint>
#include <print>
#include <source_location>
#include <string_view>

namespace tempura::matrix {

constexpr void check(
    bool condition, std::string_view message,
    std::source_location loc = std::source_location::current()) {
  if (!condition) [[unlikely]] {
    std::print("FATAL {}:{}:{}: {}\n", loc.file_name(), loc.line(),
               loc.column(), message);
    std::terminate();
  }
}
#define CHECK(condition) ::tempura::matrix::check(condition, #condition)

// The size of a dimension known at compile time.
using Extent = int64_t;

// A special value that can be used to indicate that the size of a dimension is
// not known at compile time.
inline static constexpr Extent kDynamic = std::numeric_limits<Extent>::min();

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

template <typename T>
concept MatrixT = requires(T matrix, int64_t row, int64_t col) {
  { matrix[row, col] } -> std::convertible_to<typename T::ValueType>;
  { matrix.shape() } -> std::same_as<RowCol>;
  requires std::same_as<decltype(T::kRow), const Extent>;
  requires std::same_as<decltype(T::kCol), const Extent>;
};

template <typename T, typename U>
concept MatchingExtent =
    (T::kRow == U::kRow or T::kRow == kDynamic or U::kRow == kDynamic) and
    (T::kCol == U::kCol or T::kCol == kDynamic or U::kCol == kDynamic);

template <typename T>
concept HasDynamicExtent = (T::kRow == kDynamic or T::kCol == kDynamic);

// Checks the shape field if there are dynamic extents
//
// This also imposes a constraint that the extents must match, so technically
// you don't need to explicitly add the MatchingExtent requires clause to
// functions that call this check as it will be applied implicitly.
template <typename Lhs, typename Rhs>
  requires MatchingExtent<Lhs, Rhs>
constexpr void checkSameShape(const Lhs& lhs, const Rhs& rhs) {
  if constexpr (HasDynamicExtent<Lhs> or HasDynamicExtent<Rhs>) {
    CHECK(lhs.shape() == rhs.shape());
  } else {
    static_assert(Lhs::kRow == Rhs::kRow and Lhs::kCol == Rhs::kCol,
                  "Matrix extents must match");
  }
}

// Default Equality
template <MatrixT Lhs, MatrixT Rhs>
constexpr auto operator==(const Lhs& lhs, const Rhs& rhs) -> bool {
  checkSameShape(lhs, rhs);
  for (int64_t row = 0; row < lhs.shape().row; ++row) {
    for (int64_t col = 0; col < lhs.shape().col; ++col) {
      if (lhs[row, col] != rhs[row, col]) {
        return false;
      }
    }
  }
  return true;
}

template <auto delta = 0.0001, MatrixT Lhs, MatrixT Rhs>
constexpr auto approxEqual(const Lhs& lhs, const Rhs& rhs) -> bool {
  checkSameShape(lhs, rhs);
  for (int64_t row = 0; row < lhs.shape().row; ++row) {
    for (int64_t col = 0; col < lhs.shape().col; ++col) {
      if (std::abs(lhs[row, col] - rhs[row, col]) > delta) {
        return false;
      }
    }
  }
  return true;
}

enum class Pivot : uint8_t {
  kNone,
  kRow,
  kRowImplicit,
  kFull,
};

// For uniformity, all classes that take in a matrix in the constructor should
// copy the input matrix. If you want to avoid copying, you can use a MatRef.
//
// Example:
//  auto m = InlineDense{{1., 2.}, {3., 4.}};
//  auto m_t = Transpose{MatRef{m}};
//
// See https://en.cppreference.com/w/cpp/utility/functional/reference_wrapper

namespace internal {
// It's ok to use a type different than T if you can implicitly convert to T&.
// This function both:
//  - Converts input to T
//  - Only works over lvalues, not rvalues
template <typename T>
constexpr auto FUN(T& t) -> T& {
  return t;
}
template <typename T>
constexpr void FUN(T&&) = delete;

template <typename T>
concept HasSwap = requires(T t, int64_t i, int64_t j) {
  { t.swap(i, j) };
};

template <typename T>
concept HasData = requires(const T& t) {
  { t.data() };
};

};  // namespace internal

template <typename T>
class MatRef {
 public:
  using ChildType = T;
  using ValueType = typename T::ValueType;

  static constexpr auto kRow = T::kRow;
  static constexpr auto kCol = T::kCol;

  template <typename U>
    requires(std::convertible_to<U, T&> and
             not std::same_as<std::remove_cvref_t<U>, MatRef>)
  constexpr MatRef(U&& mat) noexcept(noexcept(internal::FUN<T>(mat)))
      : mat_{std::addressof(internal::FUN(mat))} {}

  constexpr MatRef(const MatRef&) noexcept = default;

  constexpr auto operator[](this auto&& self, int64_t row, int64_t col)
      -> decltype(auto) {
    return (*self.mat_)[row, col];
  }

  constexpr auto shape() const -> RowCol { return mat_->shape(); }

  constexpr auto get() -> T& { return &mat_; }

  // Auto conversion to T& so you can pass MatRefs to functions that take T&
  constexpr operator T&() { return *mat_; }

  constexpr void swap(int64_t i, int64_t j)
    requires internal::HasSwap<T>
  {
    mat_->swap(i, j);
  }

  constexpr auto data() const -> decltype(auto)
    requires internal::HasData<T>
  {
    return mat_->data();
  }

 private:
  T* mat_;
};

template <typename T>
MatRef(T&) -> MatRef<T>;

}  // namespace tempura::matrix
