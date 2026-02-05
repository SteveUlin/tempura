#pragma once

#include "symbolic4/distributions/random_var.h"
#include "symbolic4/indexed/dim.h"
#include "symbolic4/indexed/gather.h"
#include "symbolic4/indexed/sum_over.h"

// ============================================================================
// indexed_node.h - Plate notation for replicated random variables
// ============================================================================
//
// A plate represents N i.i.d. random variables without creating N distinct
// types. This is the standard notation in probabilistic programming (BUGS,
// Stan, Pyro, NumPyro).
//
// Single plate:
//   struct Countries {};
//   auto theta = plate<Countries>(beta(2_c, 3_c));
//   // theta.sym() is IndexedSymbol<Id, Countries>
//
// Nested plates:
//   struct Years {};
//   auto y = plate<Years>(plate<Countries>(normal(mu, sigma)));
//   // y.sym() is IndexedSymbol<Id, Countries, Years>
//   // y.logProb() is SumOver<Years, SumOver<Countries, log N(y | mu, sigma)>>
//
// ============================================================================

namespace tempura::symbolic4 {

// ============================================================================
// Helper: Build nested SumOver from TypeList
// ============================================================================

namespace detail {

// Build nested SumOver from a dimension pack: SumOver<D0, SumOver<D1, ... expr>>
// Base case: no dimensions left
template <Symbolic Expr>
constexpr auto buildNestedSumOver(Expr e) {
  return e;
}

// Recursive case: wrap in SumOver for first dim, recurse for rest
template <typename First, typename... Rest, Symbolic Expr>
constexpr auto buildNestedSumOver(Expr e) {
  return sumOver<First>(buildNestedSumOver<Rest...>(e));
}

}  // namespace detail

// Reconstruct IndexedSymbol<Id, Dims...> from an IndexedSample effect.
// Defined here (before IndexedRandomVar) because IndexedSymbol is already declared.
template <typename EffectType, typename IdType>
struct MakeIndexedSymFromEffect {
 private:
  static consteval auto compute() -> std::meta::info {
    auto effect_args = std::meta::template_arguments_of(^^EffectType);
    // effect_args = [Dist, Dims...] — skip Dist, prepend Id
    std::vector<std::meta::info> sym_args;
    sym_args.push_back(^^IdType);
    for (std::size_t i = 1; i < effect_args.size(); ++i)
      sym_args.push_back(effect_args[i]);
    return std::meta::substitute(^^IndexedSymbol, sym_args);
  }
 public:
  using type = [:compute():];
};

template <typename EffectType, typename IdType>
using make_indexed_sym_from_effect_t = typename MakeIndexedSymFromEffect<EffectType, IdType>::type;

// ============================================================================
// IndexedRandomVar - A plate of random variables
// ============================================================================

template <typename Dist, typename Id, typename... Dims>
struct IndexedRandomVar {
  using id_type = Id;
  using dist_type = Dist;
  using dims_list = TypeList<Dims...>;
  using symbol_type = IndexedSymbol<Id, Dims...>;
  using discoverable_type = Atom<Id, IndexedSample<Dist, Dims...>>;

  [[no_unique_address]] Dist dist_;

  constexpr explicit IndexedRandomVar(Dist dist) : dist_{dist} {}

  // Discoverable symbol (for use in expressions - carries dist info)
  // Enables auto-discovery by collectLogProbs and discoverParams
  constexpr auto sym() const { return discoverable_type{IndexedSample<Dist, Dims...>{dist_}}; }

  // Free symbol for bindings (IndexedSymbol - no effect, just identity + dims)
  static constexpr auto freeSym() { return symbol_type{}; }

  // Implicit conversion to discoverable symbol (for use in expressions)
  constexpr operator discoverable_type() const { return sym(); }

  // Log-probability for ONE instance (symbolic template)
  constexpr auto instanceLogProb() const { return dist_.logProbFor(sym()); }

  // Total log-probability = nested SumOver for all dimensions
  // For dims = [C, Y], returns SumOver<C, SumOver<Y, logProb>>
  constexpr auto logProb() const {
    return detail::buildNestedSumOver<Dims...>(instanceLogProb());
  }

  // Unnormalized version for MCMC
  constexpr auto unnormalizedLogProb() const {
    return detail::buildNestedSumOver<Dims...>(dist_.unnormalizedLogProbFor(sym()));
  }

  // Access the distribution
  constexpr const auto& dist() const { return dist_; }

  // Binding syntax: theta = indexed(values)
  // Returns 1D binding for single-dim, multi-dim for nested plates
  auto operator=(IndexedValues iv) const {
    return IndexedBinding<symbol_type>{iv.values};
  }

  // Multi-dim binding with shape
  template <typename... ShapeDims>
  auto operator=(IndexedValuesND<ShapeDims...> iv) const {
    return IndexedBinding<symbol_type, sizeof...(ShapeDims)>{iv.data, iv.shape.sizes};
  }

  // Cross-plate indexing: z[idx] returns gather(z, idx)
  // Enables natural syntax like: a_bar + sigma * z[district_idx]
  template <SymbolicLike Index>
  constexpr auto operator[](Index idx) const {
    return gather(sym(), idx);
  }
};

// Type traits for IndexedRandomVar
template <typename T>
constexpr bool is_indexed_random_var_v = core_traits_detail::isSpecOf<T, IndexedRandomVar>();

// Concept for IndexedRandomVar
template <typename T>
concept IsIndexedRandomVar = requires(const T& node) {
  typename T::id_type;
  typename T::dims_list;
  { node.sym() };
  { node.logProb() };
  { node.dist() };
};

// ============================================================================
// plate<DimTag>() - Wrap a RandomVar into an IndexedRandomVar
// ============================================================================
//
// Single-plate usage:
//   auto theta = plate<Countries>(beta(2_c, 3_c));
//
// Creates IndexedRandomVar with dims_list = TypeList<Countries>

template <typename DimTag, typename Id = decltype([] {}), IsRandomVar RV>
  requires (!IsIndexedRandomVar<RV>)
constexpr auto plate(const RV& rv) {
  using Dist = typename RV::dist_type;
  return IndexedRandomVar<Dist, Id, DimTag>{rv.dist()};
}

// ============================================================================
// plate<DimTag>() - Wrap an IndexedRandomVar (nested plates)
// ============================================================================
//
// Nested-plate usage:
//   auto y = plate<Years>(plate<Countries>(normal(mu, sigma)));
//
// Appends new dimension to the dims_list:
//   plate<Countries>(...) has dims = [Countries]
//   plate<Years>(plate<Countries>(...)) has dims = [Countries, Years]

// Nested plate: append DimTag to existing dims pack.
// We extract the existing Dims... from the inner IndexedRandomVar and expand with DimTag.
template <typename DimTag, typename Id = decltype([] {}),
          typename Dist, typename OldId, typename... OldDims>
constexpr auto plate(const IndexedRandomVar<Dist, OldId, OldDims...>& rv) {
  return IndexedRandomVar<Dist, Id, OldDims..., DimTag>{rv.dist()};
}

// ============================================================================
// Reconstruct IndexedRandomVar<Dist, Id, Dims...> from an IndexedSample effect.
// Must be after IndexedRandomVar definition since it references ^^IndexedRandomVar.
template <typename EffectType, typename IdType>
struct MakeIndexedRVFromEffect {
 private:
  static consteval auto compute() -> std::meta::info {
    auto effect_args = std::meta::template_arguments_of(^^EffectType);
    std::vector<std::meta::info> rv_args;
    rv_args.push_back(effect_args[0]); // Dist
    rv_args.push_back(^^IdType);       // Id
    for (std::size_t i = 1; i < effect_args.size(); ++i)
      rv_args.push_back(effect_args[i]); // Dims...
    return std::meta::substitute(^^IndexedRandomVar, rv_args);
  }
 public:
  using type = [:compute():];
};

template <typename EffectType, typename IdType>
using make_indexed_rv_from_effect_t = typename MakeIndexedRVFromEffect<EffectType, IdType>::type;

// toSymbolic overload for IndexedRandomVar
// ============================================================================

template <typename Dist, typename Id, typename... Dims>
constexpr auto toSymbolic(const IndexedRandomVar<Dist, Id, Dims...>& node) {
  return node.sym();
}

// ============================================================================
// logProb overloads for IndexedRandomVar (to work with joint.h)
// ============================================================================

template <IsIndexedRandomVar RV>
constexpr auto logProb(const RV& rv) {
  return rv.logProb();
}

template <IsIndexedRandomVar RV>
constexpr auto unnormalizedLogProb(const RV& rv) {
  return rv.unnormalizedLogProb();
}

}  // namespace tempura::symbolic4
