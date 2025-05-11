#pragma once

#include <cmath>

#include "meta/utility.h"

namespace tempura {

// Helper wrapper to hold a type without needing to build the actual type.
template <typename T>
struct Tag {
  using Type = T;
};

// Same but for a list of types
template <typename... Types>
struct TypeList {};

// Ranked Overload Resolution for template dispatching.
//
// You can defined implementation functions with the tag type as the first
// arg. The Arg witht he highest rank will be preferred.
//
// Example:
//   template <typename T>
//   void foo(Rank3, T&& arg) {...}
struct Rank0 {};
struct Rank1 : Rank0 {};
struct Rank2 : Rank1 {};
struct Rank3 : Rank2 {};
struct Rank4 : Rank3 {};
struct Rank5 : Rank4 {};
struct Rank6 : Rank5 {};

}  // namespace tempura
