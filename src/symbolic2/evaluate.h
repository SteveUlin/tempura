#pragma once

#include "binding.h"
#include "core.h"

// Expression evaluation

namespace tempura {

// --- Compile-Time Expression Evaluation ---

// Constant evaluates to its embedded value
template <auto V, Symbolic... Tags, typename... Ts>
constexpr auto evaluate(Constant<V>,
                        const BinderPack<TypeValueBinder<Tags, Ts>...>&) {
  return V;
}

// Symbol lookup via type-based dispatch in BinderPack
template <typename SymbolTag, typename... Tags, typename... Ts>
constexpr auto evaluate(
    Symbol<SymbolTag>,
    const BinderPack<TypeValueBinder<Tags, Ts>...>& binders) {
  return binders[Symbol<SymbolTag>{}];
}

// Recursive evaluation: apply operator to evaluated arguments
template <typename Op, Symbolic... Args, typename... Tags, typename... Ts>
constexpr auto evaluate(
    Expression<Op, Args...>,
    const BinderPack<TypeValueBinder<Tags, Ts>...>& binders) {
  return Op{}(evaluate(Args{}, binders)...);
}

}  // namespace tempura
