#pragma once

#include <meta>

#include "meta/tags.h"
#include "meta/utility.h"

// Compile-time unique type IDs via P2996 reflection.
// ^^T gives native type identity — no stateful friend injection needed.

namespace tempura {

using TypeId = std::meta::info;

template <typename T>
constexpr TypeId kMeta = ^^T;

template <typename T>
constexpr auto getTypeId(T) {
  return kMeta<T>;
}

// Reverse mapping: TypeId → Type (splice the reflection back to a type)
template <TypeId value>
using TypeOf = [:value:];

}  // namespace tempura
