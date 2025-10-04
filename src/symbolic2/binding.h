#pragma once

#include "core.h"

// Type-Value binding for evaluation and substitution

namespace tempura {

// --- Type-Value Binding (Evaluation and Substitution) ---

// Binds a specific compile-time type (Symbol) to a runtime or compile-time
// value.
template <typename TypeKey, typename ValueType>
class TypeValueBinder {
 public:
  constexpr TypeValueBinder(TypeKey /*key*/, ValueType value) : value_(value) {}

  // Retrieve the held value if you supply the TypeKey.
  // BinderPack Overloads this function for different TypeKeys.
  constexpr auto operator[](TypeKey /*key*/) const -> ValueType {
    return value_;
  }

 private:
  ValueType value_;
};

// A collection of TypeKey-Value pairs enabling lookup with
// `binder_pack[TypeKey{}]`.
template <typename... Binders>
struct BinderPack : Binders... {
  constexpr BinderPack(Binders... binders) : Binders(binders)... {}
  using Binders::operator[]...;
};
template <typename... Ts, typename... Us>
BinderPack(TypeValueBinder<Ts, Us>...)
    -> BinderPack<TypeValueBinder<Ts, Us>...>;

// Implementation of Symbol::operator= that was forward declared in core.h
template <typename unique>
constexpr auto Symbol<unique>::operator=(auto value) const {
  return TypeValueBinder{Symbol{}, value};
}

}  // namespace tempura
