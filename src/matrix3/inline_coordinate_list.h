#pragma once

#include <array>
#include <cstddef>
#include <ranges>
#include <tuple>
#include <utility>

#include "matrix3/extents.h"
#include "matrix3/layouts.h"

namespace tempura::matrix3 {

// CoordinateListAccessor manages three arrays for COO sparse format:
// - Row indices
// - Column indices
// - Values
//
// Uses reverse linear search to find most recent value for a coordinate.
// This means last insert wins, but we never remove old duplicates.
template <typename T, std::size_t Capacity>
class CoordinateListAccessor {
 public:
  using ValueType = T;

  constexpr CoordinateListAccessor() = default;

  // Access via (row, col) tuple - returns by value since we can't provide
  // mutable references to missing elements (would need to insert zeros)
  template <typename IndexType>
  constexpr auto operator()(std::tuple<IndexType, IndexType> coords) const
      -> ValueType {
    auto [row, col] = coords;
    // Reverse search to find most recent value
    for (int64_t i = size_ - 1; i >= 0; --i) {
      if (row_indices_[i] == row && col_indices_[i] == col) {
        return values_[i];
      }
    }
    return kZero_;
  }

  // Insert a new element - returns true on success, false if capacity exceeded
  constexpr auto insert(std::size_t row, std::size_t col, T value) -> bool {
    if (size_ >= Capacity) {
      return false;
    }
    row_indices_[size_] = row;
    col_indices_[size_] = col;
    values_[size_] = std::move(value);
    ++size_;
    return true;
  }

  constexpr auto size() const -> std::size_t { return size_; }

  constexpr auto capacity() const -> std::size_t { return Capacity; }

  // Direct access to internal arrays for testing/debugging
  constexpr auto rowIndices() const -> const auto& { return row_indices_; }
  constexpr auto colIndices() const -> const auto& { return col_indices_; }
  constexpr auto values() const -> const auto& { return values_; }

 private:
  static constexpr T kZero_ = ValueType{};

  std::size_t size_ = 0;
  std::array<std::size_t, Capacity> row_indices_ = {};
  std::array<std::size_t, Capacity> col_indices_ = {};
  std::array<T, Capacity> values_ = {};
};

// Sparse matrix using Coordinate List (COO) format
// Note: Unlike dense matrices, COO can't provide mutable references to
// elements, so operator[] returns by value. Use insert() to modify.
template <typename Scalar, std::size_t Rows, std::size_t Cols,
          std::size_t Capacity = (Rows * Cols) / 4>
class InlineCoordinateList {
 public:
  struct Triplet {
    std::size_t i;
    std::size_t j;
    Scalar value;
  };

  constexpr InlineCoordinateList() = default;

  // Constructor from range of triplets
  constexpr explicit InlineCoordinateList(
      std::ranges::input_range auto&& triplets) {
    for (auto&& triplet : triplets) {
      insert(triplet);
    }
  }

  // Read access - returns by value (can't return reference to missing elements)
  template <typename... Indices>
    requires(sizeof...(Indices) == 2)
  constexpr auto operator[](this auto const& self, Indices... indices)
      -> Scalar {
    return self.accessor_(std::tuple{indices...});
  }

  // Insert operations for modifying sparse matrix
  constexpr auto insert(Triplet triplet) -> bool {
    return accessor_.insert(triplet.i, triplet.j, std::move(triplet.value));
  }

  constexpr auto insert(std::size_t row, std::size_t col, Scalar value)
      -> bool {
    return accessor_.insert(row, col, std::move(value));
  }

  constexpr auto size() const -> std::size_t { return accessor_.size(); }

  constexpr auto capacity() const -> std::size_t {
    return accessor_.capacity();
  }

  // Extent accessors for compatibility
  static constexpr auto rows() -> std::size_t { return Rows; }
  static constexpr auto cols() -> std::size_t { return Cols; }

 private:
  CoordinateListAccessor<Scalar, Capacity> accessor_;
};

}  // namespace tempura::matrix3
