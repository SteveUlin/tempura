#pragma once

#include "binding.h"
#include "core.h"

// Compile-time expression evaluation with symbol bindings

namespace tempura {

template <auto V, Symbolic... Tags, typename... Ts>
constexpr auto evaluate(Constant<V>,
                        const BinderPack<TypeValueBinder<Tags, Ts>...>&) {
  return V;
}

template <typename SymbolTag, typename... Tags, typename... Ts>
constexpr auto evaluate(
    Symbol<SymbolTag>,
    const BinderPack<TypeValueBinder<Tags, Ts>...>& binders) {
  return binders[Symbol<SymbolTag>{}];
}

// Recursively evaluate subexpressions then apply operator
template <typename Op, Symbolic... Args, typename... Tags, typename... Ts>
constexpr auto evaluate(
    Expression<Op, Args...>,
    const BinderPack<TypeValueBinder<Tags, Ts>...>& binders) {
  return Op{}(evaluate(Args{}, binders)...);
}

}  // namespace tempura
