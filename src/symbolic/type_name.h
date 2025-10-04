#pragma once

#include <string_view>

namespace tempura::symbolic {

// Extract the type name of T at compile time using compiler intrinsics.
// Reference: https://stackoverflow.com/questions/64794649
template <typename T>
constexpr auto typeName() noexcept -> std::string_view {
  std::string_view name;
#ifdef __clang__
  name = __PRETTY_FUNCTION__;
  constexpr std::string_view prefix =
      "auto tempura::symbolic::typeName() [T = ";
  constexpr std::string_view suffix = "]";
#elif defined(__GNUC__)
  name = __PRETTY_FUNCTION__;
  constexpr std::string_view prefix =
      "constexpr std::string_view tempura::symbolic::typeName() [with T = ";
  constexpr std::string_view suffix =
      "; std::string_view = std::basic_string_view<char>]";
#elif defined(_MSC_VER)
  name = __FUNCSIG__;
  constexpr std::string_view prefix =
      "auto __cdecl tempura::symbolic::typeName<";
  constexpr std::string_view suffix = ">(void) noexcept";
#else
  static_assert(false, "Unsupported compiler!");
#endif
  name.remove_prefix(prefix.size());
  name.remove_suffix(suffix.size());
  return name;
}

// Overload to deduce type from value.
template <typename T>
constexpr auto typeName(T /*unused*/) -> std::string_view {
  return typeName<T>();
}

}  // namespace tempura::symbolic
