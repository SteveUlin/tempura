#pragma once

#include "symbolic4/core.h"
#include "symbolic4/indexed/dim.h"
#include "symbolic4/operators.h"

// ============================================================================
// gather.h - Cross-plate indexing for hierarchical models
// ============================================================================
//
// In hierarchical models, parameters vary over one dimension (e.g., Districts)
// but need to be accessed using indices from another dimension (e.g., Observations).
//
// Example: Bangladesh contraception model
//   auto a = plate<Districts>(normal(a_bar, sigma_a));  // a[d] for d in Districts
//   auto district_idx = data<Observations>();           // which district each obs is in
//   auto y = plate<Observations>(bernoulli(logistic(
//       gather(a, district_idx) + ...  // ✓ gather(a, idx) at obs i = a[idx[i]]
//   )));
//
// Semantics:
//   gather(param, idx) where:
//   - param varies over ParamDim (e.g., Districts)
//   - idx varies over IndexDim (e.g., Observations)
//   - Result varies over IndexDim, selecting param[idx[i]] for each i
//
// During evaluation inside plate<Observations>:
//   1. Evaluate idx to get an integer value (the district index)
//   2. Temporarily set ParamDim index to that value
//   3. Evaluate param at that dimension index
//   4. Restore ParamDim index
//
// During differentiation:
//   d/dx[gather(a, idx)] = gather(d/dx[a], idx)
//   The index is data (constant), so no derivative term for idx.
//
// ============================================================================

namespace tempura::symbolic4 {

// ============================================================================
// Gather - Cross-plate indexing expression node
// ============================================================================
//
// Type parameters:
//   Param - Expression varying over ParamDim (e.g., plate<Districts> result)
//   Index - Expression varying over IndexDim (e.g., data<Observations>)
//
// The Gather node stores both expressions. During evaluation, it:
//   1. Evaluates Index to get an integer
//   2. Uses that integer to look up Param in its dimension

template <Symbolic Param, Symbolic Index>
struct Gather : SymbolicTag {
  using param_type = Param;
  using index_type = Index;

  [[no_unique_address]] Param param;
  [[no_unique_address]] Index index;

  constexpr Gather() = default;
  constexpr Gather(Param p, Index i) : param{p}, index{i} {}
};

// ============================================================================
// Type traits
// ============================================================================

template <typename T>
struct IsGather : std::false_type {};

template <typename P, typename I>
struct IsGather<Gather<P, I>> : std::true_type {};

template <typename T>
constexpr bool is_gather_v = IsGather<T>::value;

// Concept for use in if constexpr
template <typename T>
concept IsGatherType = is_gather_v<T>;

// ============================================================================
// Extract indexed dimension from any expression
// ============================================================================
//
// FindIndexedDim<E> searches expression E for any IndexedSymbol and returns
// its first dimension. This is used by Gather evaluation to determine which
// dimension the param expression varies over.

// Helper: find first indexed dimension in a type list
template <typename... Ts>
struct FindFirstIndexedDim {
  static constexpr bool found = false;
  using type = void;
};

// Forward declaration
template <typename E>
struct FindIndexedDim;

template <typename First, typename... Rest>
struct FindFirstIndexedDim<First, Rest...> {
  static constexpr bool first_found = FindIndexedDim<First>::found;
  static constexpr bool rest_found = FindFirstIndexedDim<Rest...>::found;
  static constexpr bool found = first_found || rest_found;

  using type = std::conditional_t<
      first_found,
      typename FindIndexedDim<First>::type,
      typename FindFirstIndexedDim<Rest...>::type>;
};

// Base case: any type that's not an IndexedSymbol or Expression
template <typename E>
struct FindIndexedDim {
  static constexpr bool found = false;
  using type = void;
};

// Base case: IndexedSymbol directly
template <typename Id, typename... Dims>
struct FindIndexedDim<IndexedSymbol<Id, Dims...>> {
  static constexpr bool found = true;
  using type = Head_t<TypeList<Dims...>>;
};

// Recursive case: Expression — search children left to right
template <typename Op, typename... Args>
struct FindIndexedDim<Expression<Op, Args...>> : FindFirstIndexedDim<Args...> {};

// Gather case: look inside the param
template <typename P, typename I>
struct FindIndexedDim<Gather<P, I>> : FindIndexedDim<P> {};

template <typename E>
constexpr bool has_indexed_dim_v = FindIndexedDim<E>::found;

template <typename E>
using find_indexed_dim_t = typename FindIndexedDim<E>::type;

// ============================================================================
// Extract dimension tags from Gather
// ============================================================================

// Get the param dimension from Gather
template <typename T>
struct GatherParamDim;

template <typename P, typename I>
  requires has_indexed_dim_v<P>
struct GatherParamDim<Gather<P, I>> {
  using type = find_indexed_dim_t<P>;
};

template <typename T>
using gather_param_dim_t = typename GatherParamDim<T>::type;

// Get the index dimension from Gather
template <typename T>
struct GatherIndexDim;

// When index is an IndexedSymbol, extract its first dimension
template <typename P, typename I>
  requires is_indexed_symbol_v<I>
struct GatherIndexDim<Gather<P, I>> {
  using type = Head_t<typename I::dims_list>;
};

template <typename T>
using gather_index_dim_t = typename GatherIndexDim<T>::type;

// ============================================================================
// Factory function
// ============================================================================
//
// Creates a Gather expression, converting operands to symbolic form.
// Usage:
//   auto a = plate<Districts>(normal(mu, sigma));
//   auto idx = data<Observations>();
//   auto gathered = gather(a, idx);  // varies over Observations

template <SymbolicLike Param, SymbolicLike Index>
constexpr auto gather(Param param, Index idx) {
  auto p = asSymbolic(param);
  auto i = asSymbolic(idx);
  return Gather<decltype(p), decltype(i)>{p, i};
}

// ============================================================================
// Arithmetic with Gather
// ============================================================================
//
// Gather inherits from SymbolicTag, so existing operators in operators.h
// work automatically. However, for disambiguation when both operands are
// Gather (or Gather + ReduceOver), we need explicit overloads.

// Gather op Symbolic
template <Symbolic P, Symbolic I, Symbolic E>
constexpr auto operator+(Gather<P, I> g, E other) {
  return Expression<AddOp, Gather<P, I>, E>{g, other};
}

template <Symbolic P, Symbolic I, Symbolic E>
constexpr auto operator+(E other, Gather<P, I> g) {
  return Expression<AddOp, E, Gather<P, I>>{other, g};
}

template <Symbolic P, Symbolic I, Symbolic E>
constexpr auto operator-(Gather<P, I> g, E other) {
  return Expression<SubOp, Gather<P, I>, E>{g, other};
}

template <Symbolic P, Symbolic I, Symbolic E>
constexpr auto operator-(E other, Gather<P, I> g) {
  return Expression<SubOp, E, Gather<P, I>>{other, g};
}

template <Symbolic P, Symbolic I, Symbolic E>
constexpr auto operator*(Gather<P, I> g, E other) {
  return Expression<MulOp, Gather<P, I>, E>{g, other};
}

template <Symbolic P, Symbolic I, Symbolic E>
constexpr auto operator*(E other, Gather<P, I> g) {
  return Expression<MulOp, E, Gather<P, I>>{other, g};
}

template <Symbolic P, Symbolic I, Symbolic E>
constexpr auto operator/(Gather<P, I> g, E other) {
  return Expression<DivOp, Gather<P, I>, E>{g, other};
}

template <Symbolic P, Symbolic I, Symbolic E>
constexpr auto operator/(E other, Gather<P, I> g) {
  return Expression<DivOp, E, Gather<P, I>>{other, g};
}

// Unary negation
template <Symbolic P, Symbolic I>
constexpr auto operator-(Gather<P, I> g) {
  return Expression<NegOp, Gather<P, I>>{g};
}

// Gather op Gather
template <Symbolic P1, Symbolic I1, Symbolic P2, Symbolic I2>
constexpr auto operator+(Gather<P1, I1> lhs, Gather<P2, I2> rhs) {
  return Expression<AddOp, Gather<P1, I1>, Gather<P2, I2>>{lhs, rhs};
}

template <Symbolic P1, Symbolic I1, Symbolic P2, Symbolic I2>
constexpr auto operator-(Gather<P1, I1> lhs, Gather<P2, I2> rhs) {
  return Expression<SubOp, Gather<P1, I1>, Gather<P2, I2>>{lhs, rhs};
}

template <Symbolic P1, Symbolic I1, Symbolic P2, Symbolic I2>
constexpr auto operator*(Gather<P1, I1> lhs, Gather<P2, I2> rhs) {
  return Expression<MulOp, Gather<P1, I1>, Gather<P2, I2>>{lhs, rhs};
}

template <Symbolic P1, Symbolic I1, Symbolic P2, Symbolic I2>
constexpr auto operator/(Gather<P1, I1> lhs, Gather<P2, I2> rhs) {
  return Expression<DivOp, Gather<P1, I1>, Gather<P2, I2>>{lhs, rhs};
}

}  // namespace tempura::symbolic4
