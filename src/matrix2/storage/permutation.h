#pragma once

#include <numeric>
#include <vector>

#include "matrix2/matrix.h"

namespace tempura::matrix {

template <Extent N>
  requires(N == kDynamic or N > 0)
class Permutation {
 public:
  using ValueType = bool;
  static constexpr Extent kRow = kDynamic;
  static constexpr Extent kCol = kDynamic;

  constexpr Permutation()
    requires(N != kDynamic)
  {
    // std::ranges::iota(order_, 0);
    std::iota(order_.begin(), order_.end(), 0);
  }

  constexpr explicit Permutation(int64_t size)
    requires(N == kDynamic)
  {
    if constexpr (N == kDynamic) {
      order_.resize(size);
    }
    // std::ranges::iota(order_, 0);
    std::iota(order_.begin(), order_.end(), 0);
  }

  constexpr explicit Permutation(std::initializer_list<int64_t> perm) {
    if constexpr (N == kDynamic) {
      order_.resize(perm.size());
    }
    std::ranges::copy(perm, order_.begin());
    validate();
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

  constexpr void swap(int64_t i, int64_t j) {
    parity_ = !parity_;
    std::swap(order_[i], order_[j]);
  }

  constexpr auto data() const -> decltype(auto) { return order_; }

  constexpr auto parity() const -> bool { return parity_; }

  template <MatrixT MatT>
    requires(MatT::kRow == N)
  constexpr void permuteRows(MatT& other) const {
    CHECK(other.shape().row == static_cast<int64_t>(order_.size()));
    std::vector<bool> visited(order_.size());
    for (int64_t i = 0; i < static_cast<int64_t>(order_.size()); ++i) {
      if (visited[i]) {
        continue;
      }
      int64_t j = i;
      do {
        for (int64_t k = 0; k < other.shape().col; ++k) {
          using std::swap;
          swap(other[i, k], other[j, k]);
        }
        visited[j] = true;
        j = order_[j];
      } while (!visited[j]);
    }
  }

 private:
  constexpr void validate() {
    parity_ = false;
    std::vector<bool> visited(order_.size());
    for (int64_t element : order_) {
      CHECK(0 <= element and element < static_cast<int64_t>(order_.size()));
      if (visited[element]) {
        continue;
      }
      parity_ = !parity_;
      do {
        visited[element] = true;
        parity_ = !parity_;
        element = order_[element];
      } while (!visited[element]);
    }
    for (bool v : visited) {
      CHECK(v);
    }
  }

  bool parity_ = false;
  std::conditional_t<N == kDynamic, std::vector<int64_t>,
                     std::array<int64_t, static_cast<size_t>(N)>>
      order_ = {};
  ;
};
static_assert(MatrixT<Permutation<kDynamic>>);

}  // namespace tempura::matrix
