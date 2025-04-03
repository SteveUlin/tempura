#pragma once

#include <tuple>

#include "matrix3/extents.h"

namespace tempura::matrix3 {

struct LayoutPassthough {
  template <typename ExtentT, typename IndexT>
  class Mapping {
   public:
    template <typename... Indicies>
    static constexpr auto operator()(Indicies... indicies) {
      return std::tuple{indicies...};
    }
  };
};

struct LayoutLeft {
  template <typename ExtentT, typename IndexT>
  class Mapping {
   public:
    using ExtentsType = ExtentT;
    using IndexType = IndexT;

    static_assert(ExtentT::rankDynamic() > 0);

    constexpr explicit Mapping(const ExtentsType& extents) noexcept {
      for (std::size_t i = 0; i < ExtentT::rank(); ++i) {
        stride_[i] = 1;
      }
      for (std::size_t i = 0; i < ExtentT::rank() - 1; ++i) {
        for (std::size_t j = i + 1; j < ExtentT::rank(); ++j) {
          stride_[j] *= extents.extent(i);
        }
      }
    }

    template <typename... Indicies>
      requires((sizeof...(Indicies) == ExtentsType::rank()) &&
               (std::is_convertible_v<Indicies, IndexType> && ...) &&
               (std::is_nothrow_constructible_v<Indicies, IndexType> && ...))
    constexpr auto operator()(Indicies... indicies) const -> IndexType {
      return [&]<auto... Is>(std::index_sequence<Is...>) {
        return (0 + ... + (indicies * stride_[Is]));
      }(std::make_index_sequence<ExtentT::rank()>{});
    }

   private:
    std::array<IndexType, ExtentsType::rank()> stride_;
  };

  template <typename ExtentT, typename IndexT>
    requires(ExtentT::rankDynamic() == 0)
  class Mapping<ExtentT, IndexT> {
   public:
    using ExtentsType = ExtentT;
    using IndexType = IndexT;

    constexpr Mapping() = default;

    constexpr explicit Mapping(const ExtentsType& /*unused*/) noexcept {}

    template <typename... Indicies>
      requires((sizeof...(Indicies) == ExtentsType::rank()) &&
               (std::is_convertible_v<Indicies, IndexType> && ...) &&
               (std::is_nothrow_constructible_v<Indicies, IndexType> && ...))
    constexpr auto operator()(Indicies... indicies) const -> IndexType {
      return [&]<auto... Is>(std::index_sequence<Is...>) {
        return (0 + ... + (indicies * stride_[Is]));
      }(std::make_index_sequence<ExtentT::rank()>{});
    }

   private:
    static constexpr std::array<IndexType, ExtentsType::rank()> stride_ = [] {
      std::array<IndexType, ExtentsType::rank()> stride;
      for (std::size_t i = 0; i < ExtentT::rank(); ++i) {
        stride[i] = 1;
      }
      for (std::size_t i = 0; i < ExtentT::rank() - 1; ++i) {
        for (std::size_t j = i + 1; j < ExtentT::rank(); ++j) {
          stride[j] *= ExtentT::staticExtent(i);
        }
      }
      return stride;
    }();
  };

  template <typename ExtentT>
  Mapping(const ExtentT&) -> Mapping<ExtentT, typename ExtentT::IndexType>;
};

struct LayoutRight {
  template <typename ExtentT, typename IndexT = ExtentT::IndexType>
  class Mapping {
   public:
    using ExtentsType = ExtentT;
    using IndexType = IndexT;

    constexpr explicit Mapping(const ExtentsType& extents) noexcept {
      for (std::size_t i = 0; i < extents.rank(); ++i) {
        stride_[i] = 1;
      }
      for (std::size_t i = 0; i < extents.rank() - 1; ++i) {
        for (std::size_t j = i + 1; j < extents.rank(); ++j) {
          stride_[extents.rank() - 1 - j] *=
              extents.extent(extents.rank() - 1 - i);
        }
      }
    }

    template <typename... Indicies>
      requires((sizeof...(Indicies) == ExtentsType::rank()) &&
               (std::is_convertible_v<Indicies, IndexType> && ...) &&
               (std::is_nothrow_constructible_v<Indicies, IndexType> && ...))
    constexpr auto operator()(Indicies... indicies) const -> IndexType {
      return [&]<auto... Is>(std::index_sequence<Is...>) {
        return (0 + ... + (indicies * stride_[Is]));
      }(std::make_index_sequence<ExtentT::rank()>{});
    }

   private:
    std::array<IndexType, ExtentsType::rank()> stride_;
  };

  template <typename ExtentT, typename IndexT>
    requires(ExtentT::rankDynamic() == 0)
  class Mapping<ExtentT, IndexT> {
   public:
    using ExtentsType = ExtentT;
    using IndexType = IndexT;

    constexpr Mapping() = default;

    constexpr explicit Mapping(const ExtentsType& /*unused*/) noexcept {}

    template <typename... Indicies>
      requires((sizeof...(Indicies) == ExtentsType::rank()) &&
               (std::is_convertible_v<Indicies, IndexType> && ...) &&
               (std::is_nothrow_constructible_v<Indicies, IndexType> && ...))
    constexpr auto operator()(Indicies... indicies) const -> IndexType {
      return [&]<auto... Is>(std::index_sequence<Is...>) {
        return (0 + ... + (indicies * stride_[Is]));
      }(std::make_index_sequence<ExtentT::rank()>{});
    }

   private:
    static constexpr std::array<IndexType, ExtentsType::rank()> stride_ = [] {
      std::array<IndexType, ExtentsType::rank()> stride;
      for (std::size_t i = 0; i < ExtentsType::rank(); ++i) {
        stride[i] = 1;
      }
      for (std::size_t i = 0; i < ExtentsType::rank() - 1; ++i) {
        for (std::size_t j = i + 1; j < ExtentsType::rank(); ++j) {
          stride[ExtentsType::rank() - 1 - j] *=
              ExtentsType::staticExtent(ExtentsType::rank() - 1 - i);
        }
      }
      return stride;
    }();
  };
};

}  // namespace tempura::matrix3
