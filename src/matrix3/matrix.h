#pragma once

#include <vector>

#include "matrix3/accessors.h"
#include "matrix3/extents.h"
#include "matrix3/layouts.h"

namespace tempura::matrix3 {

template <typename ExtentT, typename LayoutT, typename AccessorT>
class GenericMatrix {
 public:
  constexpr GenericMatrix(ExtentT extent, LayoutT layout, AccessorT accessor)
      : extent_(extent), layout_(layout), accessor_(accessor) {}

  template <typename... Indicies>
    requires(sizeof...(Indicies) == ExtentT::rank())
  constexpr auto operator[](this auto&& self, Indicies... indicies)
      -> decltype(auto) {
    return self.accessor_(self.layout_(indicies...));
  }

  auto extent() const -> const ExtentT& { return extent_; }

 private:
  ExtentT extent_;
  LayoutT layout_;
  AccessorT accessor_;
};


template <typename Scalar, std::size_t... Ns>
class Dense {
 public:
  using ExtentsType = Extents<std::size_t, Ns...>;

  constexpr Dense() = default;

  template <typename... Indicies>
    requires(sizeof...(Indicies) == sizeof...(Ns))
  constexpr auto operator[](this auto&& self, Indicies... indicies)
      -> decltype(auto) {
    return self.accessor_(self.layout_(indicies...));
  }

  constexpr auto extents() const -> ExtentsType { return {}; }

  constexpr auto data() -> decltype(auto) { return accessor_.data(); }

 private:
  LayoutLeft::Mapping<ExtentsType, typename ExtentsType::IndexType> layout_{{}};
  RangeAccessor<std::vector<Scalar>> accessor_{
      std::vector<Scalar>((1 * ... * Ns))};
};

template <typename Scalar, std::size_t... Ns>
class InlineDense {
 public:
  using ExtentsType = Extents<std::size_t, Ns...>;

  constexpr InlineDense() : layout_({}), accessor_({}) {}

  template <typename... Indicies>
    requires(sizeof...(Indicies) == sizeof...(Ns))
  constexpr auto operator[](this auto&& self, Indicies... indicies)
      -> decltype(auto) {
    return self.accessor_(self.layout_(indicies...));
  }

  constexpr auto extents() const -> ExtentsType { return {}; }

  auto data() const -> const auto& { return accessor_.data(); }

 private:
  LayoutLeft::Mapping<ExtentsType, typename ExtentsType::IndexType> layout_{{}};
  RangeAccessor<std::array<Scalar, (1 * ... * Ns)>> accessor_;
};

}  // namespace tempura::matrix3
