#pragma once

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>
#include <vector>

#include "matrix3/accessors.h"
#include "matrix3/extents.h"
#include "matrix3/layouts.h"

namespace tempura::matrix3 {

template <typename ExtentsT, typename LayoutT, typename AccessorT>
class GenericMatrix {
 public:
  using ExtentsType = ExtentsT;
  using LayoutType = LayoutT;
  using AccessorType = AccessorT;

  using MappingType =
      LayoutT::template Mapping<ExtentsT, typename ExtentsT::IndexType>;

  constexpr GenericMatrix(ExtentsT extent, MappingType layout,
                          AccessorT accessor)
      : extent_(extent), layout_(layout), accessor_(accessor) {}

  template <typename... Indices>
    requires(sizeof...(Indices) == ExtentsT::rank())
  constexpr auto operator[](this auto&& self, Indices... indices)
      -> decltype(auto) {
    return self.accessor_(self.layout_(indices...));
  }

  constexpr auto extent() const -> const ExtentsT& { return extent_; }

  constexpr auto rows() const { return extent_.extent(0); }
  constexpr auto cols() const { return extent_.extent(1); }

  constexpr auto data() -> decltype(auto)
    requires requires(const AccessorT& a) { a.data(); }
  {
    return accessor_.data();
  }

 protected:
  // NOLINTBEGIN(*-avoid-c-arrays)
  template <typename Scalar, int64_t... Sizes>
  constexpr void initHelper(this auto&& self, const Scalar (&... rows)[Sizes]) {
    [&]<int64_t... I>(std::integer_sequence<int64_t, I...> /*unused*/) {
      (
          [&]() {
            for (int64_t j = 0; j < Sizes; ++j) {
              self[I, j] = rows[j];
            }
          }(),
          ...);
    }(std::make_integer_sequence<int64_t, sizeof...(rows)>());
  }
  // NOLINTEND(*-avoid-c-arrays)

 private:
  ExtentsType extent_;
  MappingType layout_;
  AccessorType accessor_;
};

template <typename Scalar, std::size_t... Ns>
class Dense : public GenericMatrix<Extents<std::size_t, Ns...>, LayoutLeft,
                                   RangeAccessor<std::vector<Scalar>>> {
 public:
  using ParentType = GenericMatrix<Extents<std::size_t, Ns...>, LayoutLeft,
                                   RangeAccessor<std::vector<Scalar>>>;

  using ParentType::ParentType;

  constexpr Dense() : ParentType({}, {}, std::vector<Scalar>((Ns * ...))) {}

  // NOLINTBEGIN(*-avoid-c-arrays)
  template <typename OtherScalar, int64_t... Sizes>
    requires((sizeof...(Ns) == 2) &&
             (Extents<std::size_t, Ns...>::staticExtent(0) ==
              sizeof...(Sizes)) &&
             ((Extents<std::size_t, Ns...>::staticExtent(1) == Sizes) && ...))
  constexpr Dense(const OtherScalar (&... rows)[Sizes])
      : ParentType({}, {}, std::vector<Scalar>((Ns * ...))) {
    this->initHelper(rows...);
  }
  // NOLINTEND(*-avoid-c-arrays)
};

// NOLINTBEGIN(*-avoid-c-arrays)
template <typename Scalar, int64_t First, int64_t... Sizes>
Dense(const Scalar (&fst)[First], const Scalar (&... rows)[Sizes])
  -> Dense<Scalar, sizeof...(Sizes) + 1, First>;
// NOLINTEND(*-avoid-c-arrays)

template <typename Scalar, std::size_t... Ns>
class InlineDense
    : public GenericMatrix<Extents<std::size_t, Ns...>, LayoutLeft,
                           RangeAccessor<std::array<Scalar, (Ns * ...)>>> {
 public:
  using ParentType =
      GenericMatrix<Extents<std::size_t, Ns...>, LayoutLeft,
                    RangeAccessor<std::array<Scalar, (Ns * ...)>>>;
  using ParentType::ParentType;

  constexpr InlineDense() : ParentType({}, {}, {}) {}

  // NOLINTBEGIN(*-avoid-c-arrays)
  template <typename OtherScalar, int64_t... Sizes>
    requires((sizeof...(Ns) == 2) &&
             (Extents<std::size_t, Ns...>::staticExtent(0) ==
              sizeof...(Sizes)) &&
             ((Extents<std::size_t, Ns...>::staticExtent(1) == Sizes) && ...))
  constexpr InlineDense(const OtherScalar (&... rows)[Sizes])
      : ParentType({}, {}, {}) {
    this->initHelper(rows...);
  }
  // NOLINTEND(*-avoid-c-arrays)
};

// NOLINTBEGIN(*-avoid-c-arrays)
template <typename Scalar, int64_t First, int64_t... Sizes>
InlineDense(const Scalar (&fst)[First], const Scalar (&... rows)[Sizes])
  -> InlineDense<Scalar, sizeof...(Sizes) + 1, First>;
// NOLINTEND(*-avoid-c-arrays)

template <typename Scalar, std::size_t First, std::size_t... Ns>
  requires((First == Ns) && ...)
class Identity : public GenericMatrix<Extents<std::size_t, First, Ns...>,
                               LayoutPassthrough, IdentityAccessor<Scalar>> {
public:
  using ParentType = GenericMatrix<Extents<std::size_t, First, Ns...>,
                                   LayoutPassthrough, IdentityAccessor<Scalar>>;
  using ParentType::ParentType;

  constexpr Identity() : ParentType({}, {}, {}) {};
};

// Fully dynamic 2D matrix with runtime-determined dimensions.
// Uses column-major (LayoutLeft) storage.
template <typename Scalar>
class DynamicDense
    : public GenericMatrix<Extents<std::size_t, kDynamic, kDynamic>, LayoutLeft,
                           RangeAccessor<std::vector<Scalar>>> {
 public:
  using ExtentsType = Extents<std::size_t, kDynamic, kDynamic>;
  using ParentType =
      GenericMatrix<ExtentsType, LayoutLeft,
                    RangeAccessor<std::vector<Scalar>>>;
  using MappingType = typename ParentType::MappingType;

  // Construct with runtime dimensions
  DynamicDense(std::size_t rows, std::size_t cols)
      : ParentType{ExtentsType{rows, cols}, MappingType{ExtentsType{rows, cols}},
                   std::vector<Scalar>(rows * cols)} {}

  // Resize (reallocates if needed)
  void resize(std::size_t rows, std::size_t cols) {
    *this = DynamicDense(rows, cols);
  }

  // Reserve capacity without changing dimensions.
  // Reserves space for max_rows * cols() elements.
  void reserveRows(std::size_t max_rows) {
    this->data().reserve(max_rows * this->cols());
  }

  // Append a row (grows the matrix).
  // For column-major layout, this requires reshuffling data.
  void pushRow(std::span<const Scalar> row) {
    assert(row.size() == this->cols());
    std::size_t old_rows = this->rows();
    std::size_t num_cols = this->cols();
    std::size_t new_rows = old_rows + 1;

    // Create new matrix with one more row
    DynamicDense new_mat(new_rows, num_cols);

    // Copy existing data (column by column for column-major)
    for (std::size_t j = 0; j < num_cols; ++j) {
      for (std::size_t i = 0; i < old_rows; ++i) {
        new_mat[i, j] = (*this)[i, j];
      }
      new_mat[old_rows, j] = row[j];
    }

    *this = std::move(new_mat);
  }
};

}  // namespace tempura::matrix3
