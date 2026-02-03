#pragma once

#include <cmath>
#include <unordered_map>
#include <typeindex>

#include "symbolic4/core.h"
#include "symbolic4/distributions/wrappers.h"  // For support type traits
#include "symbolic4/indexed/data.h"
#include "symbolic4/indexed/dim.h"
#include "symbolic4/indexed/sum_over.h"
#include "symbolic4/operators.h"
#include "symbolic4/scheme/fold.h"

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

// Apply constraint transform based on support type (same as eval.h)
template <typename Support>
constexpr auto applyConstraint(double z) -> double {
  if constexpr (is_positive_support_v<Support>) {
    return std::exp(z);  // Positive: x = exp(z)
  } else if constexpr (is_unit_support_v<Support>) {
    return 1.0 / (1.0 + std::exp(-z));  // Unit: x = sigmoid(z)
  } else {
    return z;  // Real: x = z (no transform)
  }
}

// Check if two atoms have the same Id (regardless of effect type)
template <typename Atom, typename Binding>
struct HasSameAtomId : std::false_type {};

template <typename Id, typename E1, typename E2>
struct HasSameAtomId<Atom<Id, E1>, TypeValueBinder<Atom<Id, E2>, double>> : std::true_type {};

// Get the symbol type from a binder
template <typename Binder>
struct BinderSymbol;

template <typename S, typename V>
struct BinderSymbol<TypeValueBinder<S, V>> {
  using type = S;
};

// Look up value in BinderPack by matching atom Id (not exact type)
template <typename T, typename... Binders>
auto lookupByAtomId([[maybe_unused]] T term, const BinderPack<Binders...>& pack) -> double {
  double result = 0.0;
  bool found = false;

  auto try_lookup = [&]<typename B>(const B& binder) {
    if constexpr (HasSameAtomId<T, B>::value) {
      if (!found) {
        // Use the binder's symbol type to call operator[]
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
    } else if constexpr (is_random_var_atom_v<T>) {
      // Sample atom: Atom<Id, Sample<Dist>>
      // Look up by Free symbol (Atom<Id, Free>) and apply constraint transform
      double z = indexed_eval_detail::lookupByAtomId(term, ctx.scalars);

      // Apply constraint transform based on distribution support
      using Dist = get_distribution_t<typename T::effect_type>;
      using Support = typename Dist::support_type;
      return indexed_eval_detail::applyConstraint<Support>(z);
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
  // Look up an IndexedSymbol value using current dimension indices
  template <typename Sym>
  static auto lookupIndexedSymbol([[maybe_unused]] Sym sym, context_type& ctx) -> double {
    const auto& binding = ctx.indexed[sym];

    if constexpr (Sym::ndims == 1) {
      // Single dimension - use the first (only) dimension's index
      using Dim0 = detail::At<0, typename Sym::dims_list>;
      SizeT i = ctx.dim_indices.template get<Dim0>();
      return binding.at(i);
    } else if constexpr (Sym::ndims == 2) {
      // Two dimensions
      using Dim0 = detail::At<0, typename Sym::dims_list>;
      using Dim1 = detail::At<1, typename Sym::dims_list>;
      SizeT i = ctx.dim_indices.template get<Dim0>();
      SizeT j = ctx.dim_indices.template get<Dim1>();
      return binding.at(i, j);
    } else if constexpr (Sym::ndims == 3) {
      using Dim0 = detail::At<0, typename Sym::dims_list>;
      using Dim1 = detail::At<1, typename Sym::dims_list>;
      using Dim2 = detail::At<2, typename Sym::dims_list>;
      SizeT i = ctx.dim_indices.template get<Dim0>();
      SizeT j = ctx.dim_indices.template get<Dim1>();
      SizeT k = ctx.dim_indices.template get<Dim2>();
      return binding.at(i, j, k);
    } else {
      // Generic N-D case
      std::array<SizeT, Sym::ndims> indices;
      fillIndices<Sym, 0>(indices, ctx.dim_indices);
      return binding.at(indices);
    }
  }

  template <typename Sym, SizeT I, SizeT N = Sym::ndims>
  static void fillIndices(std::array<SizeT, N>& arr, const DimIndexMap& dim_indices) {
    if constexpr (I < N) {
      using DimI = detail::At<I, typename Sym::dims_list>;
      arr[I] = dim_indices.template get<DimI>();
      fillIndices<Sym, I + 1>(arr, dim_indices);
    }
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
  if constexpr (is_sum_over_v<E>) {
    // Handle SumOver - loop over dimension and sum
    using DimTag = typename E::dim_tag;
    auto size = getDimensionSize<DimTag>(ctx.indexed);

    double total = 0.0;
    for (SizeT i = 0; i < size; ++i) {
      ctx.dim_indices.template set<DimTag>(i, size);
      total += indexedFold<Interp>(expr.expr_, ctx);
    }
    return total;
  } else if constexpr (is_expression_v<E>) {
    return detail::indexedFoldExpression<Interp>(expr, ctx, MakeIndexSequence<E::arity>{});
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

}  // namespace tempura::symbolic4
