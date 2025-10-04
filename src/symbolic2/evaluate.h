#pragma once

#include "binding.h"
#include "core.h"

// Expression evaluation

namespace tempura {

// --- Compile-Time Expression Evaluation ---

// Evaluation base case: A Constant evaluates to its stored value.
template <auto V, Symbolic... Tags, typename... Ts>
constexpr auto evaluate(Constant<V>,
                        const BinderPack<TypeValueBinder<Tags, Ts>...>&) {
  return V;
}

template <typename SymbolTag, typename... Tags, typename... Ts>
constexpr auto evaluate(
    Symbol<SymbolTag>,
    const BinderPack<TypeValueBinder<Tags, Ts>...>& binders) {
  // The symbol's *type* is used as the key to look up the value.
  return binders[Symbol<SymbolTag>{}];
}

template <typename Op, Symbolic... Args, typename... Tags, typename... Ts>
constexpr auto evaluate(
    Expression<Op, Args...>,
    const BinderPack<TypeValueBinder<Tags, Ts>...>& binders) {
  // Instantiate the operator tag `Op{}` and call it with the evaluated
  // arguments. `evaluate(Args{}, binders)...` expands to evaluate each
  // argument recursively.
  return Op{}(evaluate(Args{}, binders)...);
}

}  // namespace tempura
