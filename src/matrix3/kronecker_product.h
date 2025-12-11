#pragma once

#include <cassert>

#include "matrix3/extents.h"
#include "matrix3/layouts.h"
#include "matrix3/matrix.h"

namespace tempura::matrix3 {

// Accessor that computes Kronecker product elements on-the-fly
// Given A ⊗ B, element (i,j) = A[i/m₂, j/n₂] * B[i%m₂, j%n₂]
// where m₂, n₂ are dimensions of B
template <typename Lhs, typename Rhs>
class KroneckerAccessor {
 public:
  constexpr KroneckerAccessor(const Lhs& lhs, const Rhs& rhs)
      : lhs_{lhs}, rhs_{rhs} {}

  template <typename IndexType>
  constexpr auto operator()(IndexType i, IndexType j) const {
    const auto rhs_rows = rhs_.extent().extent(0);
    const auto rhs_cols = rhs_.extent().extent(1);

    const auto lhs_row = i / rhs_rows;
    const auto rhs_row = i % rhs_rows;
    const auto lhs_col = j / rhs_cols;
    const auto rhs_col = j % rhs_cols;

    return lhs_[lhs_row, lhs_col] * rhs_[rhs_row, rhs_col];
  }

 private:
  const Lhs& lhs_;
  const Rhs& rhs_;
};

// Compute static extents for Kronecker product
// A(m₁×n₁) ⊗ B(m₂×n₂) = C(m₁·m₂ × n₁·n₂)
template <typename LhsExtents, typename RhsExtents>
constexpr auto kroneckerExtents() {
  static_assert(LhsExtents::rank() == 2 && RhsExtents::rank() == 2,
                "Kronecker product requires rank-2 matrices");

  constexpr auto lhs_rows = LhsExtents::staticExtent(0);
  constexpr auto lhs_cols = LhsExtents::staticExtent(1);
  constexpr auto rhs_rows = RhsExtents::staticExtent(0);
  constexpr auto rhs_cols = RhsExtents::staticExtent(1);

  // Result dimensions are products of input dimensions
  constexpr auto out_rows =
      (lhs_rows == kDynamic || rhs_rows == kDynamic) ? kDynamic : lhs_rows * rhs_rows;
  constexpr auto out_cols =
      (lhs_cols == kDynamic || rhs_cols == kDynamic) ? kDynamic : lhs_cols * rhs_cols;

  return Extents<std::size_t, out_rows, out_cols>{};
}

// Custom layout that passes through 2D indices to the Kronecker accessor
struct KroneckerLayout {
  template <typename ExtentT, typename IndexT>
  class Mapping {
   public:
    using ExtentsType = ExtentT;
    using IndexType = IndexT;

    constexpr Mapping() = default;
    constexpr explicit Mapping(const ExtentsType& /*unused*/) noexcept {}

    // Pass through (i, j) as a pair for the accessor
    template <typename I1, typename I2>
    constexpr auto operator()(I1 i, I2 j) const {
      return std::pair{i, j};
    }
  };
};

// Accessor adapter that unpacks the (i,j) pair from KroneckerLayout
template <typename KroneckerAcc>
class KroneckerAccessorAdapter {
 public:
  constexpr explicit KroneckerAccessorAdapter(KroneckerAcc acc) : acc_{acc} {}

  template <typename IndexPair>
  constexpr auto operator()(this auto&& self, const IndexPair& pair)
      -> decltype(auto) {
    return self.acc_(pair.first, pair.second);
  }

 private:
  KroneckerAcc acc_;
};

// Kronecker product expression template
// Lazily evaluates A ⊗ B without materializing the full result
template <typename Lhs, typename Rhs>
class KroneckerProduct
    : public GenericMatrix<
          decltype(kroneckerExtents<typename Lhs::ExtentsType,
                                     typename Rhs::ExtentsType>()),
          KroneckerLayout,
          KroneckerAccessorAdapter<KroneckerAccessor<Lhs, Rhs>>> {
 public:
  using ExtentsType = decltype(kroneckerExtents<typename Lhs::ExtentsType,
                                                  typename Rhs::ExtentsType>());
  using ParentType = GenericMatrix<
      ExtentsType, KroneckerLayout,
      KroneckerAccessorAdapter<KroneckerAccessor<Lhs, Rhs>>>;

  constexpr KroneckerProduct(const Lhs& lhs, const Rhs& rhs)
      : ParentType{computeExtents(lhs, rhs), {},
                   KroneckerAccessorAdapter{KroneckerAccessor{lhs, rhs}}} {
    static_assert(Lhs::ExtentsType::rank() == 2 &&
                  Rhs::ExtentsType::rank() == 2,
                  "Kronecker product requires 2D matrices");
  }

 private:
  static constexpr auto computeExtents(const Lhs& lhs, const Rhs& rhs) {
    const auto lhs_rows = lhs.extent().extent(0);
    const auto lhs_cols = lhs.extent().extent(1);
    const auto rhs_rows = rhs.extent().extent(0);
    const auto rhs_cols = rhs.extent().extent(1);

    return ExtentsType{lhs_rows * rhs_rows, lhs_cols * rhs_cols};
  }
};

// Convenient function for Kronecker product: kronecker(A, B)
template <typename Lhs, typename Rhs>
  requires(Lhs::ExtentsType::rank() == 2 && Rhs::ExtentsType::rank() == 2)
constexpr auto kronecker(const Lhs& lhs, const Rhs& rhs) {
  return KroneckerProduct<Lhs, Rhs>{lhs, rhs};
}

}  // namespace tempura::matrix3
