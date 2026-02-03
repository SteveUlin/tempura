#pragma once

#include <cstddef>
#include <tuple>
#include <type_traits>
#include <utility>

#include "symbolic4/core.h"
#include "symbolic4/distributions/indexed_node.h"
#include "symbolic4/distributions/random_var.h"
#include "symbolic4/interpreter/diff.h"
#include "symbolic4/interpreter/simplify.h"
#include "symbolic4/mcmc/plate_transforms.h"
#include "symbolic4/mcmc/support.h"
#include "symbolic4/mcmc/transforms.h"

// ============================================================================
// model.h - Unified model specification with automatic parameter handling
// ============================================================================
//
// The Model class provides a cleaner API for probabilistic programming by
// automatically managing random variables and their relationships.
//
// Key features:
//   1. Automatic joint log-probability computation
//   2. Automatic parameter extraction (no manual listing)
//   3. Automatic transform inference from distributions
//   4. Single unified posterior() API
//   5. Support for both scalar RVs and plates (indexed RVs)
//
// Usage (scalar only):
//   auto mu = normal(lit(0), lit(10));
//   auto sigma = halfNormal(lit(5));
//   auto y = normal(mu, sigma);
//   auto m = model(mu, sigma, y);
//
//   auto posterior = m.posterior()
//       .observe(y = 3.5)
//       .build();
//
// Usage (with plates):
//   auto alpha = gamma(lit(2.0), lit(0.1));
//   auto beta_param = gamma(lit(2.0), lit(0.1));
//   auto theta = plate<Countries>(beta(alpha, beta_param));
//   auto m = model(alpha, beta_param, theta);
//
//   auto posterior = m.posterior()
//       .withDimension<Countries>(38)
//       .build();
//
// ============================================================================

namespace tempura::symbolic4 {

// ============================================================================
// Type traits to detect if model contains any indexed (plate) RVs
// ============================================================================

namespace model_detail {

template <typename... RVs>
struct HasIndexedRV;

template <>
struct HasIndexedRV<> : std::false_type {};

template <typename First, typename... Rest>
struct HasIndexedRV<First, Rest...>
    : std::bool_constant<is_indexed_random_var_v<First> || HasIndexedRV<Rest...>::value> {};

template <typename... RVs>
constexpr bool has_indexed_rv_v = HasIndexedRV<RVs...>::value;

}  // namespace model_detail

// Forward declarations
template <typename ModelT, bool HasPlates>
class PosteriorBuilderFromModel;

template <typename ModelT, typename ObsBindings, bool HasPlates>
class ObservedPosteriorBuilder;

// ============================================================================
// Model - Unified container for probabilistic model specification
// ============================================================================

template <typename... RVs>
class Model {
 public:
  static constexpr std::size_t NumRVs = sizeof...(RVs);
  static constexpr bool HasPlates = model_detail::has_indexed_rv_v<RVs...>;

  constexpr Model(RVs... rvs) : rvs_{rvs...} {}

  // Access individual random variables by index
  template <std::size_t I>
  constexpr auto rv() const {
    return std::get<I>(rvs_);
  }

  // Compute joint log-probability (sum of all RV log-probs)
  constexpr auto jointLogProb() const {
    return jointLogProbImpl(std::make_index_sequence<NumRVs>{});
  }

  // Compute unnormalized joint log-probability
  constexpr auto unnormalizedJointLogProb() const {
    return unnormalizedJointLogProbImpl(std::make_index_sequence<NumRVs>{});
  }

  // Extract all parameter symbols as a tuple (free symbols for bindings)
  constexpr auto params() const {
    return std::apply([](auto&... rv) { return std::make_tuple(rv.freeSym()...); }, rvs_);
  }

  // Create a posterior builder for inference
  auto posterior() const { return PosteriorBuilderFromModel<Model, HasPlates>{*this}; }

  // Access the tuple of RVs (for advanced use)
  constexpr const auto& rvs() const { return rvs_; }

 private:
  std::tuple<RVs...> rvs_;

  template <std::size_t... Is>
  constexpr auto jointLogProbImpl(std::index_sequence<Is...>) const {
    return (std::get<Is>(rvs_).logProb() + ...);
  }

  template <std::size_t... Is>
  constexpr auto unnormalizedJointLogProbImpl(std::index_sequence<Is...>) const {
    return (std::get<Is>(rvs_).unnormalizedLogProb() + ...);
  }
};

// Deduction guide
template <typename... RVs>
Model(RVs...) -> Model<RVs...>;

// ============================================================================
// PosteriorBuilderFromModel - Scalar-only version (no plates)
// ============================================================================

template <typename ModelT>
class PosteriorBuilderFromModel<ModelT, false> {
 public:
  using model_type = ModelT;

  constexpr explicit PosteriorBuilderFromModel(const ModelT& model) : model_{model} {}

  // Add observations (variadic for multiple)
  template <typename... Binders>
  auto observe(Binders... binders) const {
    auto obs = BinderPack{binders...};
    return ObservedPosteriorBuilder<ModelT, BinderPack<Binders...>, false>{model_, obs};
  }

  // Build posterior without observations (for prior sampling)
  auto build() const {
    using TupleType = std::remove_cvref_t<decltype(model_.rvs())>;
    return buildPosteriorImpl(model_.jointLogProb(), model_.params(),
                              std::make_index_sequence<std::tuple_size_v<TupleType>>{});
  }

 private:
  ModelT model_;

  template <typename LogProb, typename ParamsTuple, std::size_t... Is>
  auto buildPosteriorImpl(LogProb lp, ParamsTuple params,
                          std::index_sequence<Is...>) const {
    auto transforms = std::make_tuple(autoTransform(std::get<Is>(model_.rvs()))...);
    auto symbols = std::make_tuple(std::get<Is>(params)...);
    auto grads = std::make_tuple(simplify(diff(lp, std::get<Is>(symbols)))...);

    return TransformedPosterior<LogProb, BinderPack<>, decltype(transforms),
                                decltype(symbols), decltype(grads)>{
        lp, BinderPack<>{}, transforms, symbols, grads};
  }
};

// ============================================================================
// PosteriorBuilderFromModel - Plate version (has indexed RVs)
// ============================================================================

template <typename ModelT>
class PosteriorBuilderFromModel<ModelT, true> {
 public:
  using model_type = ModelT;

  constexpr explicit PosteriorBuilderFromModel(const ModelT& model) : model_{model} {}

  // Set dimension for indexed parameters
  template <typename DimTag>
  auto withDimension(std::size_t size) const {
    return WithDimBuilder<DimTag>{model_, size};
  }

  // Add observations
  template <typename... Binders>
  auto observe(Binders... binders) const {
    auto obs = BinderPack{binders...};
    return ObservedPosteriorBuilder<ModelT, BinderPack<Binders...>, true>{model_, obs};
  }

  // Build without dimension - will fail at runtime if plates need sizes
  auto build() const {
    return buildImpl(std::make_index_sequence<ModelT::NumRVs>{});
  }

 private:
  ModelT model_;

  template <std::size_t... Is>
  auto buildImpl(std::index_sequence<Is...>) const {
    auto lp = model_.jointLogProb();
    return makePlateTransformedPosterior(lp, std::get<Is>(model_.rvs())...).build();
  }

  // Helper class for dimension chaining
  template <typename DimTag, typename DataBindings = BinderPack<>>
  class WithDimBuilder {
   public:
    WithDimBuilder(const ModelT& model, std::size_t size, DataBindings data = {})
        : model_{model}, size_{size}, data_bindings_{data} {}

    // Chain more dimensions
    template <typename DimTag2>
    auto withDimension(std::size_t size2) const {
      // For now, support single dimension. Multi-dim would need more infrastructure.
      return WithDimBuilder<DimTag2, DataBindings>{model_, size2, data_bindings_};
    }

    // Bind data (non-parameter indexed values)
    template <typename... Binders>
    auto bindData(Binders... binders) const {
      auto new_data = BinderPack{binders...};
      return WithDimBuilder<DimTag, decltype(new_data)>{model_, size_, new_data};
    }

    // Add observations
    template <typename... Binders>
    auto observe(Binders... binders) const {
      return ObservedWithDimBuilder<DimTag, BinderPack<Binders...>, DataBindings>{
          model_, size_, BinderPack{binders...}, data_bindings_};
    }

    auto build() const {
      return buildImpl(std::make_index_sequence<ModelT::NumRVs>{});
    }

   private:
    ModelT model_;
    std::size_t size_;
    DataBindings data_bindings_;

    template <std::size_t... Is>
    auto buildImpl(std::index_sequence<Is...>) const {
      auto lp = model_.jointLogProb();
      return makePlateTransformedPosterior(lp, std::get<Is>(model_.rvs())...)
          .template withDimension<DimTag>(size_)
          .bindData(data_bindings_)
          .build();
    }
  };

  // Helper for observed + dimension + data
  template <typename DimTag, typename ObsBindings, typename DataBindings = BinderPack<>>
  class ObservedWithDimBuilder {
   public:
    ObservedWithDimBuilder(const ModelT& model, std::size_t size, ObsBindings obs,
                            DataBindings data = {})
        : model_{model}, size_{size}, observations_{obs}, data_bindings_{data} {}

    // Bind data (non-parameter indexed values)
    template <typename... Binders>
    auto bindData(Binders... binders) const {
      auto new_data = BinderPack{binders...};
      return ObservedWithDimBuilder<DimTag, ObsBindings, decltype(new_data)>{
          model_, size_, observations_, new_data};
    }

    auto build() const {
      return buildImpl(std::make_index_sequence<ModelT::NumRVs>{});
    }

   private:
    ModelT model_;
    std::size_t size_;
    ObsBindings observations_;
    DataBindings data_bindings_;

    template <std::size_t... Is>
    auto buildImpl(std::index_sequence<Is...>) const {
      auto lp = model_.jointLogProb();
      // Use the version that filters out observed RVs from param specs
      // The jointLogProb still includes observed RVs' log-prob
      auto posterior = makePlateTransformedPosteriorWithObs(
          lp, observations_, std::get<Is>(model_.rvs())...);
      return posterior
          .template withDimension<DimTag>(size_)
          .withData(data_bindings_);
    }
  };
};

// ============================================================================
// ObservedPosteriorBuilder - Scalar version
// ============================================================================

template <typename ModelT, typename ObsBindings>
class ObservedPosteriorBuilder<ModelT, ObsBindings, false> {
 public:
  using model_type = ModelT;

  constexpr ObservedPosteriorBuilder(const ModelT& model, ObsBindings obs)
      : model_{model}, observations_{obs} {}

  auto build() const {
    using TupleType = std::remove_cvref_t<decltype(model_.rvs())>;
    return buildImpl(std::make_index_sequence<std::tuple_size_v<TupleType>>{});
  }

 private:
  ModelT model_;
  ObsBindings observations_;

  template <std::size_t... Is>
  auto buildImpl(std::index_sequence<Is...>) const {
    auto lp = model_.jointLogProb();
    auto transforms = std::make_tuple(autoTransform(std::get<Is>(model_.rvs()))...);
    // Use freeSym() for binding compatibility
    auto symbols = std::make_tuple(std::get<Is>(model_.rvs()).freeSym()...);
    auto grads = std::make_tuple(simplify(diff(lp, std::get<Is>(symbols)))...);

    return TransformedPosterior<decltype(lp), ObsBindings, decltype(transforms),
                                decltype(symbols), decltype(grads)>{lp, observations_,
                                                                     transforms, symbols,
                                                                     grads};
  }
};

// ============================================================================
// ObservedPosteriorBuilder - Plate version
// ============================================================================

template <typename ModelT, typename ObsBindings>
class ObservedPosteriorBuilder<ModelT, ObsBindings, true> {
 public:
  using model_type = ModelT;

  constexpr ObservedPosteriorBuilder(const ModelT& model, ObsBindings obs)
      : model_{model}, observations_{obs} {}

  // Set dimension for indexed parameters
  template <typename DimTag>
  auto withDimension(std::size_t size) const {
    return WithDimObservedBuilder<DimTag>{model_, observations_, size};
  }

  auto build() const {
    return buildImpl(std::make_index_sequence<ModelT::NumRVs>{});
  }

 private:
  ModelT model_;
  ObsBindings observations_;

  template <std::size_t... Is>
  auto buildImpl(std::index_sequence<Is...>) const {
    auto lp = model_.jointLogProb();
    // Note: observations need to be integrated with plate posterior
    return makePlateTransformedPosterior(lp, std::get<Is>(model_.rvs())...).build();
  }

  template <typename DimTag>
  class WithDimObservedBuilder {
   public:
    WithDimObservedBuilder(const ModelT& model, ObsBindings obs, std::size_t size)
        : model_{model}, observations_{obs}, size_{size} {}

    auto build() const {
      return buildImpl(std::make_index_sequence<ModelT::NumRVs>{});
    }

   private:
    ModelT model_;
    ObsBindings observations_;
    std::size_t size_;

    template <std::size_t... Is>
    auto buildImpl(std::index_sequence<Is...>) const {
      auto lp = model_.jointLogProb();
      return makePlateTransformedPosterior(lp, std::get<Is>(model_.rvs())...)
          .template withDimension<DimTag>(size_)
          .build();
    }
  };
};

// ============================================================================
// Factory function - model(rv1, rv2, ...)
// ============================================================================

template <typename... RVs>
constexpr auto model(RVs... rvs) {
  return Model{rvs...};
}

// ============================================================================
// Convenience: params(rv1, rv2, ...) - extract parameter symbols
// ============================================================================

template <typename... RVs>
constexpr auto params(const RVs&... rvs) {
  // Use freeSym() for binding compatibility
  return std::make_tuple(rvs.freeSym()...);
}

}  // namespace tempura::symbolic4
