#pragma once

#include <array>
#include <cassert>
#include <cstddef>
#include <initializer_list>
#include <span>
#include <type_traits>
#include <utility>
#include <vector>

namespace tempura {

// A permutation of {0, …, n−1}, stored compactly as the index map `order_`:
// order_[col] == row means the dense matrix has P[row, col] = 1. Applying it
// therefore SCATTERS — (P·A) sends source row `col` to destination `order_[col]`.
// (The convention is the bug-prone part of any permutation type; it is fixed
// here and every operation below follows it.) Static size uses std::array;
// Size == std::dynamic_extent uses std::vector.
//
// Parity is tracked incrementally — swap() toggles it — so determinantSign() is
// O(1). That sign is what LU/QR pivoting needs for det(A) = ±∏ Uᵢᵢ.
template <std::size_t Size = std::dynamic_extent>
  requires(Size == std::dynamic_extent || Size > 0)
class Permutation {
 public:
  using value_type = bool;

  // Identity (static size).
  constexpr Permutation()
    requires(Size != std::dynamic_extent)
  {
    for (std::size_t i = 0; i < Size; ++i) order_[i] = i;
  }

  // Identity (dynamic size).
  constexpr explicit Permutation(std::size_t size)
    requires(Size == std::dynamic_extent)
      : order_(size) {
    for (std::size_t i = 0; i < size; ++i) order_[i] = i;
  }

  // From an explicit index map; asserts it is a genuine permutation.
  constexpr explicit Permutation(std::initializer_list<std::size_t> perm) {
    if constexpr (Size == std::dynamic_extent) {
      order_.resize(perm.size());
    } else {
      assert(perm.size() == Size);
    }
    std::size_t i = 0;
    for (std::size_t v : perm) order_[i++] = v;
    validate();
  }

  // Dense element: P[row, col] = 1 iff order_[col] == row.
  constexpr auto operator[](std::size_t row, std::size_t col) const -> bool {
    assert(row < size() && col < size());
    return order_[col] == row;
  }

  // Compose with the transposition (i j); flips parity.
  constexpr void swap(std::size_t i, std::size_t j) {
    assert(i < size() && j < size());
    parity_ = !parity_;
    using std::swap;
    swap(order_[i], order_[j]);
  }

  // Apply to a matrix's rows in place: m ← P·m. Cycle-following, so it needs no
  // temporary copy of the matrix. m must have size() rows; m[i,j] and m.extent(r)
  // are the only requirements (Matrix, InlineMatrix, and mdspan views qualify).
  template <typename M>
    requires requires(M& m) {
      m.extent(0);
      m[std::size_t{0}, std::size_t{0}];
    }
  constexpr void permuteRows(M& m) const {
    const std::size_t n = size();
    assert(static_cast<std::size_t>(m.extent(0)) == n);
    const std::size_t cols = static_cast<std::size_t>(m.extent(1));
    auto visited = makeFlags(n);
    for (std::size_t i = 0; i < n; ++i) {
      if (visited[i]) continue;
      std::size_t j = i;
      do {
        if (i != j) {
          for (std::size_t k = 0; k < cols; ++k) {
            using std::swap;
            swap(m[i, k], m[j, k]);
          }
        }
        visited[j] = true;
        j = order_[j];
      } while (!visited[j]);
    }
  }

  constexpr auto data() const -> const auto& { return order_; }
  // Non-owning view of the index map, for building a permuted mdspan layout.
  constexpr auto span() const -> std::span<const std::size_t> {
    return {order_.data(), order_.size()};
  }
  constexpr auto parity() const -> bool { return parity_; }
  constexpr auto determinantSign() const -> int { return parity_ ? -1 : 1; }
  constexpr auto size() const -> std::size_t { return order_.size(); }

 private:
  // visited buffer: a stack array for static size, a heap vector for dynamic.
  static constexpr auto makeFlags(std::size_t n) {
    if constexpr (Size == std::dynamic_extent) {
      return std::vector<bool>(n);
    } else {
      return std::array<bool, Size>{};
    }
  }

  // Parity via cycle decomposition: a cycle of length k is (k−1) transpositions,
  // so each even-length cycle flips parity. Also asserts order_ is a permutation.
  constexpr void validate() {
    const std::size_t n = size();
    parity_ = false;
    auto visited = makeFlags(n);
    for (std::size_t start = 0; start < n; ++start) {
      assert(order_[start] < n && "permutation index out of range");
      if (visited[start]) continue;
      std::size_t i = start;
      std::size_t len = 0;
      do {
        visited[i] = true;
        i = order_[i];
        ++len;
      } while (!visited[i]);
      if ((len & 1U) == 0U) parity_ = !parity_;
    }
    for (std::size_t i = 0; i < n; ++i) assert(visited[i] && "not a permutation");
  }

  bool parity_ = false;
  std::conditional_t<Size == std::dynamic_extent, std::vector<std::size_t>,
                     std::array<std::size_t, Size>>
      order_{};
};

}  // namespace tempura
