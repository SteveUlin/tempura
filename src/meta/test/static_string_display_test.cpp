// Testing compile-time string display utilities for StaticString
// These techniques force clangd to show string contents in error messages

#include "meta/static_string_display.h"

#include "meta/function_objects.h"
#include "unit.h"

using tempura::show_string_in_error;
using tempura::ShowStaticString;
using tempura::ShowStringChars;
using tempura::StaticString;
using tempura::StringChars;
using tempura::operator""_cts;
using tempura::operator""_test;

// =============================================================================
// TEST CASES - Uncomment to see errors in clangd
// =============================================================================

auto main() -> int {
  "Display constant string"_test = [] {
    constexpr auto str = "Hello, World!"_cts;

    // Uncomment to see error:
    // ShowStaticString<str> display;
    // show_string_in_error<str>();

    static_assert(str == "Hello, World!"_cts);
  };

  "Display with macro"_test = [] {
    constexpr auto msg = "Debug message"_cts;

    // Uncomment to see error via macro:
    // SHOW_STATIC_STRING(msg);

    static_assert(msg == "Debug message"_cts);
  };

  "Display concatenated strings"_test = [] {
    constexpr auto part1 = "Hello"_cts;
    constexpr auto part2 = " "_cts;
    constexpr auto part3 = "World"_cts;
    constexpr auto combined = part1 + part2 + part3;

    // Uncomment to see combined string:
    // ShowStaticString<combined> display;

    static_assert(combined == "Hello World"_cts);
  };

  "Display with forced character sequence"_test = [] {
    constexpr auto msg = "Test"_cts;

    // Uncomment to see character-by-character breakdown:
    // ShowStringChars<msg> chars;

    static_assert(msg.size() == 4);
  };

  "Display Unicode strings"_test = [] {
    constexpr auto greek = "α + β = γ"_cts;

    // Uncomment to see Unicode content:
    // ShowStaticString<greek> display;

    static_assert(greek.size() > 0);
  };

  "Display empty string"_test = [] {
    constexpr auto empty = ""_cts;

    // Uncomment to see empty string:
    // ShowStaticString<empty> display;

    static_assert(empty.size() == 0);
  };

  return 0;
}
