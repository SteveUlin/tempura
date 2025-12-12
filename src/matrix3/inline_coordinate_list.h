#pragma once

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
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
  constexpr auto insert(int64_t row, int64_t col, T value) -> bool {
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
  std::array<int64_t, Capacity> row_indices_ = {};
  std::array<int64_t, Capacity> col_indices_ = {};
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
    int64_t i;
    int64_t j;
    Scalar value;
  };

  struct Entry {
    std::size_t row;
    std::size_t col;
    Scalar value;
  };

  // Iterator for non-zero entries
  class Iterator {
   public:
    using value_type = Entry;
    using difference_type = std::ptrdiff_t;
    using reference = Entry;
    using pointer = void;
    using iterator_category = std::forward_iterator_tag;

    constexpr Iterator() = default;
    constexpr Iterator(const CoordinateListAccessor<Scalar, Capacity>* accessor,
                       std::size_t index)
        : accessor_{accessor}, index_{index} {}

    constexpr auto operator*() const -> Entry {
      return Entry{
          static_cast<std::size_t>(accessor_->rowIndices()[index_]),
          static_cast<std::size_t>(accessor_->colIndices()[index_]),
          accessor_->values()[index_]};
    }

    constexpr auto operator++() -> Iterator& {
      ++index_;
      return *this;
    }

    constexpr auto operator++(int) -> Iterator {
      Iterator tmp = *this;
      ++index_;
      return tmp;
    }

    constexpr auto operator==(const Iterator& other) const -> bool {
      return index_ == other.index_;
    }

    constexpr auto operator!=(const Iterator& other) const -> bool {
      return index_ != other.index_;
    }

   private:
    const CoordinateListAccessor<Scalar, Capacity>* accessor_ = nullptr;
    std::size_t index_ = 0;
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
    auto coords = std::tuple{indices...};
    if (std::is_constant_evaluated()) {
      auto [row, col] = coords;
      assert(0 <= row && static_cast<std::size_t>(row) < Rows &&
             "row index out of bounds");
      assert(0 <= col && static_cast<std::size_t>(col) < Cols &&
             "col index out of bounds");
    }
    return self.accessor_(coords);
  }

  // Insert operations for modifying sparse matrix
  constexpr auto insert(Triplet triplet) -> bool {
    return accessor_.insert(triplet.i, triplet.j, std::move(triplet.value));
  }

  constexpr auto insert(int64_t row, int64_t col, Scalar value) -> bool {
    return accessor_.insert(row, col, std::move(value));
  }

  constexpr auto size() const -> std::size_t { return accessor_.size(); }

  constexpr auto capacity() const -> std::size_t {
    return accessor_.capacity();
  }

  // Extent accessors for compatibility
  static constexpr auto rows() -> std::size_t { return Rows; }
  static constexpr auto cols() -> std::size_t { return Cols; }

  // Iteration over non-zero entries
  constexpr auto begin() const -> Iterator {
    return Iterator{&accessor_, 0};
  }

  constexpr auto end() const -> Iterator {
    return Iterator{&accessor_, accessor_.size()};
  }

 private:
  CoordinateListAccessor<Scalar, Capacity> accessor_;
};

}  // namespace tempura::matrix3
