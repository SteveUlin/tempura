#pragma once

#include "meta/type_list_ops.h"
#include "symbolic4/core.h"
#include "symbolic4/distributions/prob_terminals.h"  // ProbTerminals (default handler)
#include "symbolic4/indexed/data.h"
#include "symbolic4/indexed/dim.h"
#include "symbolic4/indexed/gather.h"
#include "symbolic4/indexed/sum_over.h"
#include "symbolic4/operators.h"

// ============================================================================
// indexed_eval.h - Evaluation for multi-dimensional indexed expressions
// ============================================================================
//
// Extends evaluate() to handle:
//   - IndexedSymbol with multiple dimensions
//   - Nested SumOver / ReduceOver expressions
//   - Broadcasting (1D symbol used in 2D context)
//
// Key design: Terminal dispatch is delegated to a Terminals policy type.
// BaseTerminals handles domain-independent atoms (constants, free symbols,
// indexed symbols). ProbTerminals extends it with probabilistic atoms
// (Sample, IndexedSample). Custom domains can define their own terminals.
//
// ============================================================================

namespace tempura::symbolic4 {

// ============================================================================
// StaticDimIndexMap — compile-time dispatch replaces runtime RTTI
// ============================================================================
//
// Each DimTag becomes a base class carrying its current index and size.
// Lookup is a zero-cost static_cast, not a hash map probe.

template <typename DimTag>
struct DimState {
  SizeT index = 0;
  SizeT size = 0;
};

template <typename... DimTags>
struct StaticDimIndexMap : DimState<DimTags>... {
  template <typename D>
  void set(SizeT i, SizeT s) {
    if constexpr ((std::is_same_v<D, DimTags> || ...)) {
      DimState<D>::index = i;
      DimState<D>::size = s;
    }
  }

  template <typename D>
  auto get() const -> SizeT {
    if constexpr ((std::is_same_v<D, DimTags> || ...)) {
      return DimState<D>::index;
    } else {
      return 0;
    }
  }

  template <typename D>
  auto getSize() const -> SizeT {
    if constexpr ((std::is_same_v<D, DimTags> || ...)) {
      return DimState<D>::size;
    } else {
      return 0;
    }
  }

  template <typename D>
  auto has() const -> bool {
    return (std::is_same_v<D, DimTags> || ...);
  }
};

// Empty specialization: no indexed bindings → no dims to track
template <>
struct StaticDimIndexMap<> {
  template <typename D> void set(SizeT, SizeT) {}
  template <typename D> auto get() const -> SizeT { return 0; }
  template <typename D> auto getSize() const -> SizeT { return 0; }
  template <typename D> auto has() const -> bool { return false; }
};

// ============================================================================
// Collect DimTags from IndexedBindings
// ============================================================================

namespace dim_map_detail {

template <typename B>
struct DimsOf {
  using Type = TypeList<>;
};

template <typename B>
  requires is_indexed_binding_v<B>
struct DimsOf<B> {
  using Type = typename B::symbol_type::dims_list;
};

template <typename IndexedBindings>
struct CollectDims;

template <typename... Binders>
struct CollectDims<BinderPack<Binders...>> {
  using Type = Unique_t<Concat_t<typename DimsOf<Binders>::Type...>>;
};

template <typename DimsList>
struct MakeDimMap;

template <typename... DimTags>
struct MakeDimMap<TypeList<DimTags...>> {
  using Type = StaticDimIndexMap<DimTags...>;
};

}  // namespace dim_map_detail

// ============================================================================
// IndexedEval - Interpreter for expressions with indexed bindings
// ============================================================================
//
// Template parameters:
//   ScalarBindings  - BinderPack of scalar bindings
//   IndexedBindings - BinderPack of indexed bindings
//   Terminals       - Terminal dispatch policy (default: ProbTerminals)
//
// The Terminals policy decouples terminal handling from the fold machinery.
// BaseTerminals handles domain-independent atoms. ProbTerminals extends it
// with probabilistic atoms (Sample, IndexedSample). Custom domains can
// define their own terminal handler.

template <typename ScalarBindings, typename IndexedBindings,
          typename Terminals = ProbTerminals>
struct IndexedEval {
  using result_type = double;
  using DimMapType = typename dim_map_detail::MakeDimMap<
      typename dim_map_detail::CollectDims<IndexedBindings>::Type>::Type;

  struct context_type {
    ScalarBindings scalars;
    IndexedBindings indexed;
    DimMapType dim_indices;
  };

  // Terminal handling — delegates to Terminals policy
  template <typename T>
  static auto terminal(T term, context_type& ctx) -> double {
    return Terminals::template eval<IndexedEval>(term, ctx);
  }

  // Combine child results using the operator functor
  template <typename Op, typename... Rs>
  static constexpr auto combine(context_type&, Op op, Rs... children) -> double {
    return op(children...);
  }

  // Public: Look up an IndexedSymbol value using current dimension indices.
  // Terminal handlers call this via Interp::lookupIndexedSymbol<Sym>(sym, ctx).
  template <typename Sym>
  static auto lookupIndexedSymbol([[maybe_unused]] Sym sym, context_type& ctx) -> double {
    const auto& binding = ctx.indexed[sym];
    return lookupAtIndices<Sym>(binding, ctx.dim_indices,
                                std::make_index_sequence<Sym::ndims>{});
  }

 private:
  template <typename Sym, typename Binding, typename DimMap, SizeT... Is>
  static auto lookupAtIndices(const Binding& binding, const DimMap& dim_indices,
                              std::index_sequence<Is...>) -> double {
    return binding.at(dim_indices.template get<detail::At<Is, typename Sym::dims_list>>()...);
  }
};

// ============================================================================
// Get dimension size from bindings
// ============================================================================
//
// Searches through indexed bindings to find one that has DimTag and returns
// its size. Uses fold expression to iterate through binders without casting.

namespace detail {

// Check if a single binder has the dimension and return its size, or 0
template <typename DimTag, typename Binder, typename Pack>
auto getDimSizeFromBinder(const Pack& pack) -> SizeT {
  if constexpr (is_indexed_binding_v<Binder>) {
    using Sym = typename Binder::symbol_type;
    if constexpr (Sym::template has_dim<DimTag>) {
      return static_cast<const Binder&>(pack).template dimSize<DimTag>();
    }
  }
  return 0;
}

}  // namespace detail

template <typename DimTag, typename... Binders>
auto getDimensionSize(const BinderPack<Binders...>& pack) -> SizeT {
  SizeT result = 0;
  // Fold expression: check each binder, keep first non-zero result
  ((result == 0 ? result = detail::getDimSizeFromBinder<DimTag, Binders>(pack) : result), ...);
  return result;
}

// ============================================================================
// IndexedFold - Custom fold that handles SumOver
// ============================================================================

namespace detail {

template <typename Interp, typename Op, typename Ctx, SizeT... Is, typename... Args>
auto indexedFoldExpression([[maybe_unused]] Expression<Op, Args...> expr,
                            Ctx& ctx, IndexSequence<Is...>) -> double {
  return Interp::combine(ctx, Op{},
      indexedFold<Interp>(expr.template arg<Is>(), ctx)...);
}

}  // namespace detail

template <typename Interp, Symbolic E, typename Ctx>
auto indexedFold(E expr, Ctx& ctx) -> double;

template <typename Interp, Symbolic E, typename Ctx>
auto indexedFold(E expr, Ctx& ctx) -> double {
  if constexpr (is_reduce_over_v<E>) {
    // Handle ReduceOver - loop over dimension with ROp::identity/combine
    using ROp = typename E::reduce_op;
    using DimTag = typename E::dim_tag;
    auto size = getDimensionSize<DimTag>(ctx.indexed);

    double accum = ROp::identity();
    for (SizeT i = 0; i < size; ++i) {
      ctx.dim_indices.template set<DimTag>(i, size);
      accum = ROp::combine(accum, indexedFold<Interp>(expr.expr_, ctx));
    }
    return accum;
  } else if constexpr (is_gather_v<E>) {
    // Handle Gather - cross-plate indexing
    // 1. Evaluate the index expression to get an integer
    auto idx_val = static_cast<SizeT>(indexedFold<Interp>(expr.index, ctx));

    // 2. Extract the param dimension from the param expression
    //    Works for both IndexedSymbol and expressions containing IndexedSymbols
    using ParamType = typename E::param_type;
    if constexpr (has_indexed_dim_v<ParamType>) {
      using ParamDim = find_indexed_dim_t<ParamType>;

      // 3. Save old dimension state
      auto old_idx = ctx.dim_indices.template get<ParamDim>();
      auto old_size = ctx.dim_indices.template getSize<ParamDim>();

      // 4. Set ParamDim to the gathered index value
      auto param_size = getDimensionSize<ParamDim>(ctx.indexed);
      ctx.dim_indices.template set<ParamDim>(idx_val, param_size);

      // 5. Evaluate param with the modified dimension
      auto result = indexedFold<Interp>(expr.param, ctx);

      // 6. Restore old dimension index
      ctx.dim_indices.template set<ParamDim>(old_idx, old_size);
      return result;
    } else {
      // For expressions that don't contain any IndexedSymbol, just evaluate directly
      // (e.g., pure scalar expressions)
      return indexedFold<Interp>(expr.param, ctx);
    }
  } else if constexpr (is_expression_v<E>) {
    return detail::indexedFoldExpression<Interp>(expr, ctx, MakeIndexSequence<E::arity>{});
  } else {
    return Interp::terminal(expr, ctx);
  }
}

// ============================================================================
// indexedFoldSingleIndex - Fold that evaluates SumOver at a single index
// ============================================================================
//
// Unlike indexedFold which sums over all indices in a SumOver, this version
// evaluates SumOver expressions at the current index (already set in context).
// This is needed for computing element-wise gradients of indexed parameters.
//
// Example: For gradient of b[i], we want:
//   d/d(b[i]) SumOver<Countries, f(b)} = d/d(b[i]) f(b[i])
// Not the sum of all derivatives.
//

namespace detail {

template <typename Interp, typename Op, typename Ctx, SizeT... Is, typename... Args>
auto indexedFoldSingleIndexExpr([[maybe_unused]] Expression<Op, Args...> expr,
                                Ctx& ctx, IndexSequence<Is...>) -> double {
  return Interp::combine(ctx, Op{},
      indexedFoldSingleIndex<Interp>(expr.template arg<Is>(), ctx)...);
}

}  // namespace detail

template <typename Interp, Symbolic E, typename Ctx>
auto indexedFoldSingleIndex(E expr, Ctx& ctx) -> double;

template <typename Interp, Symbolic E, typename Ctx>
auto indexedFoldSingleIndex(E expr, Ctx& ctx) -> double {
  if constexpr (is_reduce_over_v<E>) {
    // For single-index mode: evaluate inner expression at current index (no reduction)
    // The dimension index should already be set in ctx.dim_indices
    return indexedFoldSingleIndex<Interp>(expr.expr_, ctx);
  } else if constexpr (is_gather_v<E>) {
    // Handle Gather in single-index mode - same logic as indexedFold
    auto idx_val = static_cast<SizeT>(indexedFoldSingleIndex<Interp>(expr.index, ctx));

    using ParamType = typename E::param_type;
    if constexpr (has_indexed_dim_v<ParamType>) {
      using ParamDim = find_indexed_dim_t<ParamType>;

      auto old_idx = ctx.dim_indices.template get<ParamDim>();
      auto old_size = ctx.dim_indices.template getSize<ParamDim>();
      auto param_size = getDimensionSize<ParamDim>(ctx.indexed);
      ctx.dim_indices.template set<ParamDim>(idx_val, param_size);

      auto result = indexedFoldSingleIndex<Interp>(expr.param, ctx);

      ctx.dim_indices.template set<ParamDim>(old_idx, old_size);
      return result;
    } else {
      return indexedFoldSingleIndex<Interp>(expr.param, ctx);
    }
  } else if constexpr (is_expression_v<E>) {
    return detail::indexedFoldSingleIndexExpr<Interp>(expr, ctx, MakeIndexSequence<E::arity>{});
  } else {
    return Interp::terminal(expr, ctx);
  }
}

// ============================================================================
// Binding separation helpers
// ============================================================================

namespace detail {

template <typename... Binders>
struct SeparateBinders {
  template <typename B>
  static constexpr bool is_indexed = is_indexed_binding_v<B>;

  template <typename B>
  static auto scalarPart(const B& b) {
    if constexpr (is_indexed<B>) {
      return std::tuple<>{};
    } else {
      return std::tuple<B>{b};
    }
  }

  template <typename B>
  static auto indexedPart(const B& b) {
    if constexpr (is_indexed<B>) {
      return std::tuple<B>{b};
    } else {
      return std::tuple<>{};
    }
  }

  static auto separate(const BinderPack<Binders...>& pack) {
    auto scalars = std::tuple_cat(scalarPart(static_cast<const Binders&>(pack))...);
    auto indexed = std::tuple_cat(indexedPart(static_cast<const Binders&>(pack))...);
    return std::pair{scalars, indexed};
  }
};

}  // namespace detail

// ============================================================================
// evaluateIndexed - Main entry point
// ============================================================================
//
// The Terminals parameter selects the terminal dispatch policy.
// Default is ProbTerminals which handles Sample/IndexedSample atoms.
// Use BaseTerminals for domain-independent evaluation (no probabilistic effects).

template <typename Terminals = ProbTerminals, Symbolic E, typename... Binders>
auto evaluateIndexed(E expr, BinderPack<Binders...> bindings) -> double {
  auto [scalar_tuple, indexed_tuple] = detail::SeparateBinders<Binders...>::separate(bindings);
  auto scalars = tupleToBinderPack(scalar_tuple);
  auto indexed = tupleToBinderPack(indexed_tuple);

  using ScalarBindings = decltype(scalars);
  using IndexedBindings = decltype(indexed);
  using Interp = IndexedEval<ScalarBindings, IndexedBindings, Terminals>;

  typename Interp::context_type ctx{scalars, indexed, {}};
  return indexedFold<Interp>(expr, ctx);
}

template <typename Terminals = ProbTerminals, Symbolic E, typename... Args>
auto evaluateIndexed(E expr, Args... binders) -> double {
  return evaluateIndexed<Terminals>(expr, BinderPack{binders...});
}

// ============================================================================
// evaluateAtIndex - Evaluate expression at a specific index
// ============================================================================
//
// For HMC with indexed params, we need gradients for individual elements.
// This evaluates expressions containing SumOver at a specific index WITHOUT
// summing - each SumOver just evaluates its body at the current index.
//
// Example: For grad_expr = SumOver<D, f> + SumOver<D, g> evaluated at index i:
//   Returns f(i) + g(i), NOT Σⱼf(j) + Σⱼg(j)
//
template <typename Terminals = ProbTerminals, Symbolic E, typename... Binders>
auto evaluateAtIndex(E expr, BinderPack<Binders...> bindings, SizeT idx) -> double {
  auto [scalar_tuple, indexed_tuple] = detail::SeparateBinders<Binders...>::separate(bindings);
  auto scalars = tupleToBinderPack(scalar_tuple);
  auto indexed = tupleToBinderPack(indexed_tuple);

  using ScalarBindings = decltype(scalars);
  using IndexedBindings = decltype(indexed);
  using Interp = IndexedEval<ScalarBindings, IndexedBindings, Terminals>;

  typename Interp::context_type ctx{scalars, indexed, {}};

  // Set the index for all dimension tags that might be present
  // We need to infer the dimension tag from the expression or bindings
  // For now, we use the first indexed binding's dimension to set the index
  setDimensionIndex(ctx, indexed, idx);

  // Use single-index fold - SumOver will NOT sum, just evaluate at current index
  return indexedFoldSingleIndex<Interp>(expr, ctx);
}

// Helper to set dimension index from indexed bindings
// Uses fold expression to find first indexed binding and set its dimension index
template <typename Ctx, typename... Binders>
void setDimensionIndex(Ctx& ctx, const BinderPack<Binders...>& pack, SizeT idx) {
  bool found = false;
  auto set_from_binding = [&]<typename B>(const B& binder) {
    if constexpr (is_indexed_binding_v<B>) {
      if (!found) {
        using Sym = typename B::symbol_type;
        if constexpr (Sym::ndims >= 1) {
          using Dim0 = detail::At<0, typename Sym::dims_list>;
          SizeT size = binder.size();
          ctx.dim_indices.template set<Dim0>(idx, size);
          found = true;
        }
      }
    }
  };
  (set_from_binding(static_cast<const Binders&>(pack)), ...);
}

}  // namespace tempura::symbolic4
