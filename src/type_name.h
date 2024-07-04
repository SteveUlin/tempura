#pragma once

#include <string_view>

// Returns the type name of T
template <typename T>
constexpr auto typeName() -> std::string_view {
  // Works with gcc, your mileage may vary
  std::string_view sv = __PRETTY_FUNCTION__;
  auto begin = sv.find("with T = ") + 9;
  auto end = sv.rfind(';');
  return sv.substr(begin, end - begin);
}

template <typename T>
constexpr auto typeName(T) -> std::string_view {
  return typeName<T>();
}

