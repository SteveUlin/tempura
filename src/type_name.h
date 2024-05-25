#pragma once

#include <string_view>

// Returns the type name of T
template <typename T>
constexpr auto typeName() -> std::string_view {
  // Works with clang 18, your mileage may vary
  return __PRETTY_FUNCTION__;
}

template <typename T>
constexpr auto typeName(T) -> std::string_view {
  return typeName<T>();
}

