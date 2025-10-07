#pragma once

#include "symbolic3/core.h"
#include "symbolic3/operators.h"

// Evaluation of symbolic expressions with concrete values
// Operators are directly callable for efficient evaluation

namespace tempura::symbolic3 {

// ============================================================================
// BINDING SYSTEM
// ============================================================================
// Heterogeneous symbol-to-value bindings for evaluation
// Usage: evaluate(expr, BinderPack{x = 1, y = 2.5, z = 3.14})

// Maps a unique Symbol type to a runtime value
template <typename SymbolType, typename ValueType>
class TypeValueBinder {
 public:
  constexpr TypeValueBinder(SymbolType /*symbol*/, ValueType value)
      : value_(value) {}

  constexpr auto operator[](SymbolType /*symbol*/) const -> ValueType {
    return value_;
  }

 private:
  ValueType value_;
};

// Multiple bindings using parameter pack inheritance for type-based dispatch
template <typename... Binders>
struct BinderPack : Binders... {
  constexpr BinderPack(Binders... binders) : Binders(binders)... {}
  using Binders::operator[]...;
};

// Deduction guide
template <typename... Symbols, typename... Values>
BinderPack(TypeValueBinder<Symbols, Values>...)
    -> BinderPack<TypeValueBinder<Symbols, Values>...>;

// Enable assignment syntax: x = value
template <typename unique>
constexpr auto Symbol<unique>::operator=(auto value) const {
  return TypeValueBinder{Symbol<unique>{}, value};
}

// ============================================================================
// EVALUATION FUNCTIONS
// ============================================================================

// Constant evaluation - just return the value
template <auto V, typename... Binders>
constexpr auto evaluate(Constant<V>, const BinderPack<Binders...>&) {
  return V;
}

// Symbol evaluation - lookup in binder pack
template <typename unique, typename... Binders>
constexpr auto evaluate(Symbol<unique>, const BinderPack<Binders...>& binders) {
  return binders[Symbol<unique>{}];
}

// Compound expression evaluation - recursively evaluate subexpressions
// Then apply the operator directly (operators are callable in symbolic3)
template <typename Op, Symbolic... Args, typename... Binders>
constexpr auto evaluate(Expression<Op, Args...>,
                        const BinderPack<Binders...>& binders) {
  return Op{}(evaluate(Args{}, binders)...);
}

}  // namespace tempura::symbolic3
