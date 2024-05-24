#pragma once

#include <string_view>

// Returns the type name of T
template <typename T>
constexpr auto typeName() -> std::string_view {
  // Works with clang 18, your mileage may vary
  std::string_view function_name = __PRETTY_FUNCTION__;
  auto begin = function_name.find("[T = ") + 5;
  auto end = function_name.find(']');
  return function_name.substr(begin, end - begin);
}

template <typename T>
constexpr auto typeName(T) -> std::string_view {
  return typeName<T>();
}

