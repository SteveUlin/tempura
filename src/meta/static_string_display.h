#pragma once

#include "meta/function_objects.h"

// ============================================================================
// COMPILE-TIME STRING DISPLAY UTILITIES
// ============================================================================
//
// This header provides tools for displaying StaticString contents at compile-time
// via compiler errors. Useful for debugging and development with clangd/LSP.
//
// USAGE:
//   ShowStaticString<"debug message"_cts> show;        // Shows string in error
//   show_string_in_error<"another message"_cts>();     // Alternative approach
//   SHOW_STATIC_STRING(my_string_variable);            // Macro wrapper
//
// These utilities force the compiler to display StaticString contents in error
// messages, making them visible in IDEs with clangd/LSP support.

namespace tempura {

// =============================================================================
// METHOD 1: Undefined template - Shows string in instantiation error
// =============================================================================
// Usage: ShowStaticString<"debug message"_cts> show;
//        ShowStaticString<toString(expr)> show;
// The error message will display the string content directly in the template
// instantiation backtrace.
template <StaticString S>
struct ShowStaticString;  // Intentionally undefined

// =============================================================================
// METHOD 2: Static assert with custom message
// =============================================================================
// Usage: show_string_in_error<"debug message"_cts>()
//        show_string_in_error<toString(expr)>()
// This approach uses a static_assert that always fails, showing the string
// in the template parameter.
template <StaticString S>
consteval void show_string_in_error() {
  static_assert(
      S.size() == static_cast<size_t>(-1),  // Always false
      "String content shown in template parameter S - check error message");
}

// =============================================================================
// METHOD 3: Character-by-character display
// =============================================================================
// Useful for debugging encoding issues or inspecting individual characters

template <StaticString S, size_t Idx = 0>
struct StringChars {
  static constexpr char value = S.chars_[Idx];
  using next = StringChars<S, Idx + 1>;
};

template <StaticString S>
struct StringChars<S, S.size()> {
  static constexpr char value = '\0';
};

// Helper to force display of character sequence
template <StaticString S>
struct ShowStringChars;  // Undefined - will show in error

// =============================================================================
// METHOD 4: Character at index
// =============================================================================
// Display a specific character from a StaticString

template <StaticString S, size_t Idx>
consteval char char_at() {
  static_assert(Idx < S.size(), "Index out of bounds");
  return S.chars_[Idx];
}

template <StaticString S, size_t Idx>
struct ShowCharAt;  // Undefined

// =============================================================================
// CONVENIENCE MACROS
// =============================================================================

// Convenience macro for showing StaticString content in compiler errors
// Usage: SHOW_STATIC_STRING(toString(expr))
//        SHOW_STATIC_STRING("debug message"_cts)
// The error will display the actual string content
#define SHOW_STATIC_STRING(str_expr) \
  do { \
    constexpr auto _debug_str_ = (str_expr); \
    ::tempura::ShowStaticString<_debug_str_> _show_; \
  } while (0)

// Alternative macro using the function approach
#define SHOW_STRING_ERROR(str_expr) \
  do { \
    constexpr auto _debug_str_ = (str_expr); \
    ::tempura::show_string_in_error<_debug_str_>(); \
  } while (0)

// Macro for showing a character at a specific index
#define SHOW_CHAR_AT(str_expr, idx) \
  do { \
    constexpr auto _debug_str_ = (str_expr); \
    ::tempura::ShowCharAt<_debug_str_, idx> _show_char_; \
  } while (0)

}  // namespace tempura
