#pragma once

#include <bitset>
#include <numeric>

#include "matrix2/matrix.h"

namespace tempura::matrix {

template <int64_t N>
  requires(N > 0)
class Permutation {
 public:
  using ValueType = bool;
  static constexpr int64_t kRow = N;
  static constexpr int64_t kCol = N;

  constexpr Permutation() {
    // Not implemented yet.
    // std::ranges::iota(order_, 0);
    std::iota(order_.begin(), order_.end(), 0);
  }

  constexpr Permutation(std::initializer_list<int64_t> perm) {
    CHECK(perm.size() == N);
    std::ranges::copy(perm, order_.begin());
    validate();
  }

  constexpr auto operator=(std::initializer_list<int64_t> perm) -> Permutation& {
    CHECK(perm.size() == N);
    std::ranges::copy(perm, order_.begin());
    validate();
    return *this;
  }

  constexpr auto shape() const -> RowCol { return {.row = N, .col = N}; }

  constexpr auto operator[](int64_t i, int64_t j) const -> ValueType {
    return i == order_[j];
  }

  constexpr void swap(int64_t i, int64_t j) { std::swap(order_[i], order_[j]); }

  constexpr auto data() const -> const std::array<int64_t, N>& {
    return order_;
  }

 private:
  constexpr void validate() {
    std::bitset<N> visited;
    for (int64_t elemet : order_) {
      CHECK(0 <= elemet and elemet < N);
      visited[elemet] = true;
    }
    CHECK(visited.all());
  }
  std::array<int64_t, N> order_ = {};
};
static_assert(MatrixT<Permutation<4>>);

}  // namespace tempura::matrix
