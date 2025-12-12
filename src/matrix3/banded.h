#pragma once

#include <cassert>
#include <cstdint>
#include <type_traits>
#include <utility>

#include "matrix3/matrix.h"
#include "matrix3/matrix_traits.h"

namespace tempura::matrix3 {

// Banded is a wrapper that interprets a dense matrix's columns as "bands"
// around the diagonal of a square matrix.
//
// Storage layout (3 bands, center=1):
//   ⎡ X 1 2 ⎤
//   ⎢ 3 4 5 ⎥
//   ⎣ 6 7 X ⎦
//
// Represents this banded matrix:
//   ⎡ 1 2 0 ⎤
//   ⎢ 3 4 5 ⎥
//   ⎣ 0 6 7 ⎦
//
// Band calculation: band = col - row + kCenterBand
// Out-of-band elements return a reference to kZero member (reads as zero).
//
// DANGER: Writing to out-of-band elements is undefined behavior.
template <typename ChildType, int64_t CenterBand = MatrixTraits<ChildType>::kCols / 2>
  requires(CenterBand >= 0 && CenterBand < static_cast<int64_t>(MatrixTraits<ChildType>::kCols))
class Banded {
 public:
  using ValueType = typename MatrixTraits<ChildType>::ValueType;
  static constexpr int64_t kRows = MatrixTraits<ChildType>::kRows;
  static constexpr int64_t kCols = MatrixTraits<ChildType>::kRows;  // Square matrix
  static constexpr int64_t kBands = MatrixTraits<ChildType>::kCols;
  static constexpr int64_t kCenterBand = CenterBand;

  constexpr Banded() = default;
  constexpr explicit Banded(const ChildType& mat) : mat_{mat} {}
  constexpr explicit Banded(ChildType&& mat) : mat_{std::move(mat)} {}

  // Access element (i, j) of the banded matrix
  // Out-of-band elements return reference to kZero
  template <typename... Indices>
    requires(sizeof...(Indices) == 2)
  constexpr auto operator[](this auto&& self, Indices... indices)
      -> decltype(self.mat_[0, 0]) {
    auto [i, j] = std::tuple{indices...};
    if (std::is_constant_evaluated()) {
      assert(i >= 0 && i < kRows && "row index out of bounds");
      assert(j >= 0 && j < kCols && "col index out of bounds");
    }
    int64_t band = j - i + CenterBand;
    if (band < 0 || band >= kBands) {
      return self.kZero;
    }
    return self.mat_[i, band];
  }

  // Extent accessors for compatibility
  static constexpr auto rows() -> int64_t { return kRows; }
  static constexpr auto cols() -> int64_t { return kCols; }

  constexpr auto data() const -> const ChildType& { return mat_; }

 private:
  ValueType kZero{0};  // Out-of-band elements return reference to this
  ChildType mat_;
};

// Deduction guide for Banded with default center band
template <typename Scalar, std::size_t... Ns>
Banded(const InlineDense<Scalar, Ns...>&)
    -> Banded<InlineDense<Scalar, Ns...>, MatrixTraits<InlineDense<Scalar, Ns...>>::kCols / 2>;

// Factory function with explicit center band
template <int64_t CenterBand, typename M>
constexpr auto makeBanded(M&& mat)
    -> Banded<std::remove_cvref_t<M>, CenterBand> {
  return Banded<std::remove_cvref_t<M>, CenterBand>{std::forward<M>(mat)};
}

}  // namespace tempura::matrix3
