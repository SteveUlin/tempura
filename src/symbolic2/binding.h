#pragma once

#include "core.h"

// Heterogeneous symbol-to-value bindings for compile-time evaluation

namespace tempura {

// Maps a unique Symbol type to a runtime value
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

// Multiple bindings using parameter pack inheritance for type-based dispatch
// Usage: BinderPack{x = 1, y = 2.5}
template <typename... Binders>
struct BinderPack : Binders... {
  constexpr BinderPack(Binders... binders) : Binders(binders)... {}
  using Binders::operator[]...;
};
template <typename... Ts, typename... Us>
BinderPack(TypeValueBinder<Ts, Us>...)
    -> BinderPack<TypeValueBinder<Ts, Us>...>;

template <typename unique>
constexpr auto Symbol<unique>::operator=(auto value) const {
  return TypeValueBinder{Symbol{}, value};
}

}  // namespace tempura
