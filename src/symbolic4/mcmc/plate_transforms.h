#pragma once

#include <array>
#include <cstddef>
#include <random>
#include <span>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include "bayes/estimation/hmc.h"
#include "meta/type_list_ops.h"
#include "symbolic4/core.h"
#include "symbolic4/distributions/indexed_node.h"
#include "symbolic4/indexed/indexed_eval.h"
#include "symbolic4/strategy/diff.h"
#include "symbolic4/mcmc/samples.h"
#include "symbolic4/mcmc/sampler.h"
#include "symbolic4/mcmc/state.h"
#include "symbolic4/mcmc/support.h"
#include "symbolic4/mcmc/transform_pack.h"
#include "symbolic4/mcmc/transforms.h"

// ============================================================================
// plate_transforms.h - Integration of plates with automatic MCMC transforms
// ============================================================================
//
// This header extends the transform system to handle plate (indexed) parameters.
// The key insight is that plate parameters are collections of values that all
// share the same transform (inferred from the distribution).
//
// Usage:
//   auto alpha = gamma(2.0, 0.1);           // scalar, positive
//   auto theta = plate<Countries>(beta(alpha, lit(3.0)));  // indexed, (0,1)
//
//   auto joint = logProb(alpha, theta);
//
//   // Create posterior that handles both scalar and indexed params
//   auto posterior = makePlateTransformedPosterior(joint, alpha, theta)
//                      .withDimensions(DimSizes<Countries>{38})
//                      .build();
//
//   // State is a flat vector: [z_alpha, z_theta[0], ..., z_theta[37]]
//   std::vector<double> state(39);
//   double lp = posterior.logProb(state);
//   auto grad = posterior.gradient(state);
//
// ============================================================================

namespace tempura::symbolic4 {

// ============================================================================
// DimSizes - Runtime dimension sizes for plates
// ============================================================================

// Helper: Find index of a type in a pack
namespace plate_detail {

template <typename Target, typename... Ts>
struct TypeIndexIn;

template <typename Target, typename First, typename... Rest>
struct TypeIndexIn<Target, First, Rest...> {
  static constexpr std::size_t value =
      std::is_same_v<Target, First> ? 0 : 1 + TypeIndexIn<Target, Rest...>::value;
};

template <typename Target>
struct TypeIndexIn<Target> {
  static constexpr std::size_t value = 0;  // Not found, but should never reach here
};

// Find index of symbol in a symbols tuple
template <typename Sym, typename SymsTuple, std::size_t I = 0>
struct SymbolIndexIn {
  static constexpr std::size_t value = [] {
    if constexpr (I >= std::tuple_size_v<SymsTuple>) {
      return std::size_t(-1);  // Not found
    } else if constexpr (std::is_same_v<Sym, std::tuple_element_t<I, SymsTuple>>) {
      return I;
    } else {
      return SymbolIndexIn<Sym, SymsTuple, I + 1>::value;
    }
  }();
};

}  // namespace plate_detail

// ============================================================================
// GradientResult - Gradient values queryable by symbols
// ============================================================================
//
// Wraps a gradient vector with metadata for type-safe querying by symbol.
// Usage:
//   auto grad = posterior.gradientResult(z);
//   double d_alpha = grad[alpha];  // Query by RandomVar
//
// Also supports array-style access for HMC: grad.values()
//
template <typename SymbolsTuple, typename SpecsTuple>
class GradientResult {
 public:
  GradientResult(std::vector<double> grad, SymbolsTuple symbols, SpecsTuple specs)
      : grad_{std::move(grad)}, symbols_{symbols}, specs_{specs} {
    // Compute offsets for each symbol
    computeOffsets();
  }

  // Access underlying gradient values (for HMC and other algorithms)
  auto values() const -> const std::vector<double>& { return grad_; }
  auto values() -> std::vector<double>& { return grad_; }

  // Array-style element access
  auto operator[](std::size_t i) const -> double { return grad_[i]; }
  auto operator[](std::size_t i) -> double& { return grad_[i]; }

  // Size
  auto size() const -> std::size_t { return grad_.size(); }

  // Query gradient by RandomVar
  // Returns double for scalar params, std::span<const double> for indexed params
  template <typename RV>
    requires requires { typename RV::id_type; }
  auto operator[](const RV& /*rv*/) const {
    constexpr std::size_t idx = findSymbolIndex<RV>();
    static_assert(idx < std::tuple_size_v<SymbolsTuple>, "Symbol not found in posterior");

    using SpecType = std::tuple_element_t<idx, SpecsTuple>;
    if constexpr (!SpecType::is_indexed) {
      return grad_[offsets_[idx]];
    } else {
      return std::span<const double>(grad_.data() + offsets_[idx],
                                      std::get<idx>(specs_).size());
    }
  }

  // Get offset for a RandomVar (useful for indexed params)
  template <typename RV>
    requires requires { typename RV::id_type; }
  auto offset(const RV& /*rv*/) const -> std::size_t {
    constexpr std::size_t idx = findSymbolIndex<RV>();
    static_assert(idx < std::tuple_size_v<SymbolsTuple>, "Symbol not found in posterior");
    return offsets_[idx];
  }

  // Get size for a RandomVar (1 for scalar, N for indexed)
  template <typename RV>
    requires requires { typename RV::id_type; }
  auto paramSize(const RV& /*rv*/) const -> std::size_t {
    constexpr std::size_t idx = findSymbolIndex<RV>();
    static_assert(idx < std::tuple_size_v<SymbolsTuple>, "Symbol not found in posterior");
    return std::get<idx>(specs_).size();
  }

 private:
  std::vector<double> grad_;
  SymbolsTuple symbols_;
  SpecsTuple specs_;
  std::array<std::size_t, std::tuple_size_v<SymbolsTuple>> offsets_;

  // Find the index of RV's symbol in our symbols tuple
  template <typename RV>
  static constexpr std::size_t findSymbolIndex() {
    return plate_detail::SymbolIndexIn<typename RV::symbol_type, SymbolsTuple>::value;
  }

  void computeOffsets() {
    computeOffsetsImpl(std::make_index_sequence<std::tuple_size_v<SymbolsTuple>>{});
  }

  template <std::size_t... Is>
  void computeOffsetsImpl(std::index_sequence<Is...>) {
    std::size_t running = 0;
    ((offsets_[Is] = running, running += std::get<Is>(specs_).size()), ...);
  }
};

// Factory function
template <typename SymbolsTuple, typename SpecsTuple>
auto makeGradientResult(std::vector<double> grad, SymbolsTuple symbols, SpecsTuple specs) {
  return GradientResult<SymbolsTuple, SpecsTuple>{std::move(grad), symbols, specs};
}

// ============================================================================
// DimSizes - Runtime dimension sizes for plates
// ============================================================================

template <typename... DimTags>
struct DimSizes {
  std::array<std::size_t, sizeof...(DimTags)> sizes;

  template <typename DimTag>
  constexpr auto get() const -> std::size_t {
    constexpr std::size_t idx = plate_detail::TypeIndexIn<DimTag, DimTags...>::value;
    return sizes[idx];
  }
};

// Single dimension convenience constructor
template <typename DimTag>
constexpr auto makeDimSizes(std::size_t size) -> DimSizes<DimTag> {
  return DimSizes<DimTag>{{size}};
}

// ============================================================================
// Parameter specification - describes a parameter's shape and transform
// ============================================================================

// Scalar parameter
template <typename Param, typename Transform>
struct ScalarParamSpec {
  [[no_unique_address]] Transform transform;

  static constexpr bool is_indexed = false;

  constexpr auto size() const -> std::size_t { return 1; }

  auto transformValue(double z) const -> double { return transform.transform(z); }

  auto logJacobian(double z) const -> double { return transform.logJacobian(z); }

  // Chain rule for gradient: convert d(logp)/dx to d(logp)/dz
  // Returns grad_z = grad_x * dx/dz + d(logJacobian)/dz
  auto chainRuleGrad(double grad_x, double z) const -> double {
    return transform.chainRuleGrad(grad_x, z);
  }
};

// Indexed parameter (from a plate)
template <typename Param, typename Transform, typename DimsList>
struct IndexedParamSpec {
  [[no_unique_address]] Transform transform;
  std::size_t size_ = 0;

  static constexpr bool is_indexed = true;
  using dims_list = DimsList;

  constexpr auto size() const -> std::size_t { return size_; }

  auto transformValue(double z) const -> double { return transform.transform(z); }

  auto logJacobian(double z) const -> double { return transform.logJacobian(z); }

  auto totalLogJacobian(std::span<const double> z_values) const -> double {
    double sum = 0.0;
    for (double z : z_values) {
      sum += transform.logJacobian(z);
    }
    return sum;
  }

  // Chain rule for gradient: convert d(logp)/dx to d(logp)/dz
  // grad_x is the gradient w.r.t. constrained value x
  // Returns grad_z = grad_x * dx/dz + d(logJacobian)/dz
  auto chainRuleGrad(double grad_x, double z) const -> double {
    return transform.chainRuleGrad(grad_x, z);
  }
};

// ============================================================================
// Type traits for param specs
// ============================================================================

template <typename T>
struct IsScalarParamSpec : std::false_type {};

template <typename P, typename T>
struct IsScalarParamSpec<ScalarParamSpec<P, T>> : std::true_type {};

template <typename T>
constexpr bool is_scalar_param_spec_v = IsScalarParamSpec<T>::value;

template <typename T>
struct IsIndexedParamSpec : std::false_type {};

template <typename P, typename T, typename D>
struct IsIndexedParamSpec<IndexedParamSpec<P, T, D>> : std::true_type {};

template <typename T>
constexpr bool is_indexed_param_spec_v = IsIndexedParamSpec<T>::value;

// Extract the Param type (RandomVar) from a spec
template <typename Spec>
struct ExtractParamFromSpec;

template <typename Param, typename Transform>
struct ExtractParamFromSpec<ScalarParamSpec<Param, Transform>> {
  using type = Param;
};

template <typename Param, typename Transform, typename Dims>
struct ExtractParamFromSpec<IndexedParamSpec<Param, Transform, Dims>> {
  using type = Param;
};

template <typename Spec>
using extract_param_t = typename ExtractParamFromSpec<std::decay_t<Spec>>::type;

// Helper to extract Params... from ParamSpecsTuple
template <typename SpecsTuple, typename = std::make_index_sequence<std::tuple_size_v<SpecsTuple>>>
struct ExtractParamsFromSpecs;

template <typename SpecsTuple, std::size_t... Is>
struct ExtractParamsFromSpecs<SpecsTuple, std::index_sequence<Is...>> {
  template <template <typename...> class Container>
  using apply = Container<extract_param_t<std::tuple_element_t<Is, SpecsTuple>>...>;
};

// ============================================================================
// makeParamSpec - Create appropriate spec from param and transform
// ============================================================================

namespace detail {

// For scalar RandomVar
template <typename RV, typename Transform>
  requires(IsRandomVar<RV> && !IsIndexedRandomVar<RV>)
constexpr auto makeParamSpec(Transform t) {
  return ScalarParamSpec<RV, Transform>{t};
}

// For IndexedRandomVar (plate)
template <typename RV, typename Transform>
  requires IsIndexedRandomVar<RV>
constexpr auto makeParamSpec(Transform t) {
  using DimsList = typename RV::dims_list;
  return IndexedParamSpec<RV, Transform, DimsList>{t, 0};
}

// Extract the underlying transform from wrapped types
template <typename T>
constexpr auto extractTransformType(T t) {
  if constexpr (is_transform_v<T>) {
    return t;
  } else {
    return autoTransform(t);
  }
}

}  // namespace detail

// ============================================================================
// PlateTransformedPosterior - Posterior with mixed scalar/indexed params
// ============================================================================

template <Symbolic LogProbExpr, typename ObsBindings, typename DataBindings,
          typename ParamSpecsTuple, typename ParamSymbolsTuple, typename GradExprsTuple>
class PlateTransformedPosterior {
 public:
  static constexpr std::size_t NumParamSlots = std::tuple_size_v<ParamSpecsTuple>;

  constexpr PlateTransformedPosterior(LogProbExpr lp, ObsBindings obs,
                                       DataBindings data,
                                       ParamSpecsTuple specs,
                                       ParamSymbolsTuple symbols,
                                       GradExprsTuple grads)
      : log_prob_expr_{lp},
        observations_{obs},
        data_bindings_{data},
        specs_{specs},
        symbols_{symbols},
        grad_exprs_{grads},
        transform_pack_{buildNonObservedSymbols(std::make_index_sequence<NumParamSlots>{}),
                         buildNonObservedSpecs(std::make_index_sequence<NumParamSlots>{})} {}

  // Total state dimension (sum of all param sizes)
  auto stateDim() const -> std::size_t {
    return transform_pack_.stateDim();
  }

  // Debug: evaluate log_prob_expr_ with externally provided bindings
  // This bypasses internal binding construction to test if the expression itself works
  template <typename... Binders>
  auto debugEvalWithBindings(BinderPack<Binders...> bindings) const -> double {
    return evaluateIndexed(log_prob_expr_, bindings);
  }

  // Debug: return the log_prob expression for external inspection
  auto debugLogProbExpr() const { return log_prob_expr_; }

  // Debug: return symbols tuple for type checking
  auto debugSymbols() const { return symbols_; }

  // Debug: build bindings and return them for inspection
  auto debugBuildBindings(std::span<const double> z) const {
    return buildBindingsUnconstrained(z, std::make_index_sequence<NumParamSlots>{});
  }

  // Debug: build and merge all bindings
  auto debugMergedBindings(std::span<const double> z) const {
    auto bindings = buildBindingsUnconstrained(z, std::make_index_sequence<NumParamSlots>{});
    return mergeAllBindings(bindings, observations_, data_bindings_);
  }

  // Debug: evaluate with merged bindings and return value
  auto debugLogProbWithMergedBindings(std::span<const double> z) const -> double {
    auto merged = debugMergedBindings(z);
    return evaluateIndexed(log_prob_expr_, merged);
  }

  // Evaluate log-probability at unconstrained state
  auto logProb(std::span<const double> z) const -> double {
    return logProbImpl(z, std::make_index_sequence<NumParamSlots>{});
  }

  // Evaluate gradient at unconstrained state
  auto gradient(std::span<const double> z) const -> std::vector<double> {
    return gradientImpl(z, std::make_index_sequence<NumParamSlots>{});
  }

  // Evaluate gradient and return a result queryable by symbols
  // Usage:
  //   auto grad = posterior.gradientResult(z);
  //   double d_alpha = grad[alpha];  // Query by RandomVar
  auto gradientResult(std::span<const double> z) const {
    auto grad = gradientImpl(z, std::make_index_sequence<NumParamSlots>{});
    return makeGradientResult(std::move(grad), symbols_, specs_);
  }

  // =========================================================================
  // Binding-based evaluation API
  // =========================================================================
  //
  // Evaluate log-probability at parameter values specified via bindings:
  //   double lp = posterior.logProbAt(BinderPack{alpha = 0.5, beta = 1.0});
  //
  template <typename... Binders>
  auto logProbAt(BinderPack<Binders...> bindings) const -> double {
    static constexpr std::size_t NumSampledParams = std::tuple_size_v<NonObservedSpecsTuple>;
    std::array<double, NumSampledParams> z_arr{};
    initStateFromBindingsToArray<Binders...>(bindings, z_arr);
    return logProb(std::span<const double>(z_arr));
  }

  // Evaluate gradient at parameter values specified via bindings:
  //   auto grad = posterior.gradientAt(BinderPack{alpha = 0.5, beta = 1.0});
  //   double d_alpha = grad[alpha];
  //
  template <typename... Binders>
  auto gradientAt(BinderPack<Binders...> bindings) const {
    static constexpr std::size_t NumSampledParams = std::tuple_size_v<NonObservedSpecsTuple>;
    std::array<double, NumSampledParams> z_arr{};
    initStateFromBindingsToArray<Binders...>(bindings, z_arr);
    return gradientResult(std::span<const double>(z_arr));
  }

  // Transform from unconstrained to constrained space
  auto transform(std::span<const double> z) const -> std::vector<double> {
    return transform_pack_.transform(z);
  }

  // Inverse: constrained to unconstrained
  auto inverse(std::span<const double> x) const -> std::vector<double> {
    return transform_pack_.inverse(x);
  }

  // Set dimension size for a plate
  template <typename DimTag>
  auto withDimension(std::size_t size) const {
    auto new_specs = setDimensionImpl<DimTag>(size, std::make_index_sequence<NumParamSlots>{});
    return PlateTransformedPosterior<LogProbExpr, ObsBindings, DataBindings,
                                      decltype(new_specs), ParamSymbolsTuple, GradExprsTuple>{
        log_prob_expr_, observations_, data_bindings_, new_specs, symbols_, grad_exprs_};
  }

  // Set data bindings (for non-parameter indexed values)
  template <typename NewDataBindings>
  auto withData(NewDataBindings new_data) const {
    return PlateTransformedPosterior<LogProbExpr, ObsBindings, NewDataBindings,
                                      ParamSpecsTuple, ParamSymbolsTuple, GradExprsTuple>{
        log_prob_expr_, observations_, new_data, specs_, symbols_, grad_exprs_};
  }

  // =========================================================================
  // sample() - Run HMC and collect samples with binding-centric API
  // =========================================================================
  //
  // Usage:
  //   auto samples = posterior.sample(
  //       HmcConfig{.epsilon = 0.01, .steps = 20, .warmup = 500, .draws = 1000},
  //       BinderPack{alpha = 0.0, beta = 0.0, sigma = 1.0},  // init
  //       rng);
  //
  //   double mean_alpha = mean(samples[alpha]);
  //

 private:
  // Forward declare helper to check if Sym is in ObsBindings
  template <typename Sym, typename OB>
  struct IsSymbolInObsBindingsHelperFwd {
    static constexpr bool value = false;
  };

  template <typename Sym, typename... Binders>
  struct IsSymbolInObsBindingsHelperFwd<Sym, BinderPack<Binders...>> {
    static constexpr bool value = ((std::is_same_v<Sym, typename Binders::symbol_type>) || ...);
  };

  // Type-level filter to get non-observed specs
  template <std::size_t I>
  struct MaybeSpecType {
    using SymType = std::decay_t<std::tuple_element_t<I, ParamSymbolsTuple>>;
    using SpecType = std::tuple_element_t<I, ParamSpecsTuple>;

    // Use std::tuple<> for observed, std::tuple<SpecType> for non-observed
    using type = std::conditional_t<
        IsSymbolInObsBindingsHelperFwd<SymType, ObsBindings>::value,
        std::tuple<>,
        std::tuple<SpecType>>;
  };

  // Primary template (takes index_sequence)
  template <typename IndexSeq>
  struct ConcatNonObservedSpecs;

  template <std::size_t... Is>
  struct ConcatNonObservedSpecs<std::index_sequence<Is...>> {
    using type = decltype(std::tuple_cat(std::declval<typename MaybeSpecType<Is>::type>()...));
  };

  using NonObservedSpecsTuple =
      typename ConcatNonObservedSpecs<std::make_index_sequence<NumParamSlots>>::type;

  // Type-level filter to get non-observed symbols (matching specs filter)
  template <std::size_t I>
  struct MaybeSymbolType {
    using SymType = std::decay_t<std::tuple_element_t<I, ParamSymbolsTuple>>;

    using type = std::conditional_t<
        IsSymbolInObsBindingsHelperFwd<SymType, ObsBindings>::value,
        std::tuple<>,
        std::tuple<SymType>>;
  };

  template <typename IndexSeq>
  struct ConcatNonObservedSymbols;

  template <std::size_t... Is>
  struct ConcatNonObservedSymbols<std::index_sequence<Is...>> {
    using type = decltype(std::tuple_cat(std::declval<typename MaybeSymbolType<Is>::type>()...));
  };

  using NonObservedSymbolsTuple =
      typename ConcatNonObservedSymbols<std::make_index_sequence<NumParamSlots>>::type;

  // TransformPack for non-observed params — delegates transform/inverse/logJacobian
  using TransformPackType = TransformPack<NonObservedSymbolsTuple, NonObservedSpecsTuple>;

  // Check if all non-observed specs are scalar (for compile-time validation)
  static constexpr auto allParamsAreScalar() -> bool {
    return allParamsAreScalarImpl(std::make_index_sequence<std::tuple_size_v<NonObservedSpecsTuple>>{});
  }

  template <std::size_t... Is>
  static constexpr auto allParamsAreScalarImpl(std::index_sequence<Is...>) -> bool {
    return (is_scalar_param_spec_v<std::tuple_element_t<Is, NonObservedSpecsTuple>> && ...);
  }

 public:
  // Samples type only includes non-observed parameters
  using SamplesType =
      typename ExtractParamsFromSpecs<NonObservedSpecsTuple>::template apply<Samples>;

  // Dynamic samples type for models with indexed latent params
  using DynamicSamplesType = DynamicSamples<NonObservedSymbolsTuple, NonObservedSpecsTuple>;

  // Main sample() method - dispatches to scalar or dynamic implementation
  template <typename... Binders, std::uniform_random_bit_generator RNG>
  auto sample(HmcConfig config, BinderPack<Binders...> init, RNG& rng) const {
    if constexpr (allParamsAreScalar()) {
      return sampleScalar(config, init, rng);
    } else {
      return sampleDynamic(config, init, rng);
    }
  }

 private:
  // Scalar-only sampling (compile-time sized state)
  template <typename... Binders, std::uniform_random_bit_generator RNG>
  auto sampleScalar(HmcConfig config, BinderPack<Binders...> init, RNG& rng) const -> SamplesType {
    static constexpr std::size_t NumSampledParams = std::tuple_size_v<NonObservedSpecsTuple>;
    static_assert(NumSampledParams <= 64, "Too many parameters for sample()");

    // Convert init bindings to array
    std::array<double, NumSampledParams> z_arr{};
    initStateFromBindingsToArray<Binders...>(init, z_arr);

    // Create HMC kernel operating on non-observed params only
    auto hmc = bayes::makeHMC<double, NumSampledParams>(
        [this](const std::array<double, NumSampledParams>& state) {
          return this->logProb(std::span<const double>(state));
        },
        [this](const std::array<double, NumSampledParams>& state) {
          auto g = this->gradient(std::span<const double>(state));
          std::array<double, NumSampledParams> result{};
          for (std::size_t i = 0; i < NumSampledParams; ++i) {
            result[i] = g[i];
          }
          return result;
        },
        config.epsilon, config.steps);

    double lp = hmc.logProb(z_arr);

    // Warmup
    for (std::size_t i = 0; i < config.warmup; ++i) {
      auto [next_z, next_lp, accepted] = hmc.step(z_arr, lp, rng);
      z_arr = next_z;
      lp = next_lp;
    }

    // Collect samples (transformed to constrained space)
    SamplesType samples;
    samples.reserve(config.draws);

    for (std::size_t i = 0; i < config.draws; ++i) {
      auto [next_z, next_lp, accepted] = hmc.step(z_arr, lp, rng);
      z_arr = next_z;
      lp = next_lp;

      // Transform to constrained space
      auto constrained = transform(std::span<const double>(z_arr));
      std::array<double, NumSampledParams> x_arr{};
      for (std::size_t j = 0; j < NumSampledParams; ++j) {
        x_arr[j] = constrained[j];
      }
      samples.push_back(x_arr);
    }

    return samples;
  }

  // Dynamic sampling (runtime-sized state for indexed params)
  template <typename... Binders, std::uniform_random_bit_generator RNG>
  auto sampleDynamic(HmcConfig config, BinderPack<Binders...> init, RNG& rng) const
      -> DynamicSamplesType {
    // Get runtime state dimension
    std::size_t dim = stateDim();

    // Convert init bindings to vector
    std::vector<double> z = initStateFromBindingsDynamic(init);
    assert(z.size() == dim);

    // Create dynamic HMC kernel
    auto hmc = bayes::makeHMCDynamic(
        [this](std::span<const double> state) { return this->logProb(state); },
        [this](std::span<const double> state) { return this->gradient(state); },
        config.epsilon, config.steps, dim);

    double lp = hmc.logProb(z);

    // Warmup
    for (std::size_t i = 0; i < config.warmup; ++i) {
      auto [next_z, next_lp, accepted] = hmc.step(z, lp, rng);
      z = std::move(next_z);
      lp = next_lp;
    }

    // Collect samples (transformed to constrained space)
    // Build filtered symbols and specs tuples (excluding observed params)
    auto non_observed_symbols = buildNonObservedSymbols(std::make_index_sequence<NumParamSlots>{});
    auto non_observed_specs = buildNonObservedSpecs(std::make_index_sequence<NumParamSlots>{});
    DynamicSamplesType samples(non_observed_symbols, non_observed_specs, dim);
    samples.reserve(config.draws);

    for (std::size_t i = 0; i < config.draws; ++i) {
      auto [next_z, next_lp, accepted] = hmc.step(z, lp, rng);
      z = std::move(next_z);
      lp = next_lp;

      // Transform to constrained space and add to samples
      auto constrained = transform(std::span<const double>(z));
      samples.push_back(std::span<const double>(constrained));
    }

    return samples;
  }

  // Initialize dynamic state from bindings (handles vector<double> for indexed params)
  template <typename... Binders>
  auto initStateFromBindingsDynamic(BinderPack<Binders...> bindings) const -> std::vector<double> {
    std::size_t dim = stateDim();
    std::vector<double> z(dim);
    std::size_t offset = 0;
    initStateFromBindingsDynamicImpl(bindings, z, offset,
                                      std::make_index_sequence<NumParamSlots>{});
    return z;
  }

  template <typename... Binders, std::size_t... Is>
  void initStateFromBindingsDynamicImpl(BinderPack<Binders...> bindings,
                                         std::vector<double>& z,
                                         std::size_t& offset,
                                         std::index_sequence<Is...>) const {
    (extractBindingValueDynamic<Is>(bindings, z, offset), ...);
  }

  template <std::size_t I, typename... Binders>
  void extractBindingValueDynamic(BinderPack<Binders...> bindings,
                                   std::vector<double>& z,
                                   std::size_t& offset) const {
    using SymType = std::decay_t<std::tuple_element_t<I, ParamSymbolsTuple>>;
    if constexpr (isSymbolObserved<SymType>()) {
      return;  // Skip observed params
    } else {
      const auto& spec = std::get<I>(specs_);
      std::size_t n = spec.size();

      if constexpr (is_scalar_param_spec_v<std::decay_t<decltype(spec)>>) {
        // Scalar param
        z[offset] = static_cast<double>(bindings[SymType{}]);
        offset += 1;
      } else {
        // Indexed param - expect IndexedBinding with vector<double> values
        // Apply inverse transform so users can pass constrained-space init values
        auto binding = bindings[SymType{}];
        for (std::size_t i = 0; i < n; ++i) {
          z[offset + i] = spec.transform.inverse(binding.at(i));
        }
        offset += n;
      }
    }
  }

  // Build filtered symbols tuple (excluding observed params)
  template <std::size_t... Is>
  auto buildNonObservedSymbols(std::index_sequence<Is...>) const -> NonObservedSymbolsTuple {
    return std::tuple_cat(maybeIncludeSymbol<Is>()...);
  }

  template <std::size_t I>
  auto maybeIncludeSymbol() const {
    using SymType = std::decay_t<std::tuple_element_t<I, ParamSymbolsTuple>>;
    if constexpr (isSymbolObserved<SymType>()) {
      return std::tuple<>();
    } else {
      return std::make_tuple(std::get<I>(symbols_));
    }
  }

  // Build filtered specs tuple (excluding observed params)
  template <std::size_t... Is>
  auto buildNonObservedSpecs(std::index_sequence<Is...>) const -> NonObservedSpecsTuple {
    return std::tuple_cat(maybeIncludeSpec<Is>()...);
  }

  template <std::size_t I>
  auto maybeIncludeSpec() const {
    using SymType = std::decay_t<std::tuple_element_t<I, ParamSymbolsTuple>>;
    if constexpr (isSymbolObserved<SymType>()) {
      return std::tuple<>();
    } else {
      return std::make_tuple(std::get<I>(specs_));
    }
  }

 private:
  // Helper to initialize state array from bindings
  template <typename... Binders, std::size_t N>
  void initStateFromBindingsToArray(BinderPack<Binders...> bindings,
                                    std::array<double, N>& z) const {
    std::size_t offset = 0;
    initStateFromBindingsImpl(bindings, z, offset, std::make_index_sequence<NumParamSlots>{});
  }

  template <typename... Binders, std::size_t N, std::size_t... Is>
  void initStateFromBindingsImpl(BinderPack<Binders...> bindings,
                                 std::array<double, N>& z,
                                 std::size_t& offset,
                                 std::index_sequence<Is...>) const {
    // For each parameter slot, extract the value from bindings (skipping observed)
    (extractBindingValue<Is>(bindings, z, offset), ...);
  }

  template <std::size_t I, typename... Binders, std::size_t N>
  void extractBindingValue(BinderPack<Binders...> bindings,
                           std::array<double, N>& z,
                           std::size_t& offset) const {
    using SymType = std::decay_t<std::tuple_element_t<I, ParamSymbolsTuple>>;
    if constexpr (isSymbolObserved<SymType>()) {
      return;
    } else {
      // User binds constrained value (sigma = 2.0 → Symbol<Id> = 2.0).
      // Convert to unconstrained z via inverse transform.
      double constrained = static_cast<double>(bindings[SymType{}]);
      const auto& spec = std::get<I>(specs_);
      z[offset] = spec.transform.inverse(constrained);
      offset += 1;  // Scalar params only for now
    }
  }


  LogProbExpr log_prob_expr_;
  ObsBindings observations_;
  DataBindings data_bindings_;
  ParamSpecsTuple specs_;
  ParamSymbolsTuple symbols_;
  GradExprsTuple grad_exprs_;
  TransformPackType transform_pack_;

  // -------------------------------------------------------------------------
  // Implementation helpers
  // -------------------------------------------------------------------------

  // Helper to get size of param I, returning 0 if observed
  template <std::size_t I>
  auto paramSizeIfNotObserved() const -> std::size_t {
    using SymType = std::decay_t<std::tuple_element_t<I, ParamSymbolsTuple>>;
    if constexpr (isSymbolObserved<SymType>()) {
      return 0;
    } else {
      return std::get<I>(specs_).size();
    }
  }

  template <std::size_t... Is>
  auto logProbImpl(std::span<const double> z, std::index_sequence<Is...>) const -> double {
    // Evaluate model log-probability: log p(x) where x = transform(z)
    // All params: pre-transform z → x, bind constrained x to Free symbols
    auto bindings = buildBindingsUnconstrained(z, std::make_index_sequence<NumParamSlots>{});
    auto merged = mergeAllBindings(bindings, observations_, data_bindings_);
    double lp = evaluateIndexed(log_prob_expr_, merged);

    // Jacobian correction: log p(z) = log p(x) + log |dx/dz|
    // Required for correct density in unconstrained space (HMC target)
    // Use subspan matching non-observed param count (z may be oversized if caller
    // includes slots for observed params that buildBindingsUnconstrained skips)
    auto state_dim = transform_pack_.stateDim();
    double log_jacobian = transform_pack_.logJacobian(z.subspan(0, state_dim));

    return lp + log_jacobian;
  }

  // Check if a symbol type is in ObsBindings (to avoid duplicate bindings)
  template <typename Sym>
  static constexpr bool isSymbolObserved() {
    return IsSymbolInObsBindingsHelper<Sym, ObsBindings>::value;
  }

  // Helper to check if Sym is in a BinderPack
  template <typename Sym, typename OB>
  struct IsSymbolInObsBindingsHelper {
    static constexpr bool value = false;
  };

  template <typename Sym, typename... Binders>
  struct IsSymbolInObsBindingsHelper<Sym, BinderPack<Binders...>> {
    static constexpr bool value = ((std::is_same_v<Sym, typename Binders::symbol_type>) || ...);
  };

  // Build bindings with constrained values (used in gradient chain rule)
  template <std::size_t... Is>
  auto buildBindings(const std::vector<double>& x, std::index_sequence<Is...>) const {
    // Pre-compute offsets to avoid undefined evaluation order in tuple_cat
    auto offsets = computeParamOffsets(std::make_index_sequence<NumParamSlots>{});
    auto binding_tuple = std::tuple_cat(maybeBindParamAtOffset<Is>(x, offsets[Is])...);
    return tupleToBinderPackStatic(binding_tuple);
  }

  // Bind constrained value at a specific offset (doesn't modify offset)
  template <std::size_t I>
  auto maybeBindParamAtOffset(const std::vector<double>& x, std::size_t offset) const {
    using SymType = std::decay_t<decltype(std::get<I>(symbols_))>;

    if constexpr (isSymbolObserved<SymType>()) {
      return std::tuple<>();
    } else {
      const auto& spec = std::get<I>(specs_);
      const auto& sym = std::get<I>(symbols_);

      if constexpr (is_scalar_param_spec_v<std::decay_t<decltype(spec)>>) {
        double val = x[offset];
        return std::make_tuple(sym = val);
      } else {
        std::size_t n = spec.size();
        indexed_values_[I].assign(x.begin() + static_cast<std::ptrdiff_t>(offset),
                                   x.begin() + static_cast<std::ptrdiff_t>(offset + n));
        using SymType2 = std::decay_t<decltype(sym)>;
        return std::make_tuple(IndexedBinding<SymType2, 1>{std::span<const double>(indexed_values_[I])});
      }
    }
  }

  // Build bindings with UNCONSTRAINED values (used in log-prob evaluation)
  // The expression contains constrainedExpr() like exp(z), so we bind z, not exp(z)
  template <std::size_t... Is>
  auto buildBindingsUnconstrained(std::span<const double> z, std::index_sequence<Is...>) const {
    // Pre-compute offsets to avoid undefined evaluation order in tuple_cat
    // Each offset[I] is the sum of sizes of non-observed params with index < I
    auto offsets = computeParamOffsets(std::make_index_sequence<NumParamSlots>{});
    auto binding_tuple = std::tuple_cat(maybeBindParamUnconstrainedAtOffset<Is>(z, offsets[Is])...);
    return tupleToBinderPackStatic(binding_tuple);
  }

  // Compute the starting offset for each parameter slot
  template <std::size_t... Is>
  auto computeParamOffsets(std::index_sequence<Is...>) const -> std::array<std::size_t, NumParamSlots> {
    std::array<std::size_t, NumParamSlots> offsets{};
    std::size_t running_offset = 0;
    // Use comma fold expression to ensure left-to-right evaluation
    ((offsets[Is] = running_offset, running_offset += paramSizeIfNotObserved<Is>()), ...);
    return offsets;
  }

  // Bind constrained value at a specific offset.
  // Both scalar and indexed params: pre-transform z → x, bind x to the Free symbol.
  // Expression trees use plain Free atoms — no eval-time constraint transforms.
  template <std::size_t I>
  auto maybeBindParamUnconstrainedAtOffset(std::span<const double> z, std::size_t offset) const {
    using SymType = std::decay_t<decltype(std::get<I>(symbols_))>;

    if constexpr (isSymbolObserved<SymType>()) {
      return std::tuple<>();
    } else {
      const auto& spec = std::get<I>(specs_);
      const auto& sym = std::get<I>(symbols_);

      if constexpr (is_scalar_param_spec_v<std::decay_t<decltype(spec)>>) {
        // Scalar: pre-transform z → x, bind constrained value
        double x = spec.transformValue(z[offset]);
        return std::make_tuple(sym = x);
      } else {
        // Indexed: pre-transform z → x, bind constrained values
        std::size_t n = spec.size();
        constrained_values_[I].resize(n);
        for (std::size_t i = 0; i < n; ++i) {
          constrained_values_[I][i] = spec.transformValue(z[offset + i]);
        }
        using SymType2 = std::decay_t<decltype(sym)>;
        return std::make_tuple(IndexedBinding<SymType2, 1>{std::span<const double>(constrained_values_[I])});
      }
    }
  }

  template <typename... Ts>
  static auto tupleToBinderPackStatic(const std::tuple<Ts...>& t) {
    return std::apply([](const auto&... bs) { return BinderPack{bs...}; }, t);
  }

  static auto tupleToBinderPackStatic(const std::tuple<>&) { return BinderPack<>{}; }

  template <std::size_t... Is>
  auto gradientImpl(std::span<const double> z, std::index_sequence<Is...>) const
      -> std::vector<double> {
    // Build bindings: pre-transform z → x, bind constrained x to Free symbols
    auto bindings = buildBindingsUnconstrained(z, std::make_index_sequence<NumParamSlots>{});
    auto merged = mergeAllBindings(bindings, observations_, data_bindings_);

    // Compute gradients for each non-observed parameter slot
    std::vector<double> grad(z.size());
    std::size_t offset = 0;

    // Use analytic gradients if gradient expressions are available,
    // otherwise fall back to finite differences
    if constexpr (std::tuple_size_v<GradExprsTuple> == NumParamSlots) {
      (maybeComputeAnalyticGrad<Is>(z, merged, grad, offset), ...);
    } else {
      // Fallback: finite differences
      computeFiniteDiffGradient(z, grad);
    }

    return grad;
  }

  // Analytic gradient for a single parameter slot (skipping observed).
  // All params bind constrained values x = transform(z), so grad_expr
  // evaluates to d(logp)/dx. Chain rule converts to unconstrained space:
  //   grad_z = grad_x * dx/dz + d(logJacobian)/dz
  template <std::size_t I, typename Bindings>
  void maybeComputeAnalyticGrad(std::span<const double> z, const Bindings& bindings,
                                std::vector<double>& grad, std::size_t& offset) const {
    using SymType = std::decay_t<std::tuple_element_t<I, ParamSymbolsTuple>>;
    if constexpr (isSymbolObserved<SymType>()) {
      return;
    } else {
      const auto& spec = std::get<I>(specs_);
      std::size_t n = spec.size();
      const auto& grad_expr = std::get<I>(grad_exprs_);

      if constexpr (is_scalar_param_spec_v<std::decay_t<decltype(spec)>>) {
        // Scalar: grad_expr gives d(logp)/dx, apply chain rule for z → x
        double grad_x = evaluateIndexed(grad_expr, bindings);
        double grad_z = spec.chainRuleGrad(grad_x, z[offset]);
        grad[offset] = grad_z;
        offset += 1;
      } else {
        // Indexed: same logic per-element, diff distributes through SumOver
        for (std::size_t i = 0; i < n; ++i) {
          double z_i = z[offset + i];
          double grad_x = evaluateGradAtIndex<I>(grad_expr, bindings, i);
          double grad_z = spec.chainRuleGrad(grad_x, z_i);
          grad[offset + i] = grad_z;
        }
        offset += n;
      }
    }
  }

  // Evaluate gradient for indexed parameter at specific index
  template <std::size_t I, typename GradExpr, typename Bindings>
  double evaluateGradAtIndex(const GradExpr& grad_expr, const Bindings& bindings,
                             std::size_t idx) const {
    // For indexed parameters, the gradient expression is SumOver<DimTag, inner>
    // We need d(logp)/d(b[idx]), which is just the inner expr evaluated at idx
    // (not summed over all indices)
    return evaluateAtIndex(grad_expr, bindings, idx);
  }

  // Finite differences fallback
  void computeFiniteDiffGradient(std::span<const double> z, std::vector<double>& grad) const {
    constexpr double eps = 1e-7;
    std::vector<double> z_plus(z.begin(), z.end());
    std::vector<double> z_minus(z.begin(), z.end());

    for (std::size_t i = 0; i < z.size(); ++i) {
      z_plus[i] = z[i] + eps;
      z_minus[i] = z[i] - eps;

      double lp_plus = logProb(z_plus);
      double lp_minus = logProb(z_minus);

      grad[i] = (lp_plus - lp_minus) / (2.0 * eps);

      z_plus[i] = z[i];
      z_minus[i] = z[i];
    }
  }

  template <typename DimTag, std::size_t... Is>
  auto setDimensionImpl(std::size_t size, std::index_sequence<Is...>) const {
    return std::make_tuple(setDimForSpec<DimTag, Is>(size)...);
  }

  template <typename DimTag, std::size_t I>
  auto setDimForSpec(std::size_t size) const {
    auto spec = std::get<I>(specs_);
    if constexpr (is_indexed_param_spec_v<std::decay_t<decltype(spec)>>) {
      using SpecDims = typename std::decay_t<decltype(spec)>::dims_list;
      if constexpr (Contains_v<DimTag, SpecDims>) {
        spec.size_ = size;
      }
    }
    return spec;
  }

  // Merge all binding sources: params, observations, and data
  template <typename... PB, typename... Obs, typename... Data>
  static auto mergeAllBindings(BinderPack<PB...> pb,
                                [[maybe_unused]] BinderPack<Obs...> obs,
                                [[maybe_unused]] BinderPack<Data...> data) {
    if constexpr (sizeof...(Obs) == 0 && sizeof...(Data) == 0) {
      return pb;
    } else if constexpr (sizeof...(Obs) == 0) {
      return BinderPack{static_cast<PB&>(pb)..., static_cast<Data&>(data)...};
    } else if constexpr (sizeof...(Data) == 0) {
      return BinderPack{static_cast<PB&>(pb)..., static_cast<Obs&>(obs)...};
    } else {
      return BinderPack{static_cast<PB&>(pb)..., static_cast<Obs&>(obs)...,
                        static_cast<Data&>(data)...};
    }
  }

  // Mutable storage for indexed values (needed for lifetime management)
  mutable std::array<std::vector<double>, NumParamSlots> indexed_values_;
  mutable std::array<std::vector<double>, NumParamSlots> unconstrained_values_;
  mutable std::array<std::vector<double>, NumParamSlots> constrained_values_;
};

// ============================================================================
// PlateTransformedPosteriorBuilder
// ============================================================================

template <Symbolic LogProbExpr, typename DataBindings, typename ParamSpecsTuple,
          typename ParamSymbolsTuple, typename GradExprsTuple>
class PlateTransformedPosteriorBuilder {
 public:
  constexpr PlateTransformedPosteriorBuilder(LogProbExpr lp, DataBindings data,
                                              ParamSpecsTuple specs,
                                              ParamSymbolsTuple symbols,
                                              GradExprsTuple grads)
      : log_prob_expr_{lp}, data_bindings_{data}, specs_{specs}, symbols_{symbols},
        grad_exprs_{grads} {}

  // Set dimension size for plates
  template <typename DimTag>
  auto withDimension(std::size_t size) const {
    auto new_specs = setDimension<DimTag>(size);
    return PlateTransformedPosteriorBuilder<LogProbExpr, DataBindings, decltype(new_specs),
                                            ParamSymbolsTuple, GradExprsTuple>{
        log_prob_expr_, data_bindings_, new_specs, symbols_, grad_exprs_};
  }

  // Bind data (non-parameter indexed values like sample sizes)
  template <typename... Binders>
  auto bindData(Binders... binders) const {
    auto new_data = BinderPack{binders...};
    return PlateTransformedPosteriorBuilder<LogProbExpr, decltype(new_data),
                                            ParamSpecsTuple, ParamSymbolsTuple,
                                            GradExprsTuple>{
        log_prob_expr_, new_data, specs_, symbols_, grad_exprs_};
  }

  // Overload: accept a BinderPack directly (for chaining from model API)
  template <typename... Binders>
  auto bindData(BinderPack<Binders...> data_pack) const {
    return PlateTransformedPosteriorBuilder<LogProbExpr, BinderPack<Binders...>,
                                            ParamSpecsTuple, ParamSymbolsTuple,
                                            GradExprsTuple>{
        log_prob_expr_, data_pack, specs_, symbols_, grad_exprs_};
  }

  // Add observations
  template <typename... Binders>
  auto observe(Binders... binders) const {
    auto obs = BinderPack{binders...};
    return PlateTransformedPosterior<LogProbExpr, BinderPack<Binders...>, DataBindings,
                                      ParamSpecsTuple, ParamSymbolsTuple,
                                      GradExprsTuple>{
        log_prob_expr_, obs, data_bindings_, specs_, symbols_, grad_exprs_};
  }

  // Overload: accept a BinderPack directly (for chaining from model API)
  template <typename... Binders>
  auto observe(BinderPack<Binders...> obs_pack) const {
    return PlateTransformedPosterior<LogProbExpr, BinderPack<Binders...>, DataBindings,
                                      ParamSpecsTuple, ParamSymbolsTuple,
                                      GradExprsTuple>{
        log_prob_expr_, obs_pack, data_bindings_, specs_, symbols_, grad_exprs_};
  }

  // =========================================================================
  // Unified bind() - accepts both data and observation bindings
  // =========================================================================
  // Automatically separates bindings into:
  //   - Observation bindings (for observed random variables in the model)
  //   - Data bindings (for covariates/predictors)
  // Also auto-infers dimension sizes from the bindings.
  //
  // Usage:
  //   auto posterior = infer(alpha, beta, sigma, y)
  //       .bind(x = x_data, y = y_data)
  //       .build();

  template <typename... Binders>
  auto bind(Binders... binders) const {
    auto all_bindings = BinderPack{binders...};

    // Separate into observations (for RVs in symbols_) and data (everything else)
    auto [obs_tuple, data_tuple] = separateBindings(all_bindings);
    auto obs = tupleToBinderPack(obs_tuple);
    auto data = tupleToBinderPack(data_tuple);

    // Infer dimension sizes from bindings
    auto new_specs = inferDimensionsFromBindings(all_bindings);

    return PlateTransformedPosterior<LogProbExpr, decltype(obs), decltype(data),
                                      decltype(new_specs), ParamSymbolsTuple,
                                      GradExprsTuple>{
        log_prob_expr_, obs, data, new_specs, symbols_, grad_exprs_};
  }

  // Build without observations
  auto build() const {
    return PlateTransformedPosterior<LogProbExpr, BinderPack<>, DataBindings,
                                      ParamSpecsTuple, ParamSymbolsTuple, GradExprsTuple>{
        log_prob_expr_, BinderPack<>{}, data_bindings_, specs_, symbols_, grad_exprs_};
  }

 private:
  LogProbExpr log_prob_expr_;
  DataBindings data_bindings_;
  ParamSpecsTuple specs_;
  ParamSymbolsTuple symbols_;
  GradExprsTuple grad_exprs_;

  template <typename DimTag>
  auto setDimension(std::size_t size) const {
    return setDimensionImpl<DimTag>(size, std::make_index_sequence<std::tuple_size_v<ParamSpecsTuple>>{});
  }

  template <typename DimTag, std::size_t... Is>
  auto setDimensionImpl(std::size_t size, std::index_sequence<Is...>) const {
    return std::make_tuple(setDimForSpec<DimTag, Is>(size)...);
  }

  template <typename DimTag, std::size_t I>
  auto setDimForSpec(std::size_t size) const {
    auto spec = std::get<I>(specs_);
    if constexpr (is_indexed_param_spec_v<std::decay_t<decltype(spec)>>) {
      using SpecDims = typename std::decay_t<decltype(spec)>::dims_list;
      if constexpr (Contains_v<DimTag, SpecDims>) {
        spec.size_ = size;
      }
    }
    return spec;
  }

  // -------------------------------------------------------------------------
  // bind() helper: check if binder is for one of our param symbols
  // -------------------------------------------------------------------------
  template <typename Binder>
  static constexpr bool isObservationBinder() {
    using BinderSym = typename Binder::symbol_type;
    return isSymbolInTuple<BinderSym>(std::make_index_sequence<std::tuple_size_v<ParamSymbolsTuple>>{});
  }

  template <typename Sym, std::size_t... Is>
  static constexpr bool isSymbolInTuple(std::index_sequence<Is...>) {
    return (std::is_same_v<Sym, std::tuple_element_t<Is, ParamSymbolsTuple>> || ...);
  }

  // -------------------------------------------------------------------------
  // bind() helper: separate bindings into observations and data
  // -------------------------------------------------------------------------
  template <typename... Binders>
  auto separateBindings(const BinderPack<Binders...>& pack) const {
    auto obs_tuple = filterObservations<Binders...>(pack);
    auto data_tuple = filterData<Binders...>(pack);
    return std::pair{obs_tuple, data_tuple};
  }

  template <typename... Binders>
  auto filterObservations(const BinderPack<Binders...>& pack) const {
    return std::tuple_cat(maybeIncludeObs<Binders>(static_cast<const Binders&>(pack))...);
  }

  template <typename Binder>
  auto maybeIncludeObs(const Binder& b) const {
    if constexpr (isObservationBinder<Binder>()) {
      return std::make_tuple(b);
    } else {
      return std::tuple<>();
    }
  }

  template <typename... Binders>
  auto filterData(const BinderPack<Binders...>& pack) const {
    return std::tuple_cat(maybeIncludeData<Binders>(static_cast<const Binders&>(pack))...);
  }

  template <typename Binder>
  auto maybeIncludeData(const Binder& b) const {
    if constexpr (!isObservationBinder<Binder>()) {
      return std::make_tuple(b);
    } else {
      return std::tuple<>();
    }
  }

  // -------------------------------------------------------------------------
  // bind() helper: convert tuple to BinderPack
  // -------------------------------------------------------------------------
  template <typename... Ts>
  static auto tupleToBinderPack(const std::tuple<Ts...>& t) {
    return std::apply([](const auto&... bs) { return BinderPack{bs...}; }, t);
  }

  static auto tupleToBinderPack(const std::tuple<>&) { return BinderPack<>{}; }

  // -------------------------------------------------------------------------
  // bind() helper: infer dimension sizes from indexed bindings
  // -------------------------------------------------------------------------
  template <typename... Binders>
  auto inferDimensionsFromBindings(const BinderPack<Binders...>& pack) const {
    return inferDimensionsImpl(pack, std::make_index_sequence<std::tuple_size_v<ParamSpecsTuple>>{});
  }

  template <typename Bindings, std::size_t... Is>
  auto inferDimensionsImpl(const Bindings& bindings, std::index_sequence<Is...>) const {
    return std::make_tuple(inferDimForSpec<Is>(bindings)...);
  }

  template <std::size_t I, typename Bindings>
  auto inferDimForSpec(const Bindings& bindings) const {
    auto spec = std::get<I>(specs_);
    if constexpr (is_indexed_param_spec_v<std::decay_t<decltype(spec)>>) {
      // Try to infer size from bindings
      using SpecDims = typename std::decay_t<decltype(spec)>::dims_list;
      auto size = inferSizeFromBindings<Head_t<SpecDims>>(bindings);
      if (size > 0) {
        spec.size_ = size;
      }
    }
    return spec;
  }

  template <typename DimTag, typename Bindings>
  static auto inferSizeFromBindings(const Bindings& bindings) -> std::size_t {
    return getDimensionSize<DimTag>(bindings);
  }
};

// ============================================================================
// makePlateTransformedPosterior - Factory function
// ============================================================================

namespace plate_detail {

// Extract symbol from param (handles both RandomVar and IndexedRandomVar)
// Uses freeSym() for binding compatibility
template <typename Param>
constexpr auto extractPlateSymbol(const Param& p) {
  if constexpr (IsRandomVar<Param> || IsIndexedRandomVar<Param>) {
    return p.freeSym();
  } else if constexpr (is_transform_v<Param>) {
    return extractPlateSymbol(p.param);
  } else {
    return p;
  }
}

// Check if a symbol type is in the observation bindings
template <typename Sym, typename ObsBindings>
struct IsObservedSymbol : std::false_type {};

template <typename Sym, typename... Binders>
struct IsObservedSymbol<Sym, BinderPack<Binders...>> {
  // Check if any binder's symbol_type matches Sym
  static constexpr bool value = (std::is_same_v<typename Binders::symbol_type, Sym> || ...);
};

template <typename Sym, typename ObsBindings>
constexpr bool is_observed_symbol_v = IsObservedSymbol<Sym, ObsBindings>::value;

// Check if an RV is observed
template <typename RV, typename ObsBindings>
constexpr bool is_rv_observed_v = is_observed_symbol_v<typename RV::symbol_type, ObsBindings>;

// Extract the underlying param from transform wrapper
template <typename T>
constexpr auto extractPlateParam(const T& t) {
  if constexpr (is_transform_v<T>) {
    return t.param;
  } else {
    return t;
  }
}

// Get transform for a param (auto-infer or use explicit)
template <typename T>
constexpr auto getPlateTransform(const T& t) {
  if constexpr (is_transform_v<T>) {
    return t;
  } else {
    return autoTransform(t);
  }
}

// Create param spec from param
template <typename Param>
constexpr auto createPlateParamSpec(const Param& p) {
  auto transform = getPlateTransform(p);
  auto underlying = extractPlateParam(p);

  if constexpr (IsIndexedRandomVar<std::decay_t<decltype(underlying)>>) {
    using RV = std::decay_t<decltype(underlying)>;
    using DimsList = typename RV::dims_list;
    return IndexedParamSpec<RV, decltype(transform), DimsList>{transform, 0};
  } else {
    using RV = std::decay_t<decltype(underlying)>;
    return ScalarParamSpec<RV, decltype(transform)>{transform};
  }
}

// Filter tuple to only include non-observed RVs
template <typename ObsBindings, typename RVTuple, std::size_t... Is>
auto filterNonObservedImpl(const RVTuple& rvs, std::index_sequence<Is...>) {
  // Create tuple of optionals, then filter
  auto maybe_include = [&]<std::size_t I>() {
    using RV = std::tuple_element_t<I, RVTuple>;
    if constexpr (is_rv_observed_v<RV, ObsBindings>) {
      return std::tuple<>();  // Observed - skip
    } else {
      return std::make_tuple(std::get<I>(rvs));  // Not observed - include
    }
  };
  return std::tuple_cat(maybe_include.template operator()<Is>()...);
}

template <typename ObsBindings, typename... RVs>
auto filterNonObserved(const std::tuple<RVs...>& rvs) {
  return filterNonObservedImpl<ObsBindings>(rvs, std::make_index_sequence<sizeof...(RVs)>{});
}

}  // namespace plate_detail

template <Symbolic LogProbExpr, typename... Params>
auto makePlateTransformedPosterior(LogProbExpr lp, Params... params) {
  // Create param specs with transforms
  auto specs = std::make_tuple(plate_detail::createPlateParamSpec(params)...);

  // Extract symbols
  auto symbols = std::make_tuple(plate_detail::extractPlateSymbol(params)...);

  // Compute gradient expressions (differentiate log-prob w.r.t. each param)
  auto grads = std::make_tuple(simplify(diff(lp, plate_detail::extractPlateSymbol(params)))...);

  // Start with empty data bindings
  auto empty_data = BinderPack<>{};

  return PlateTransformedPosteriorBuilder<LogProbExpr, decltype(empty_data), decltype(specs),
                                          decltype(symbols), decltype(grads)>{
      lp, empty_data, specs, symbols, grads};
}

// Version that filters out observed RVs - only creates param specs for non-observed
template <Symbolic LogProbExpr, typename ObsBindings, typename... Params>
auto makePlateTransformedPosteriorWithObs(LogProbExpr lp, ObsBindings obs, Params... params) {
  // Filter to only non-observed params
  auto all_rvs = std::make_tuple(params...);
  auto non_observed = plate_detail::filterNonObserved<ObsBindings>(all_rvs);

  // Apply to create specs and symbols from non-observed only
  auto specs = std::apply([](auto&... p) {
    return std::make_tuple(plate_detail::createPlateParamSpec(p)...);
  }, non_observed);

  auto symbols = std::apply([](auto&... p) {
    return std::make_tuple(plate_detail::extractPlateSymbol(p)...);
  }, non_observed);

  // Compute gradient expressions for non-observed params
  auto grads = std::apply([&lp](auto&... p) {
    return std::make_tuple(simplify(diff(lp, plate_detail::extractPlateSymbol(p)))...);
  }, non_observed);

  auto empty_data = BinderPack<>{};

  // Create posterior directly with observations already included
  return PlateTransformedPosterior<LogProbExpr, ObsBindings, decltype(empty_data),
                                    decltype(specs), decltype(symbols), decltype(grads)>{
      lp, obs, empty_data, specs, symbols, grads};
}

}  // namespace tempura::symbolic4
