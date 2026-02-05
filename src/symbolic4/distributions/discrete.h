#pragma once

#include <array>
#include <concepts>
#include <cstddef>
#include <tuple>
#include <type_traits>
#include <utility>

#include "prob/discrete.h"
#include "symbolic4/core.h"
#include "symbolic4/operators.h"

// ============================================================================
// discrete.h - Symbolic discrete distributions over enum types
// ============================================================================
//
// Provides symbolic support for discrete distributions:
// - DiscreteSymbol<Id, Enum> - Symbolic variable taking enum values
// - DiscreteDist<Enum, Probs...> - Distribution with symbolic probabilities
// - Select<Index, Values...> - Symbolic indexing into a set of values
//
// Usage:
//   enum class Choice { A, B, C, kCount };
//
//   // Symbolic probabilities
//   Symbol<struct P_A> p_a;
//   Symbol<struct P_B> p_b;
//   Symbol<struct P_C> p_c;
//
//   // Discrete distribution
//   auto dist = DiscreteDist<Choice>(p_a, p_b, p_c);
//
//   // Symbolic discrete variable
//   DiscreteSymbol<struct X, Choice> x;
//
//   // Log-probability: uses Select to pick the right parameter
//   auto lp = dist.logProbFor(x);
//
// ============================================================================

namespace tempura::symbolic4 {

// Forward declarations
template <typename Sym>
struct DiscreteBinding;

// ============================================================================
// DiscreteSymbol - Symbolic variable for discrete/enum values
// ============================================================================

template <typename Id, prob::DiscreteEnum E>
struct DiscreteSymbol : SymbolicTag {
  using id_type = Id;
  using enum_type = E;
  static constexpr std::size_t num_values = prob::EnumTraits<E>::count;

  // Bind to a specific enum value
  constexpr auto operator=(E value) const {
    return DiscreteBinding<DiscreteSymbol<Id, E>>{value};
  }
};

// Binding for discrete symbols
template <typename Sym>
struct DiscreteBinding {
  using symbol_type = Sym;
  using enum_type = typename Sym::enum_type;

  enum_type value;

  constexpr auto index() const -> std::size_t {
    return prob::EnumTraits<enum_type>::toIndex(value);
  }
};

// Type traits — DiscreteSymbol has NTTP (enum), use info-based isSpecOf
template <typename T>
constexpr bool is_discrete_symbol_v = core_traits_detail::isSpecOf<T>(^^DiscreteSymbol);

template <typename T>
constexpr bool is_discrete_binding_v = core_traits_detail::isSpecOf<T, DiscreteBinding>();

// ============================================================================
// Select - Symbolic expression that selects one of N values based on index
// ============================================================================
//
// Select<IndexExpr, V0, V1, V2, ...> evaluates to V[index] where index
// is the value of IndexExpr. Used to represent lookups into parameter vectors.

template <typename IndexExpr, Symbolic... Values>
struct Select : SymbolicTag {
  static constexpr std::size_t num_values = sizeof...(Values);

  [[no_unique_address]] IndexExpr index_;
  std::tuple<Values...> values_;

  constexpr Select(IndexExpr idx, Values... vals)
      : index_{idx}, values_{vals...} {}

  constexpr auto index() const { return index_; }

  template <std::size_t I>
  constexpr auto value() const {
    return std::get<I>(values_);
  }
};

// Factory function
template <typename IndexExpr, Symbolic... Values>
constexpr auto select(IndexExpr idx, Values... vals) {
  return Select<IndexExpr, Values...>{idx, vals...};
}

// Type trait
template <typename T>
constexpr bool is_select_v = core_traits_detail::isSpecOf<T, Select>();

// ============================================================================
// DiscreteDist - Symbolic discrete distribution
// ============================================================================
//
// Represents P(X=k) = p_k where p_k are symbolic probabilities.

template <prob::DiscreteEnum E, Symbolic... Probs>
  requires(sizeof...(Probs) == prob::EnumTraits<E>::count)
struct DiscreteDist {
  using enum_type = E;
  static constexpr std::size_t K = sizeof...(Probs);

  std::tuple<Probs...> probs_;

  constexpr DiscreteDist(Probs... ps) : probs_{ps...} {}

  // Get probability for enum value k
  template <std::size_t I>
  constexpr auto probAt() const {
    return std::get<I>(probs_);
  }

  // Log-probability for a symbolic discrete variable
  // Returns Select(x, log(p_0), log(p_1), ..., log(p_{K-1}))
  template <typename Id>
  constexpr auto logProbFor(DiscreteSymbol<Id, E> x) const {
    return makeLogProbSelect(x, std::make_index_sequence<K>{});
  }

  // Unnormalized is the same for discrete (no normalizing constant)
  template <typename Id>
  constexpr auto unnormalizedLogProbFor(DiscreteSymbol<Id, E> x) const {
    return logProbFor(x);
  }

 private:
  template <typename Id, std::size_t... Is>
  constexpr auto makeLogProbSelect(DiscreteSymbol<Id, E> x, std::index_sequence<Is...>) const {
    return select(x, log(std::get<Is>(probs_))...);
  }
};

// ============================================================================
// Factory functions
// ============================================================================

// Variadic constructor with symbolic types
template <prob::DiscreteEnum E, Symbolic... Probs>
  requires(sizeof...(Probs) == prob::EnumTraits<E>::count)
constexpr auto makeDiscreteDist(Probs... ps) {
  return DiscreteDist<E, Probs...>{ps...};
}

// Convenience: create from Constant values
// Use _c suffix: discreteDist<E>(0.3_c, 0.7_c)
template <prob::DiscreteEnum E, Symbolic... Ts>
  requires(sizeof...(Ts) == prob::EnumTraits<E>::count)
constexpr auto discreteDist(Ts... ps) {
  return DiscreteDist<E, Ts...>{ps...};
}

// Note: runtime double arrays can no longer create symbolic distributions.
// Use Constant<V> or Symbol + binding for runtime data.

// ============================================================================
// Evaluation support for Select and DiscreteSymbol
// ============================================================================

// Helper to evaluate at a specific index
namespace detail {

template <typename Sel, typename Bindings, std::size_t... Is>
auto evaluateSelectImpl(const Sel& sel, std::size_t idx, const Bindings& bindings,
                        std::index_sequence<Is...>) -> double {
  // Build array of evaluated values
  std::array<double, sizeof...(Is)> values = {
      evaluate(sel.template value<Is>(), bindings)...
  };
  return values[idx];
}

}  // namespace detail

// Evaluate Select expression when index is a DiscreteSymbol
template <typename Id, prob::DiscreteEnum E, Symbolic... Values, typename Bindings>
auto evaluateSelect(const Select<DiscreteSymbol<Id, E>, Values...>& sel,
                    const Bindings& bindings) -> double {
  // Get the binding for this discrete symbol
  const auto& binding = bindings[sel.index()];
  std::size_t idx = binding.index();

  return detail::evaluateSelectImpl(sel, idx, bindings,
                                    std::make_index_sequence<sizeof...(Values)>{});
}

// ============================================================================
// OneHot indicator for gradient computation
// ============================================================================
//
// For gradient computation, we sometimes need indicator expressions:
// OneHot(x, k) = 1 if x == k, else 0

template <std::size_t K>
struct OneHotIndicator {
  std::size_t target_index;
  std::size_t actual_index;

  constexpr auto value() const -> double {
    return (target_index == actual_index) ? 1.0 : 0.0;
  }
};

}  // namespace tempura::symbolic4
