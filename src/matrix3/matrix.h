#pragma once

#include <cstddef>
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

  template <typename... Indicies>
    requires(sizeof...(Indicies) == ExtentsT::rank())
  constexpr auto operator[](this auto&& self, Indicies... indicies)
      -> decltype(auto) {
    return self.accessor_(self.layout_(indicies...));
  }

  auto extent() const -> const ExtentsT& { return extent_; }

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
                               LayoutPassthough, IdentityAccessor<Scalar>> {
public:
  using ParentType = GenericMatrix<Extents<std::size_t, First, Ns...>,
                                   LayoutPassthough, IdentityAccessor<Scalar>>;
  using ParentType::ParentType;

  constexpr Identity() : ParentType({}, {}, {}) {};
};

}  // namespace tempura::matrix3
