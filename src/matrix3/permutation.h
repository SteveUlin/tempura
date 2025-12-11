#pragma once

#include <array>
#include <cassert>
#include <cstdint>
#include <initializer_list>
#include <ranges>
#include <tuple>
#include <type_traits>
#include <vector>

#include "matrix3/extents.h"

namespace tempura::matrix3 {

// Permutation matrix with compact representation: stores array where
// order_[j] = i means P[i,j] = 1 (row index stored at column j).
//
// Uses static storage (std::array) for compile-time size, or dynamic
// storage (std::vector) when Size = kDynamic.
//
// Parity tracking via cycle decomposition enables efficient determinant
// computation. swap() updates parity incrementally.
//
// permuteRows() applies permutation in-place using cycle-following algorithm
// without temporary storage.

template <std::size_t Size = kDynamic>
  requires(Size == kDynamic || Size > 0)
class Permutation {
 public:
  using ValueType = bool;
  static constexpr std::size_t kRows = Size;
  static constexpr std::size_t kCols = Size;

  // Identity permutation (static size)
  constexpr Permutation()
    requires(Size != kDynamic)
  {
    // std::iota not constexpr - manual loop replacement
    for (std::size_t i = 0; i < Size; ++i) {
      order_[i] = static_cast<int64_t>(i);
    }
  }

  // Identity permutation (dynamic size)
  constexpr explicit Permutation(std::size_t size)
    requires(Size == kDynamic)
      : order_(size) {
    for (std::size_t i = 0; i < size; ++i) {
      order_[i] = static_cast<int64_t>(i);
    }
  }

  // Construct from initializer list
  constexpr explicit Permutation(std::initializer_list<int64_t> perm) {
    if constexpr (Size == kDynamic) {
      order_.resize(perm.size());
    }
    std::ranges::copy(perm, order_.begin());
    validate();
  }

  // Read access - returns true if P[row, col] = 1
  template <typename... Indices>
    requires(sizeof...(Indices) == 2)
  constexpr auto operator[](this auto const& self, Indices... indices)
      -> ValueType {
    auto [row, col] = std::tuple{indices...};
    if (std::is_constant_evaluated()) {
      assert(0 <= row && static_cast<std::size_t>(row) < self.order_.size() &&
             "row index out of bounds");
      assert(0 <= col && static_cast<std::size_t>(col) < self.order_.size() &&
             "col index out of bounds");
    }
    return row == self.order_[col];
  }

  // Swap two elements (columns) and update parity
  constexpr void swap(std::size_t i, std::size_t j) {
    parity_ = !parity_;
    std::swap(order_[i], order_[j]);
  }

  // Access underlying permutation array
  constexpr auto data() const -> const auto& { return order_; }

  // Get parity (false = even, true = odd)
  constexpr auto parity() const -> bool { return parity_; }

  // Extent accessors for compatibility
  constexpr auto rows() const -> std::size_t { return order_.size(); }
  constexpr auto cols() const -> std::size_t { return order_.size(); }
  static constexpr auto staticRows() -> std::size_t { return Size; }
  static constexpr auto staticCols() -> std::size_t { return Size; }

  // Apply permutation to matrix rows in-place using cycle-following
  // algorithm. Avoids temporary storage by processing each cycle.
  // Works with any matrix type that provides operator[i,j] and extent().
  template <typename MatrixT>
  constexpr void permuteRows(MatrixT& other) const {
    // Get matrix dimensions via extent() API
    auto matrix_rows = other.extent().extent(0);
    auto matrix_cols = other.extent().extent(1);

    if (std::is_constant_evaluated()) {
      assert(static_cast<std::size_t>(matrix_rows) == order_.size() &&
             "matrix row count must match permutation size");
    }

    std::vector<bool> visited(order_.size());
    for (std::size_t i = 0; i < order_.size(); ++i) {
      if (visited[i]) {
        continue;
      }

      // Follow cycle starting at i
      std::size_t j = i;
      do {
        // Swap rows i and j
        for (int64_t k = 0; k < matrix_cols; ++k) {
          using std::swap;
          swap(other[i, k], other[j, k]);
        }
        visited[j] = true;
        j = static_cast<std::size_t>(order_[j]);
      } while (!visited[j]);
    }
  }

 private:
  // Validate permutation and compute parity via cycle decomposition
  constexpr void validate() {
    parity_ = false;
    std::vector<bool> visited(order_.size());

    for (int64_t element : order_) {
      // Check element is in valid range
      if (std::is_constant_evaluated()) {
        assert(0 <= element &&
               static_cast<std::size_t>(element) < order_.size() &&
               "permutation element out of range");
      }

      if (visited[element]) {
        continue;
      }

      // Found new cycle - toggle parity
      parity_ = !parity_;

      // Follow cycle
      do {
        visited[element] = true;
        parity_ = !parity_;  // Each element in cycle toggles parity
        element = order_[element];
      } while (!visited[element]);
    }

    // Verify all elements were visited (complete permutation)
    for (bool v : visited) {
      if (std::is_constant_evaluated()) {
        assert(v && "permutation must contain all indices exactly once");
      }
    }
  }

  bool parity_ = false;
  std::conditional_t<Size == kDynamic, std::vector<int64_t>,
                     std::array<int64_t, Size>>
      order_ = {};
};

}  // namespace tempura::matrix3
