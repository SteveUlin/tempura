#pragma once

#include <unordered_map>
#include <typeindex>

#include "symbolic4/constraints.h"
#include "symbolic4/core.h"
#include "symbolic4/distributions/indexed_node.h"  // For make_indexed_symbol_t, IndexedRandomVar
#include "symbolic4/indexed/data.h"
#include "symbolic4/indexed/dim.h"
#include "symbolic4/indexed/sum_over.h"
#include "symbolic4/operators.h"

// ============================================================================
// indexed_eval.h - Evaluation for multi-dimensional indexed expressions
// ============================================================================
//
// Extends evaluate() to handle:
//   - IndexedSymbol with multiple dimensions
//   - Nested SumOver expressions
//   - Broadcasting (1D symbol used in 2D context)
//
// Key design: Context tracks current index for each dimension tag using
// type-erased storage. When evaluating IndexedSymbol<Id, D1, D2>, we look up
// the current index for D1 and D2 from the context.
//
// ============================================================================

namespace tempura::symbolic4 {

// ============================================================================
// Atom Id matching - allows Atom<Id, Free> to bind Atom<Id, Sample<D>>
// ============================================================================
// When evaluating expressions from distributions, we see Atom<Id, Sample<D>>
// but bindings are made with Atom<Id, Free>. They share the same Id so should
// match for evaluation purposes.

namespace indexed_eval_detail {

// Get the symbol type from a binder
template <typename Binder>
struct BinderSymbol;

template <typename S, typename V>
struct BinderSymbol<TypeValueBinder<S, V>> {
  using type = S;
};

// Check if a binder's symbol matches the given atom by Id (uses same_atom_id_v from core.h)
template <typename AtomT, typename Binder>
struct BinderMatchesAtom : std::false_type {};

template <typename Id, typename E1, typename E2, typename V>
struct BinderMatchesAtom<Atom<Id, E1>, TypeValueBinder<Atom<Id, E2>, V>> : std::true_type {};

// Look up value in BinderPack by matching atom Id (not exact type)
template <typename T, typename... Binders>
auto lookupByAtomId([[maybe_unused]] T term, const BinderPack<Binders...>& pack) -> double {
  double result = 0.0;
  bool found = false;

  auto try_lookup = [&]<typename B>(const B& binder) {
    if constexpr (BinderMatchesAtom<T, B>::value) {
      if (!found) {
        using BinderSym = typename BinderSymbol<B>::type;
        result = binder[BinderSym{}];
        found = true;
      }
    }
  };

  (try_lookup(static_cast<const Binders&>(pack)), ...);

  return result;
}

}  // namespace indexed_eval_detail

// ============================================================================
// DimIndex - Type-erased dimension index storage
// ============================================================================

class DimIndexMap {
  std::unordered_map<std::type_index, SizeT> indices_;
  std::unordered_map<std::type_index, SizeT> sizes_;

 public:
  template <typename DimTag>
  void set(SizeT index, SizeT size) {
    indices_[std::type_index(typeid(DimTag))] = index;
    sizes_[std::type_index(typeid(DimTag))] = size;
  }

  template <typename DimTag>
  SizeT get() const {
    auto it = indices_.find(std::type_index(typeid(DimTag)));
    return it != indices_.end() ? it->second : 0;
  }

  template <typename DimTag>
  SizeT getSize() const {
    auto it = sizes_.find(std::type_index(typeid(DimTag)));
    return it != sizes_.end() ? it->second : 0;
  }

  template <typename DimTag>
  bool has() const {
    return indices_.find(std::type_index(typeid(DimTag))) != indices_.end();
  }
};

// ============================================================================
// IndexedEval - Interpreter for expressions with indexed bindings
// ============================================================================

template <typename ScalarBindings, typename IndexedBindings>
struct IndexedEval {
  using result_type = double;

  struct context_type {
    ScalarBindings scalars;
    IndexedBindings indexed;
    DimIndexMap dim_indices;
  };

  // Terminal handling - dispatch based on terminal type
  template <typename T>
  static auto terminal(T term, context_type& ctx) -> double {
    if constexpr (is_constant_v<T>) {
      return static_cast<double>(T::value);
    } else if constexpr (is_fraction_v<T>) {
      return T::value;
    } else if constexpr (is_literal_v<T>) {
      return static_cast<double>(term.effect_.value);
    } else if constexpr (is_indexed_symbol_v<T>) {
      // Look up indexed binding and extract value at current indices
      return lookupIndexedSymbol<T>(term, ctx);
    } else if constexpr (is_indexed_data_v<T>) {
      // IndexedData - convert to its symbol and look up in indexed bindings
      using SymType = typename T::symbol_type;
      return lookupIndexedSymbol<SymType>(SymType{}, ctx);
    } else if constexpr (is_indexed_random_var_atom_v<T>) {
      // Atom<Id, IndexedSample<Dist, DimsList>> - look up via IndexedSymbol
      using DimsList = typename T::effect_type::dims_list;
      using IdType = typename T::id_type;
      using LookupSym = detail::make_indexed_symbol_t<IdType, DimsList>;
      return lookupIndexedSymbol<LookupSym>(LookupSym{}, ctx);
    } else if constexpr (is_random_var_atom_v<T>) {
      // Sample atom: Atom<Id, Sample<Dist>>
      // Look up by Free symbol (Atom<Id, Free>) and apply constraint transform
      double z = indexed_eval_detail::lookupByAtomId(term, ctx.scalars);

      // Apply constraint transform based on distribution support
      using Dist = get_distribution_t<typename T::effect_type>;
      using Support = typename Dist::support_type;
      return constraints::applyNumeric<Support>(z);
    } else if constexpr (is_atom_v<T>) {
      // Regular Free symbol - look up in scalar bindings (no transform)
      return indexed_eval_detail::lookupByAtomId(term, ctx.scalars);
    } else {
      static_assert(is_atom_v<T>, "Unknown terminal type");
      return 0.0;
    }
  }

  // Combine child results using the operator functor
  template <typename Op, typename... Rs>
  static constexpr auto combine(context_type&, Op op, Rs... children) -> double {
    return op(children...);
  }

 private:
  // Look up an IndexedSymbol value using current dimension indices.
  // Pack-expansion calls binding.at(idx0, idx1, ...) for any ndims.
  template <typename Sym>
  static auto lookupIndexedSymbol([[maybe_unused]] Sym sym, context_type& ctx) -> double {
    const auto& binding = ctx.indexed[sym];
    return lookupAtIndices<Sym>(binding, ctx.dim_indices,
                                std::make_index_sequence<Sym::ndims>{});
  }

  template <typename Sym, typename Binding, SizeT... Is>
  static auto lookupAtIndices(const Binding& binding, const DimIndexMap& dim_indices,
                              std::index_sequence<Is...>) -> double {
    return binding.at(dim_indices.template get<detail::At<Is, typename Sym::dims_list>>()...);
  }
};

// ============================================================================
// Get dimension size from bindings
// ============================================================================

template <typename DimTag, typename Bindings>
struct GetDimensionSizeImpl;

template <typename DimTag>
struct GetDimensionSizeImpl<DimTag, BinderPack<>> {
  static auto get(const BinderPack<>&) -> SizeT { return 0; }
};

template <typename DimTag, typename First, typename... Rest>
struct GetDimensionSizeImpl<DimTag, BinderPack<First, Rest...>> {
  static auto get(const BinderPack<First, Rest...>& pack) -> SizeT {
    if constexpr (is_indexed_binding_v<First>) {
      using Sym = typename First::symbol_type;
      if constexpr (Sym::template has_dim<DimTag>) {
        return static_cast<const First&>(pack).template dimSize<DimTag>();
      } else {
        return GetDimensionSizeImpl<DimTag, BinderPack<Rest...>>::get(
            static_cast<const BinderPack<Rest...>&>(pack));
      }
    } else {
      return GetDimensionSizeImpl<DimTag, BinderPack<Rest...>>::get(
          static_cast<const BinderPack<Rest...>&>(pack));
    }
  }
};

template <typename DimTag, typename Bindings>
auto getDimensionSize(const Bindings& bindings) -> SizeT {
  return GetDimensionSizeImpl<DimTag, Bindings>::get(bindings);
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

template <typename... Ts>
auto tupleToBinderPack(std::tuple<Ts...> t) {
  return std::apply([](auto... bs) { return BinderPack{bs...}; }, t);
}

// Handle empty tuple -> empty BinderPack
inline auto tupleToBinderPack(std::tuple<>) { return BinderPack<>{}; }

}  // namespace detail

// ============================================================================
// evaluateIndexed - Main entry point
// ============================================================================

template <Symbolic E, typename... Binders>
auto evaluateIndexed(E expr, BinderPack<Binders...> bindings) -> double {
  auto [scalar_tuple, indexed_tuple] = detail::SeparateBinders<Binders...>::separate(bindings);
  auto scalars = detail::tupleToBinderPack(scalar_tuple);
  auto indexed = detail::tupleToBinderPack(indexed_tuple);

  using ScalarBindings = decltype(scalars);
  using IndexedBindings = decltype(indexed);
  using Interp = IndexedEval<ScalarBindings, IndexedBindings>;

  typename Interp::context_type ctx{scalars, indexed, {}};
  return indexedFold<Interp>(expr, ctx);
}

template <Symbolic E, typename... Args>
auto evaluateIndexed(E expr, Args... binders) -> double {
  return evaluateIndexed(expr, BinderPack{binders...});
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
template <Symbolic E, typename... Binders>
auto evaluateAtIndex(E expr, BinderPack<Binders...> bindings, SizeT idx) -> double {
  auto [scalar_tuple, indexed_tuple] = detail::SeparateBinders<Binders...>::separate(bindings);
  auto scalars = detail::tupleToBinderPack(scalar_tuple);
  auto indexed = detail::tupleToBinderPack(indexed_tuple);

  using ScalarBindings = decltype(scalars);
  using IndexedBindings = decltype(indexed);
  using Interp = IndexedEval<ScalarBindings, IndexedBindings>;

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
