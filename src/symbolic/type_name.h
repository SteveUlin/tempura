#pragma once

#include <string_view>

// Returns the type name of T
// https://stackoverflow.com/questions/64794649/is-there-a-way-to-consistently-sort-type-template-parameters
template <typename T>
constexpr auto typeName() noexcept {
  std::string_view name = "Error: unsupported compiler", prefix, suffix;
#ifdef __clang__
  name = __PRETTY_FUNCTION__;
  prefix = "auto typeName() [T = ";
  suffix = "]";
#elif defined(__GNUC__)
  name = __PRETTY_FUNCTION__;
  prefix = "constexpr auto typeName() [with T = ";
  suffix = "]";
#elif defined(_MSC_VER)
  name = __FUNCSIG__;
  prefix = "auto __cdecl typeName<";
  suffix = ">(void) noexcept";
#else
  static_assert(false, "Unsupported compiler!");
#endif
  name.remove_prefix(prefix.size());
  name.remove_suffix(suffix.size());
  return name;
}

template <typename T>
constexpr auto typeName(T /*unused*/) -> std::string_view {
  return typeName<T>();
}
