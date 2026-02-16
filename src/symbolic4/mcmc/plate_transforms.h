#pragma once

#include <array>
#include <cstddef>
#include <experimental/meta>
#include <random>
#include <span>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include "meta/type_list_ops.h"
#include "symbolic4/core.h"
#include "symbolic4/distributions/indexed_node.h"
#include "symbolic4/indexed/indexed_eval.h"
#include "symbolic4/strategy/diff.h"
#include "symbolic4/mcmc/mutable_state.h"
#include "symbolic4/mcmc/samples.h"
#include "symbolic4/mcmc/sampler.h"
#include "symbolic4/mcmc/support.h"
#include "symbolic4/mcmc/transform_pack.h"
#include "symbolic4/mcmc/transforms.h"
#include "symbolic4/symbolic_state.h"

// ============================================================================
// plate_transforms.h - Integration of plates with automatic MCMC transforms
// ============================================================================
//
// This header extends the transform system to handle plate (indexed) parameters.
// The key insight is that plate parameters are collections of values that all
// share the same transform (inferred from the distribution).
//
// Usage:
//   auto alpha = gamma(2_c, 0.1_c);           // scalar, positive
//   auto theta = plate<Countries>(beta(alpha, 3.0_c));  // indexed, (0,1)
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

namespace plate_detail {

// Check if DimTag is in a TypeList<Dims...> (local fold, no type_list_ops.h)
template <typename DimTag, typename DimsList>
struct DimsContains : std::false_type {};

template <typename DimTag, typename... Dims>
struct DimsContains<DimTag, TypeList<Dims...>> {
  static constexpr bool value = (std::is_same_v<DimTag, Dims> || ...);
};

template <typename DimTag, typename DimsList>
constexpr bool dims_contains_v = DimsContains<DimTag, DimsList>::value;

// Extract first dim from TypeList<Dims...>
template <typename DimsList>
struct FirstDim;

template <typename First, typename... Rest>
struct FirstDim<TypeList<First, Rest...>> {
  using type = First;
};

template <typename DimsList>
using first_dim_t = typename FirstDim<DimsList>::type;

}  // namespace plate_detail

// ============================================================================
// DimSizes - Runtime dimension sizes for plates
// ============================================================================

template <typename... DimTags>
struct DimSizes {
  std::array<std::size_t, sizeof...(DimTags)> sizes;

  template <typename DimTag>
  constexpr auto get() const -> std::size_t {
    constexpr std::size_t idx = symbolic_state_detail::SymbolIndex<DimTag,
        std::tuple<DimTags...>>::value;
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
template <typename Param, typename Transform, typename... Dims>
struct IndexedParamSpec {
  [[no_unique_address]] Transform transform;
  std::size_t size_ = 0;

  static constexpr bool is_indexed = true;
  using dims_list = TypeList<Dims...>;

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
constexpr bool is_scalar_param_spec_v = core_traits_detail::isSpecOf<T, ScalarParamSpec>();

template <typename T>
constexpr bool is_indexed_param_spec_v = core_traits_detail::isSpecOf<T, IndexedParamSpec>();

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

// Helper: construct IndexedParamSpec<RV, Transform, Dims...> from IndexedRandomVar<Dist, Id, Dims...>
template <typename RV, typename Transform>
struct MakeIndexedParamSpecType {
 private:
  static consteval auto compute() -> std::meta::info {
    auto rv_args = std::meta::template_arguments_of(^^RV);
    std::vector<std::meta::info> spec_args;
    spec_args.push_back(^^RV);
    spec_args.push_back(^^Transform);
    for (std::size_t i = 2; i < rv_args.size(); ++i)
      spec_args.push_back(rv_args[i]);
    return std::meta::substitute(^^IndexedParamSpec, spec_args);
  }
 public:
  using type = [:compute():];
};

// For IndexedRandomVar (plate) — unpack dims from the RV
template <typename RV, typename Transform>
  requires IsIndexedRandomVar<RV>
constexpr auto makeParamSpec(Transform t) {
  return typename MakeIndexedParamSpecType<RV, Transform>::type{t, 0};
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
        non_observed_specs_{buildNonObservedSpecs(std::make_index_sequence<NumParamSlots>{})},
        non_observed_symbols_{buildNonObservedSymbols(std::make_index_sequence<NumParamSlots>{})},
        transform_pack_{non_observed_symbols_, non_observed_specs_} {}

  // Total state dimension (sum of all param sizes)
  auto stateDim() const -> std::size_t {
    return transform_pack_.stateDim();
  }

  // Evaluate log-probability at unconstrained state (flat vector API)
  auto logProb(std::span<const double> z) const -> double {
    return logProbImpl(z, std::make_index_sequence<NumParamSlots>{});
  }

  // Evaluate gradient at unconstrained state (flat vector API)
  auto gradient(std::span<const double> z) const -> std::vector<double> {
    return gradientImpl(z, std::make_index_sequence<NumParamSlots>{});
  }

  // =========================================================================
  // Binding-based evaluation API
  // =========================================================================

  // Evaluate log-probability at parameter values specified via bindings
  template <typename... Binders>
  auto logProbAt(BinderPack<Binders...> bindings) const -> double {
    return logProbTyped(initStateFromBindings(bindings));
  }

  // Evaluate gradient at parameter values specified via bindings.
  // Returns MutableState (GradientType) — queryable by symbol:
  //   grad[alpha] → double&, grad[z_b] → span<double>
  template <typename... Binders>
  auto gradientAt(BinderPack<Binders...> bindings) const {
    return computeGradient(initStateFromBindings(bindings));
  }

  // Verify analytic gradient matches finite-difference gradient
  template <typename... Binders>
  auto debugGradientCheck(BinderPack<Binders...> bindings, double tol = 1e-4) const -> bool {
    auto state = initStateFromBindings(bindings);
    auto analytic = computeGradient(state);
    auto analytic_vec = stateToVector(analytic, std::make_index_sequence<NumNonObserved>{});
    auto fd_vec = finiteDiffGradient(state);
    if (analytic_vec.size() != fd_vec.size()) return false;
    for (std::size_t i = 0; i < analytic_vec.size(); ++i) {
      if (std::abs(analytic_vec[i] - fd_vec[i]) > tol) return false;
    }
    return true;
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
  template <typename D>
  auto withDimension(D, std::size_t size) const {
    auto new_specs = setDimensionImpl<D>(size, std::make_index_sequence<NumParamSlots>{});
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
  // Check if Sym is in ObsBindings — used by type-level filter and isSymbolObserved()
  template <typename Sym, typename OB>
  struct IsSymbolInObsBindings {
    static constexpr bool value = false;
  };

  template <typename Sym, typename... Binders>
  struct IsSymbolInObsBindings<Sym, BinderPack<Binders...>> {
    static constexpr bool value = ((std::is_same_v<Sym, typename Binders::symbol_type>) || ...);
  };

  // -----------------------------------------------------------------------
  // Type-level filter: non-observed specs, symbols, and HMC slot types
  //
  // Uses conditional tuple_cat for specs/symbols (proven pattern), and
  // derives HmcState (MutableState) from NonObservedSymbolsTuple +
  // NonObservedSpecsTuple via a consteval reflection pass.
  // -----------------------------------------------------------------------

  template <std::size_t I>
  struct MaybeSpecType {
    using SymType = std::decay_t<std::tuple_element_t<I, ParamSymbolsTuple>>;
    using SpecType = std::tuple_element_t<I, ParamSpecsTuple>;
    using type = std::conditional_t<
        IsSymbolInObsBindings<SymType, ObsBindings>::value,
        std::tuple<>, std::tuple<SpecType>>;
  };

  template <std::size_t I>
  struct MaybeSymbolType {
    using SymType = std::decay_t<std::tuple_element_t<I, ParamSymbolsTuple>>;
    using type = std::conditional_t<
        IsSymbolInObsBindings<SymType, ObsBindings>::value,
        std::tuple<>, std::tuple<SymType>>;
  };

  template <typename IS> struct ConcatNonObservedSpecs;
  template <std::size_t... Is>
  struct ConcatNonObservedSpecs<std::index_sequence<Is...>> {
    using type = decltype(std::tuple_cat(std::declval<typename MaybeSpecType<Is>::type>()...));
  };

  template <typename IS> struct ConcatNonObservedSymbols;
  template <std::size_t... Is>
  struct ConcatNonObservedSymbols<std::index_sequence<Is...>> {
    using type = decltype(std::tuple_cat(std::declval<typename MaybeSymbolType<Is>::type>()...));
  };

  using NonObservedSpecsTuple =
      typename ConcatNonObservedSpecs<std::make_index_sequence<NumParamSlots>>::type;
  using NonObservedSymbolsTuple =
      typename ConcatNonObservedSymbols<std::make_index_sequence<NumParamSlots>>::type;

  // Map (Sym, Spec) → appropriate slot type
  template <typename Sym, typename Spec>
  using SlotFor = std::conditional_t<Spec::is_indexed, IndexedSlot<Sym>, ScalarSlot<Sym>>;

  // Derive MutableState from non-observed symbols/specs tuples
  template <typename SymsTuple, typename SpecsTuple, typename IS>
  struct DeriveHmcStateImpl;

  template <typename SymsTuple, typename SpecsTuple, std::size_t... Is>
  struct DeriveHmcStateImpl<SymsTuple, SpecsTuple, std::index_sequence<Is...>> {
    using type = MutableState<
        SlotFor<std::tuple_element_t<Is, SymsTuple>,
                std::tuple_element_t<Is, SpecsTuple>>...>;
  };

  using HmcState = typename DeriveHmcStateImpl<
      NonObservedSymbolsTuple, NonObservedSpecsTuple,
      std::make_index_sequence<std::tuple_size_v<NonObservedSymbolsTuple>>>::type;

  static constexpr std::size_t NumNonObserved = std::tuple_size_v<NonObservedSymbolsTuple>;

  // TransformPack for non-observed params — delegates transform/inverse/logJacobian
  using TransformPackType = TransformPack<NonObservedSymbolsTuple, NonObservedSpecsTuple>;

 public:
  using SamplesType = Samples<NonObservedSymbolsTuple, NonObservedSpecsTuple>;
  using GradientType = HmcState;

  // Run HMC and collect samples via typed leapfrog on HmcState.
  template <typename... Binders, std::uniform_random_bit_generator RNG>
  auto sample(HmcConfig config, BinderPack<Binders...> init, RNG& rng) const
      -> SamplesType {
    auto q = initStateFromBindings(init);
    double lp = logProbTyped(q);

    // Warmup
    for (std::size_t i = 0; i < config.warmup; ++i) {
      auto [next_q, next_lp, accepted] = hmcStep(q, lp, config.epsilon, config.steps, rng);
      q = std::move(next_q);
      lp = next_lp;
    }

    // Collect samples
    SamplesType samples(non_observed_symbols_, non_observed_specs_, totalDim());
    samples.reserve(config.draws);

    for (std::size_t i = 0; i < config.draws; ++i) {
      auto [next_q, next_lp, accepted] = hmcStep(q, lp, config.epsilon, config.steps, rng);
      q = std::move(next_q);
      lp = next_lp;
      auto constrained = transformFromState(q, std::make_index_sequence<NumNonObserved>{});
      samples.push_back(std::span<const double>(constrained));
    }

    return samples;
  }

 private:
  // =========================================================================
  // Data members
  // =========================================================================

  LogProbExpr log_prob_expr_;
  ObsBindings observations_;
  DataBindings data_bindings_;
  ParamSpecsTuple specs_;
  ParamSymbolsTuple symbols_;
  GradExprsTuple grad_exprs_;
  NonObservedSpecsTuple non_observed_specs_;
  NonObservedSymbolsTuple non_observed_symbols_;
  TransformPackType transform_pack_;

  // Mutable cache for indexed constrained values during binding construction.
  // One per non-observed param slot — indexed slots store pre-transformed
  // values here so the binding's span has valid lifetime.
  mutable std::array<std::vector<double>, NumNonObserved> constrained_cache_;

  // =========================================================================
  // Non-observed symbols/specs runtime construction
  // =========================================================================

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

  // =========================================================================
  // Observation check
  // =========================================================================

  template <typename Sym>
  static constexpr bool isSymbolObserved() {
    return IsSymbolInObsBindings<Sym, ObsBindings>::value;
  }

  // =========================================================================
  // Total dimension (sum of non-observed param sizes)
  // =========================================================================

  auto totalDim() const -> std::size_t {
    return transform_pack_.stateDim();
  }

  // =========================================================================
  // Typed evaluator bridge: HmcState → BinderPack → evaluateIndexed
  // =========================================================================

  // Build BinderPack from HmcState by reading each non-observed param by symbol.
  // Pre-transforms unconstrained z → constrained x before binding.
  template <std::size_t... Is>
  auto buildBindingsFromState(const HmcState& state, std::index_sequence<Is...>) const {
    auto binding_tuple = std::tuple_cat(bindOneParam<Is>(state)...);
    return tupleToBinderPack(binding_tuple);
  }

  template <std::size_t I>
  auto bindOneParam(const HmcState& state) const {
    using Sym = std::tuple_element_t<I, NonObservedSymbolsTuple>;
    using Spec = std::decay_t<std::tuple_element_t<I, NonObservedSpecsTuple>>;
    const auto& spec = std::get<I>(non_observed_specs_);

    if constexpr (!Spec::is_indexed) {
      double x = spec.transformValue(state[Sym{}]);
      return std::make_tuple(Sym{} = x);
    } else {
      auto z_span = state[Sym{}];
      constrained_cache_[I].resize(z_span.size());
      for (std::size_t i = 0; i < z_span.size(); ++i)
        constrained_cache_[I][i] = spec.transformValue(z_span[i]);
      return std::make_tuple(
          IndexedBinding<Sym, 1>{std::span<const double>(constrained_cache_[I])});
    }
  }

  // =========================================================================
  // logProb on typed state
  // =========================================================================

  auto logProbTyped(const HmcState& state) const -> double {
    auto bindings = buildBindingsFromState(state, std::make_index_sequence<NumNonObserved>{});
    auto merged = mergeAllBindings(bindings, observations_, data_bindings_);
    double lp = evaluateIndexed(log_prob_expr_, merged);
    double log_jac = logJacobianFromState(state, std::make_index_sequence<NumNonObserved>{});
    return lp + log_jac;
  }

  template <std::size_t... Is>
  auto logJacobianFromState(const HmcState& state, std::index_sequence<Is...>) const -> double {
    double total = 0.0;
    (logJacobianOneParam<Is>(state, total), ...);
    return total;
  }

  template <std::size_t I>
  void logJacobianOneParam(const HmcState& state, double& total) const {
    using Sym = std::tuple_element_t<I, NonObservedSymbolsTuple>;
    using Spec = std::decay_t<std::tuple_element_t<I, NonObservedSpecsTuple>>;
    const auto& spec = std::get<I>(non_observed_specs_);

    if constexpr (!Spec::is_indexed) {
      total += spec.logJacobian(state[Sym{}]);
    } else {
      auto z_span = state[Sym{}];
      for (std::size_t i = 0; i < z_span.size(); ++i)
        total += spec.logJacobian(z_span[i]);
    }
  }

  // =========================================================================
  // Gradient on typed state
  // =========================================================================

  auto computeGradient(const HmcState& q) const -> HmcState {
    auto grad = makeZeroState(non_observed_symbols_, non_observed_specs_,
                              std::make_index_sequence<NumNonObserved>{});

    auto bindings = buildBindingsFromState(q, std::make_index_sequence<NumNonObserved>{});
    auto merged = mergeAllBindings(bindings, observations_, data_bindings_);

    if constexpr (std::tuple_size_v<GradExprsTuple> == NumParamSlots) {
      analyticGradAll(q, merged, grad, std::make_index_sequence<NumParamSlots>{});
    }

    return grad;
  }

  // Dispatch analytic gradient over ALL param slots (including observed — skipped)
  template <typename Bindings, std::size_t... Is>
  void analyticGradAll(const HmcState& q, const Bindings& bindings,
                       HmcState& grad, std::index_sequence<Is...>) const {
    // NonObserved index counter — incremented only for non-observed params
    std::size_t no_idx = 0;
    (analyticGradOneSlot<Is>(q, bindings, grad, no_idx), ...);
  }

  template <std::size_t I, typename Bindings>
  void analyticGradOneSlot(const HmcState& q, const Bindings& bindings,
                           HmcState& grad, std::size_t& no_idx) const {
    using SymType = std::decay_t<std::tuple_element_t<I, ParamSymbolsTuple>>;
    if constexpr (isSymbolObserved<SymType>()) {
      return;  // Skip observed params
    } else {
      const auto& spec = std::get<I>(specs_);
      const auto& grad_expr = std::get<I>(grad_exprs_);

      if constexpr (is_scalar_param_spec_v<std::decay_t<decltype(spec)>>) {
        double grad_x = evaluateIndexed(grad_expr, bindings);
        grad[SymType{}] = spec.chainRuleGrad(grad_x, q[SymType{}]);
      } else {
        auto z_span = q[SymType{}];
        auto g_span = grad[SymType{}];
        for (std::size_t i = 0; i < z_span.size(); ++i) {
          double grad_x = evaluateAtIndex(grad_expr, bindings, i);
          g_span[i] = spec.chainRuleGrad(grad_x, z_span[i]);
        }
      }
      ++no_idx;
    }
  }

  // =========================================================================
  // Serialization boundary: HmcState → flat vector (for Samples storage)
  // =========================================================================

  template <std::size_t... Is>
  auto transformFromState(const HmcState& state, std::index_sequence<Is...>) const
      -> std::vector<double> {
    std::vector<double> out;
    out.reserve(totalDim());
    (appendTransformed<Is>(state, out), ...);
    return out;
  }

  template <std::size_t I>
  void appendTransformed(const HmcState& state, std::vector<double>& out) const {
    using Sym = std::tuple_element_t<I, NonObservedSymbolsTuple>;
    using Spec = std::decay_t<std::tuple_element_t<I, NonObservedSpecsTuple>>;
    const auto& spec = std::get<I>(non_observed_specs_);

    if constexpr (!Spec::is_indexed) {
      out.push_back(spec.transformValue(state[Sym{}]));
    } else {
      auto z_span = state[Sym{}];
      for (std::size_t i = 0; i < z_span.size(); ++i)
        out.push_back(spec.transformValue(z_span[i]));
    }
  }

  // =========================================================================
  // Leapfrog integration on typed state
  // =========================================================================

  auto leapfrog(HmcState q, HmcState p, double eps, std::size_t steps) const
      -> std::pair<HmcState, HmcState> {
    double half_eps = eps / 2.0;
    auto grad = computeGradient(q);
    // Negate gradient: HMC uses potential energy = -logProb, so grad_U = -grad_logProb
    // p -= half_eps * grad_U = p += half_eps * grad_logProb
    applyUpdate(p, grad, half_eps, std::make_index_sequence<NumNonObserved>{});

    for (std::size_t step = 0; step < steps; ++step) {
      applyUpdate(q, p, eps, std::make_index_sequence<NumNonObserved>{});
      if (step < steps - 1) {
        grad = computeGradient(q);
        applyUpdate(p, grad, eps, std::make_index_sequence<NumNonObserved>{});
      }
    }

    grad = computeGradient(q);
    applyUpdate(p, grad, half_eps, std::make_index_sequence<NumNonObserved>{});
    negate(p, std::make_index_sequence<NumNonObserved>{});
    return {std::move(q), std::move(p)};
  }

  template <std::size_t... Is>
  static void applyUpdate(HmcState& target, const HmcState& source, double scale,
                           std::index_sequence<Is...>) {
    (updateOneParam<Is>(target, source, scale), ...);
  }

  template <std::size_t I>
  static void updateOneParam(HmcState& target, const HmcState& source, double scale) {
    using Sym = std::tuple_element_t<I, NonObservedSymbolsTuple>;
    using Spec = std::decay_t<std::tuple_element_t<I, NonObservedSpecsTuple>>;
    if constexpr (!Spec::is_indexed) {
      target[Sym{}] += scale * source[Sym{}];
    } else {
      auto t = target[Sym{}];
      auto s = source[Sym{}];
      for (std::size_t i = 0; i < t.size(); ++i)
        t[i] += scale * s[i];
    }
  }

  template <std::size_t... Is>
  static void negate(HmcState& state, std::index_sequence<Is...>) {
    (negateOneParam<Is>(state), ...);
  }

  template <std::size_t I>
  static void negateOneParam(HmcState& state) {
    using Sym = std::tuple_element_t<I, NonObservedSymbolsTuple>;
    using Spec = std::decay_t<std::tuple_element_t<I, NonObservedSpecsTuple>>;
    if constexpr (!Spec::is_indexed) {
      state[Sym{}] = -state[Sym{}];
    } else {
      auto span = state[Sym{}];
      for (std::size_t i = 0; i < span.size(); ++i)
        span[i] = -span[i];
    }
  }

  // =========================================================================
  // HMC step: sample momentum, leapfrog, Metropolis accept/reject
  // =========================================================================

  template <std::uniform_random_bit_generator RNG>
  auto hmcStep(const HmcState& current, double current_lp,
               double eps, std::size_t steps, RNG& rng) const
      -> std::tuple<HmcState, double, bool> {
    auto p = sampleMomentum(rng, std::make_index_sequence<NumNonObserved>{});
    double current_ke = kineticEnergy(p, std::make_index_sequence<NumNonObserved>{});
    double current_H = -current_lp + current_ke;

    auto [proposed_q, proposed_p] = leapfrog(current, p, eps, steps);

    double proposed_lp = logProbTyped(proposed_q);
    if (!std::isfinite(proposed_lp)) {
      return {current, current_lp, false};
    }

    double proposed_ke = kineticEnergy(proposed_p, std::make_index_sequence<NumNonObserved>{});
    double proposed_H = -proposed_lp + proposed_ke;
    double log_accept = current_H - proposed_H;

    bool accept = false;
    if (log_accept >= 0.0) {
      accept = true;
    } else {
      std::uniform_real_distribution<double> uniform(0.0, 1.0);
      accept = std::log(uniform(rng)) < log_accept;
    }

    if (accept) {
      return {std::move(proposed_q), proposed_lp, true};
    }
    return {current, current_lp, false};
  }

  template <std::uniform_random_bit_generator RNG, std::size_t... Is>
  auto sampleMomentum(RNG& rng, std::index_sequence<Is...>) const -> HmcState {
    auto p = makeZeroState(non_observed_symbols_, non_observed_specs_,
                           std::make_index_sequence<NumNonObserved>{});
    std::normal_distribution<double> normal(0.0, 1.0);
    (fillMomentum<Is>(p, normal, rng), ...);
    return p;
  }

  template <std::size_t I, typename Dist, std::uniform_random_bit_generator RNG>
  static void fillMomentum(HmcState& p, Dist& dist, RNG& rng) {
    using Sym = std::tuple_element_t<I, NonObservedSymbolsTuple>;
    using Spec = std::decay_t<std::tuple_element_t<I, NonObservedSpecsTuple>>;
    if constexpr (!Spec::is_indexed) {
      p[Sym{}] = dist(rng);
    } else {
      auto span = p[Sym{}];
      for (std::size_t i = 0; i < span.size(); ++i)
        span[i] = dist(rng);
    }
  }

  template <std::size_t... Is>
  static auto kineticEnergy(const HmcState& p, std::index_sequence<Is...>) -> double {
    double ke = 0.0;
    (keOneParam<Is>(p, ke), ...);
    return ke / 2.0;
  }

  template <std::size_t I>
  static void keOneParam(const HmcState& p, double& ke) {
    using Sym = std::tuple_element_t<I, NonObservedSymbolsTuple>;
    using Spec = std::decay_t<std::tuple_element_t<I, NonObservedSpecsTuple>>;
    if constexpr (!Spec::is_indexed) {
      double v = p[Sym{}];
      ke += v * v;
    } else {
      auto span = p[Sym{}];
      for (std::size_t i = 0; i < span.size(); ++i)
        ke += span[i] * span[i];
    }
  }

  // =========================================================================
  // Init state from bindings: BinderPack → HmcState
  // =========================================================================

  template <typename... Binders>
  auto initStateFromBindings(BinderPack<Binders...> bindings) const -> HmcState {
    auto state = makeZeroState(non_observed_symbols_, non_observed_specs_,
                               std::make_index_sequence<NumNonObserved>{});
    initFromBindingsImpl(state, bindings, std::make_index_sequence<NumParamSlots>{});
    return state;
  }

  template <typename... Binders, std::size_t... Is>
  void initFromBindingsImpl(HmcState& state, BinderPack<Binders...> bindings,
                            std::index_sequence<Is...>) const {
    (initOneParam<Is>(state, bindings), ...);
  }

  template <std::size_t I, typename... Binders>
  void initOneParam(HmcState& state, BinderPack<Binders...> bindings) const {
    using SymType = std::decay_t<std::tuple_element_t<I, ParamSymbolsTuple>>;
    if constexpr (isSymbolObserved<SymType>()) {
      return;
    } else {
      const auto& spec = std::get<I>(specs_);
      if constexpr (is_scalar_param_spec_v<std::decay_t<decltype(spec)>>) {
        state[SymType{}] = static_cast<double>(bindings[SymType{}]);
      } else {
        auto binding = bindings[SymType{}];
        auto z_span = state[SymType{}];
        std::size_t n = spec.size();
        for (std::size_t i = 0; i < n; ++i)
          z_span[i] = spec.transform.inverse(binding.at(i));
      }
    }
  }

  // =========================================================================
  // Finite-difference gradient (for gradient validation)
  // =========================================================================

  auto finiteDiffGradient(const HmcState& state) const -> std::vector<double> {
    auto z = stateToVector(state, std::make_index_sequence<NumNonObserved>{});
    constexpr double eps = 1e-7;
    std::vector<double> grad(z.size());
    for (std::size_t i = 0; i < z.size(); ++i) {
      auto z_plus = z;
      auto z_minus = z;
      z_plus[i] += eps;
      z_minus[i] -= eps;
      auto s_plus = stateFromSpan(z_plus);
      auto s_minus = stateFromSpan(z_minus);
      grad[i] = (logProbTyped(s_plus) - logProbTyped(s_minus)) / (2.0 * eps);
    }
    return grad;
  }

  // =========================================================================
  // Span-based API — delegates through HmcState for backward compatibility
  // =========================================================================

  template <std::size_t... Is>
  auto logProbImpl(std::span<const double> z, std::index_sequence<Is...>) const -> double {
    auto state = stateFromSpan(z);
    return logProbTyped(state);
  }

  template <std::size_t... Is>
  auto gradientImpl(std::span<const double> z, std::index_sequence<Is...>) const
      -> std::vector<double> {
    auto state = stateFromSpan(z);
    auto grad = computeGradient(state);
    return stateToVector(grad, std::make_index_sequence<NumNonObserved>{});
  }

  // Build HmcState from a flat span (for backward-compat span-based API)
  auto stateFromSpan(std::span<const double> z) const -> HmcState {
    auto state = makeZeroState(non_observed_symbols_, non_observed_specs_,
                               std::make_index_sequence<NumNonObserved>{});
    fillStateFromSpan(state, z, std::make_index_sequence<NumNonObserved>{});
    return state;
  }

  template <std::size_t... Is>
  void fillStateFromSpan(HmcState& state, std::span<const double> z,
                         std::index_sequence<Is...>) const {
    std::size_t offset = 0;
    (fillOneSlotFromSpan<Is>(state, z, offset), ...);
  }

  template <std::size_t I>
  void fillOneSlotFromSpan(HmcState& state, std::span<const double> z,
                           std::size_t& offset) const {
    using Sym = std::tuple_element_t<I, NonObservedSymbolsTuple>;
    using Spec = std::decay_t<std::tuple_element_t<I, NonObservedSpecsTuple>>;
    const auto& spec = std::get<I>(non_observed_specs_);

    if constexpr (!Spec::is_indexed) {
      state[Sym{}] = z[offset];
      offset += 1;
    } else {
      auto span = state[Sym{}];
      std::size_t n = spec.size();
      for (std::size_t i = 0; i < n; ++i)
        span[i] = z[offset + i];
      offset += n;
    }
  }

  // Convert HmcState gradient back to flat vector (for backward-compat API)
  template <std::size_t... Is>
  auto stateToVector(const HmcState& state, std::index_sequence<Is...>) const
      -> std::vector<double> {
    std::vector<double> out;
    out.reserve(totalDim());
    (appendSlotToVector<Is>(state, out), ...);
    return out;
  }

  template <std::size_t I>
  void appendSlotToVector(const HmcState& state, std::vector<double>& out) const {
    using Sym = std::tuple_element_t<I, NonObservedSymbolsTuple>;
    using Spec = std::decay_t<std::tuple_element_t<I, NonObservedSpecsTuple>>;
    if constexpr (!Spec::is_indexed) {
      out.push_back(state[Sym{}]);
    } else {
      auto span = state[Sym{}];
      for (std::size_t i = 0; i < span.size(); ++i)
        out.push_back(span[i]);
    }
  }

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

  // =========================================================================
  // Merge bindings, setDimension
  // =========================================================================

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

  template <typename DimTag, std::size_t... Is>
  auto setDimensionImpl(std::size_t size, std::index_sequence<Is...>) const {
    return std::make_tuple(setDimForSpec<DimTag, Is>(size)...);
  }

  template <typename DimTag, std::size_t I>
  auto setDimForSpec(std::size_t size) const {
    auto spec = std::get<I>(specs_);
    if constexpr (is_indexed_param_spec_v<std::decay_t<decltype(spec)>>) {
      using SpecDims = typename std::decay_t<decltype(spec)>::dims_list;
      if constexpr (plate_detail::dims_contains_v<DimTag, SpecDims>) {
        spec.size_ = size;
      }
    }
    return spec;
  }
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
  template <typename D>
  auto withDimension(D, std::size_t size) const {
    auto new_specs = setDimension<D>(size);
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
      if constexpr (plate_detail::dims_contains_v<DimTag, SpecDims>) {
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
      auto size = inferSizeFromBindings<plate_detail::first_dim_t<SpecDims>>(bindings);
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
    return typename detail::MakeIndexedParamSpecType<RV, decltype(transform)>::type{transform, 0};
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
  auto grads = std::make_tuple(diff(lp, plate_detail::extractPlateSymbol(params))...);

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
    return std::make_tuple(diff(lp, plate_detail::extractPlateSymbol(p))...);
  }, non_observed);

  auto empty_data = BinderPack<>{};

  // Create posterior directly with observations already included
  return PlateTransformedPosterior<LogProbExpr, ObsBindings, decltype(empty_data),
                                    decltype(specs), decltype(symbols), decltype(grads)>{
      lp, obs, empty_data, specs, symbols, grads};
}

}  // namespace tempura::symbolic4
