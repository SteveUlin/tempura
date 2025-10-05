#pragma once

#include "core.h"

// Type-Value binding for evaluation and substitution

namespace tempura {

// --- Type-Value Binding ---

// Associates a Symbol type with a value for evaluation
// Enables type-safe heterogeneous symbol-to-value mappings
template <typename TypeKey, typename ValueType>
class TypeValueBinder {
 public:
  constexpr TypeValueBinder(TypeKey /*key*/, ValueType value) : value_(value) {}

  constexpr auto operator[](TypeKey /*key*/) const -> ValueType {
    return value_;
  }

 private:
  ValueType value_;
};

// Heterogeneous compile-time map from Symbol types to values
// Usage: BinderPack{x = 1, y = 2.5, z = "text"}
template <typename... Binders>
struct BinderPack : Binders... {
  constexpr BinderPack(Binders... binders) : Binders(binders)... {}
  using Binders::operator[]...;  // Expose all lookup operators
};
template <typename... Ts, typename... Us>
BinderPack(TypeValueBinder<Ts, Us>...)
    -> BinderPack<TypeValueBinder<Ts, Us>...>;

// Symbol::operator= implementation - creates binder from symbol and value
template <typename unique>
constexpr auto Symbol<unique>::operator=(auto value) const {
  return TypeValueBinder{Symbol{}, value};
}

}  // namespace tempura
