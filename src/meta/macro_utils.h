#pragma once

// Variadic Macro Iteration Utilities
// ====================================
// Provides TEMPURA_FOR_EACH for applying a macro to each variadic argument.
//
// QUICK REFERENCE
// ---------------
// TEMPURA_FOR_EACH(macro, ...args)
//   Applies macro(arg) to each argument, separated by semicolons.
//   Supports up to ~256 arguments.
//   Uses C++20 __VA_OPT__ for clean empty argument handling.
//
// Example:
//   #define DECLARE_VAR(x) int x = 0
//   TEMPURA_FOR_EACH(DECLARE_VAR, a, b, c)
//   // Expands to: int a = 0; int b = 0; int c = 0;
//
// Based on: https://www.scs.stanford.edu/~dm/blog/va-opt.html

// MOTIVATION: Why Not Just Use __VA_ARGS__?
// -------------------------------------------
// __VA_ARGS__ captures variadic arguments but provides NO iteration mechanism.
//
// What __VA_ARGS__ can do:
//   ✓ Pass all args as a group:  printf(__VA_ARGS__)
//   ✓ Stringify all args:         #__VA_ARGS__
//
// What __VA_ARGS__ CANNOT do:
//   ✗ Apply an operation to EACH argument individually
//
// Problem: Without iteration, repetitive declarations become tedious:
//   int x = 0; int y = 0; int z = 0;  // Manual repetition
//
// Solution: TEMPURA_FOR_EACH automates this:
//   #define DECLARE(v) int v = 0
//   TEMPURA_FOR_EACH(DECLARE, x, y, z)
//   // Expands to: int x = 0; int y = 0; int z = 0;
//
// The machinery below implements iteration by recursively peeling off
// one argument at a time using the C preprocessor.
//
// IMPLEMENTATION: Why the Complex Expansion Machinery?
// ------------------------------------------------------
// The C preprocessor doesn't expand macros recursively in a single pass.
// When TEMPURA_FOR_EACH_HELPER recursively invokes itself (via
// TEMPURA_FOR_EACH_AGAIN), multiple expansion rounds are needed.
//
// Problem: Without TEMPURA_EXPAND, recursion terminates after 2-3 arguments.
//   The preprocessor sees "TEMPURA_FOR_EACH_AGAIN(...)" as already expanded
//   and refuses to expand it further, even though it should trigger another
//   round of TEMPURA_FOR_EACH_HELPER.
//
// Solution: TEMPURA_EXPAND forces additional expansion rounds by nesting
//   layers. Each layer expands its argument 4 times:
//
//   TEMPURA_EXPAND1: 4^1 = 4 rounds    → handles ~4 arguments
//   TEMPURA_EXPAND2: 4^2 = 16 rounds   → handles ~16 arguments
//   TEMPURA_EXPAND3: 4^3 = 64 rounds   → handles ~64 arguments
//   TEMPURA_EXPAND4: 4^4 = 256 rounds  → handles ~256 arguments
//
// The TEMPURA_PARENS() trick delays expansion by preventing
// TEMPURA_FOR_EACH_AGAIN from immediately looking like a function-like
// macro invocation. This delay is crucial for the recursion mechanism.
//
// For deeper explanation: https://www.scs.stanford.edu/~dm/blog/va-opt.html

// ============================================================================
// IMPLEMENTATION DETAILS
// ============================================================================

// Delays macro expansion by one round (prevents premature function-like macro
// recognition)
#define TEMPURA_PARENS ()

// Multi-level expansion engine: forces preprocessor to perform 4^4 = 256
// expansion rounds by nesting 4 layers, each expanding its argument 4 times
#define TEMPURA_EXPAND(...) \
  TEMPURA_EXPAND4(          \
      TEMPURA_EXPAND4(TEMPURA_EXPAND4(TEMPURA_EXPAND4(__VA_ARGS__))))
#define TEMPURA_EXPAND4(...) \
  TEMPURA_EXPAND3(           \
      TEMPURA_EXPAND3(TEMPURA_EXPAND3(TEMPURA_EXPAND3(__VA_ARGS__))))
#define TEMPURA_EXPAND3(...) \
  TEMPURA_EXPAND2(           \
      TEMPURA_EXPAND2(TEMPURA_EXPAND2(TEMPURA_EXPAND2(__VA_ARGS__))))
#define TEMPURA_EXPAND2(...) \
  TEMPURA_EXPAND1(           \
      TEMPURA_EXPAND1(TEMPURA_EXPAND1(TEMPURA_EXPAND1(__VA_ARGS__))))
#define TEMPURA_EXPAND1(...) __VA_ARGS__

// Main FOR_EACH entry point
// Applies macro(arg) to each argument, separated by semicolons.
// Uses __VA_OPT__ (C++20) to cleanly handle empty argument lists (no-op).
//
// Example:
//   #define PRINT_VAR(x) std::print("{} = {}\n", #x, x)
//   TEMPURA_FOR_EACH(PRINT_VAR, a, b, c)
//   // Expands to: std::print(...); std::print(...); std::print(...);
#define TEMPURA_FOR_EACH(macro, ...) \
  __VA_OPT__(TEMPURA_EXPAND(TEMPURA_FOR_EACH_HELPER(macro, __VA_ARGS__)))

// Recursive helper: processes one argument, then recurses on remaining args
// Pattern: macro(a1); [recursively process rest if any exist]
#define TEMPURA_FOR_EACH_HELPER(macro, a1, ...) \
  macro(a1);                                    \
  __VA_OPT__(TEMPURA_FOR_EACH_AGAIN TEMPURA_PARENS(macro, __VA_ARGS__))

// Indirection layer: enables recursive expansion by preventing self-reference
// during macro substitution
#define TEMPURA_FOR_EACH_AGAIN() TEMPURA_FOR_EACH_HELPER
