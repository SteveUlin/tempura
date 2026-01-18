#pragma once

#include <concepts>

#include "symbolic3/core.h"

namespace tempura::prob {

// Re-export the Symbolic concept for convenience
using symbolic3::Symbolic;

// Check if a type is a concrete floating-point number
template <typename T>
concept ConcreteFloat = std::floating_point<T>;

// Check if a type is a concrete integral number
template <typename T>
concept ConcreteInt = std::integral<T>;

// Check if a type is a concrete numeric type (not symbolic)
template <typename T>
concept Concrete = ConcreteFloat<T> || ConcreteInt<T>;

}  // namespace tempura::prob
