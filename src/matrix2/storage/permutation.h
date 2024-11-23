#pragma once

#include <bitset>
#include <numeric>
#include <vector>

#include "matrix2/matrix.h"

namespace tempura::matrix {

template <Extent N>
class Permutation;

template <>
class Permutation<kDynamic> {
 public:
  using ValueType = bool;
  static constexpr Extent kRow = kDynamic;
  static constexpr Extent kCol = kDynamic;

  constexpr Permutation(int64_t size) : order_(size) {
    // std::ranges::iota(order_, 0);
    std::iota(order_.begin(), order_.end(), 0);
  }

  constexpr Permutation(std::initializer_list<int64_t> perm) {
    order_.reserve(perm.size());
    std::ranges::copy(perm, std::back_inserter(order_));
    validate();
  }

  constexpr auto operator=(std::initializer_list<int64_t> perm)
      -> Permutation& {
    CHECK(perm.size() == order_.size());
    std::ranges::copy(perm, order_.begin());
    validate();
    return *this;
  }

  constexpr auto shape() const -> RowCol {
    return {.row = static_cast<int64_t>(order_.size()),
            .col = static_cast<int64_t>(order_.size())};
  }

  constexpr auto operator[](int64_t row, int64_t col) const -> ValueType {
    if (std::is_constant_evaluated()) {
      CHECK((0 <= row) and (row < static_cast<int64_t>(order_.size())));
      CHECK((0 <= col) and (col < static_cast<int64_t>(order_.size())));
    }
    return row == order_[col];
  }

  constexpr void swap(int64_t i, int64_t j) { std::swap(order_[i], order_[j]); }

  constexpr auto data() const -> const std::vector<int64_t>& { return order_; }

 private:
  constexpr void validate() {
    std::vector<bool> visited(order_.size());
    for (int64_t elemet : order_) {
      CHECK(0 <= elemet and elemet < static_cast<int64_t>(order_.size()));
      visited[elemet] = true;
    }
    for (bool v : visited) {
      CHECK(v);
    }
  }
  std::vector<int64_t> order_;
  ;
};
static_assert(MatrixT<Permutation<kDynamic>>);

template <Extent N>
  requires(N > 0 and N != kDynamic)
class Permutation<N> {
 public:
  using ValueType = bool;
  static constexpr int64_t kRow = N;
  static constexpr int64_t kCol = N;

  constexpr Permutation() {
    // std::ranges::iota(order_, 0);
    std::iota(order_.begin(), order_.end(), 0);
  }

  constexpr Permutation(std::initializer_list<int64_t> perm) {
    CHECK(perm.size() == N);
    std::ranges::copy(perm, order_.begin());
    validate();
  }

  constexpr auto operator=(std::initializer_list<int64_t> perm)
      -> Permutation& {
    CHECK(perm.size() == N);
    std::ranges::copy(perm, order_.begin());
    validate();
    return *this;
  }

  constexpr auto shape() const -> RowCol { return {.row = N, .col = N}; }

  constexpr auto operator[](int64_t row, int64_t col) const -> ValueType {
    if (std::is_constant_evaluated()) {
      CHECK((0 <= row) and (row < kRow));
      CHECK((0 <= col) and (col < kCol));
    }
    return row == order_[col];
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

}  // namespace tempura::matrix
