#pragma once

#include <print>
#include <tuple>
#include <type_traits>

#include "meta/macro_utils.h"
#include "symbolic3/core.h"
#include "symbolic3/to_string.h"

// Pretty printing macros for symbolic expressions
// Automatically generates symbol-to-name mappings

namespace tempura::symbolic3 {
}  // namespace tempura::symbolic3

// =============================================================================
// PRETTY_PRINT MACRO
// =============================================================================

// Helper macro: converts a symbol to a (symbol, "name"_cts) pair
#define TEMPURA_SYMBOL_PAIR(sym) sym, #sym##_cts

// Main pretty print macro - generates compile-time string
// Usage: constexpr auto str = PRETTY_PRINT(expr, x, y, z);
//   - expr: symbolic expression to stringify
//   - ...: all symbols used in the expression
//
// The macro will:
//   1. Auto-generate name mappings: x -> "x", y -> "y", z -> "z"
//   2. Verify at compile-time that all symbols in expr are provided
//   3. Return a constexpr StaticString
//
// Example:
//   Symbol x, y;
//   auto expr = x * x + 2 * y;
//   constexpr auto str = PRETTY_PRINT(expr, x, y);  // "x * x + 2 * y"
//   static_assert(str == "x * x + 2 * y");
//
#define PRETTY_PRINT(expr, ...)                                                        \
  ([&]() {                                                                             \
    using namespace tempura;                                                           \
    constexpr auto _pp_ctx = ::tempura::symbolic3::makeSymbolNames(                    \
        __VA_OPT__(TEMPURA_FOR_EACH_COMMA(TEMPURA_SYMBOL_PAIR, __VA_ARGS__))          \
    );                                                                                 \
    return ::tempura::symbolic3::toString(expr, _pp_ctx);                              \
  }())
