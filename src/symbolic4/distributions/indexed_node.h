#pragma once

#include "meta/type_list_ops.h"
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
//   auto theta = plate<Countries>(beta(lit(2), lit(3)));
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

// Base case: no dimensions left - return expression as-is
template <typename DimsToSum, typename Expr>
struct BuildNestedSumOver {
  static constexpr auto build(Expr e) { return e; }
};

// Recursive case: wrap in SumOver for first dim, recurse for rest
template <typename First, typename... Rest, typename Expr>
struct BuildNestedSumOver<TypeList<First, Rest...>, Expr> {
  static constexpr auto build(Expr e) {
    return sumOver<First>(BuildNestedSumOver<TypeList<Rest...>, Expr>::build(e));
  }
};

template <typename DimsToSum, typename Expr>
constexpr auto buildNestedSumOver(Expr e) {
  return BuildNestedSumOver<DimsToSum, Expr>::build(e);
}

// Helper to convert TypeList<Dims...> to IndexedSymbol<Id, Dims...>
template <typename Id, typename DimsList>
struct MakeIndexedSymbol;

template <typename Id, typename... Dims>
struct MakeIndexedSymbol<Id, TypeList<Dims...>> {
  using type = IndexedSymbol<Id, Dims...>;
};

template <typename Id, typename DimsList>
using make_indexed_symbol_t = typename MakeIndexedSymbol<Id, DimsList>::type;

}  // namespace detail

// ============================================================================
// IndexedRandomVar - A plate of random variables
// ============================================================================

template <typename Dist, typename Id, typename DimsList>
struct IndexedRandomVar {
  using id_type = Id;
  using dist_type = Dist;
  using dims_list = DimsList;
  using symbol_type = detail::make_indexed_symbol_t<Id, DimsList>;
  using discoverable_type = Atom<Id, IndexedSample<Dist, DimsList>>;

  // For backward compatibility: single-dim case exposes dim_tag
  using dim_tag = Head_t<DimsList>;

  [[no_unique_address]] Dist dist_;

  constexpr explicit IndexedRandomVar(Dist dist) : dist_{dist} {}

  // Discoverable symbol (for use in expressions - carries dist info)
  // Enables auto-discovery by collectLogProbs and discoverParams
  constexpr auto sym() const { return discoverable_type{IndexedSample<Dist, DimsList>{dist_}}; }

  // Free symbol for bindings (IndexedSymbol - no effect, just identity + dims)
  static constexpr auto freeSym() { return symbol_type{}; }

  // Implicit conversion to discoverable symbol (for use in expressions)
  constexpr operator discoverable_type() const { return sym(); }

  // Log-probability for ONE instance (symbolic template)
  constexpr auto instanceLogProb() const { return dist_.logProbFor(sym()); }

  // Total log-probability = nested SumOver for all dimensions
  // For dims = [C, Y], returns SumOver<C, SumOver<Y, logProb>>
  constexpr auto logProb() const {
    return detail::buildNestedSumOver<DimsList>(instanceLogProb());
  }

  // Unnormalized version for MCMC
  constexpr auto unnormalizedLogProb() const {
    return detail::buildNestedSumOver<DimsList>(dist_.unnormalizedLogProbFor(sym()));
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
struct IsIndexedRandomVarTrait : std::false_type {};

template <typename Dist, typename Id, typename DimsList>
struct IsIndexedRandomVarTrait<IndexedRandomVar<Dist, Id, DimsList>> : std::true_type {};

template <typename T>
constexpr bool is_indexed_random_var_v = IsIndexedRandomVarTrait<T>::value;

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
//   auto theta = plate<Countries>(beta(lit(2), lit(3)));
//
// Creates IndexedRandomVar with dims_list = TypeList<Countries>

template <typename DimTag, typename Id = decltype([] {}), IsRandomVar RV>
  requires (!IsIndexedRandomVar<RV>)
constexpr auto plate(const RV& rv) {
  using Dist = typename RV::dist_type;
  using DimsList = TypeList<DimTag>;
  return IndexedRandomVar<Dist, Id, DimsList>{rv.dist()};
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

template <typename DimTag, typename Id = decltype([] {}), IsIndexedRandomVar RV>
constexpr auto plate(const RV& rv) {
  using Dist = typename RV::dist_type;
  using OldDims = typename RV::dims_list;
  using NewDims = Concat_t<OldDims, TypeList<DimTag>>;
  return IndexedRandomVar<Dist, Id, NewDims>{rv.dist()};
}

// ============================================================================
// toSymbolic overload for IndexedRandomVar
// ============================================================================

template <typename Dist, typename Id, typename DimsList>
constexpr auto toSymbolic(const IndexedRandomVar<Dist, Id, DimsList>& node) {
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
