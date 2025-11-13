# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

**Tempura** is an experimental C++26 numerical methods library emphasizing compile-time computation through heavy template metaprogramming. The philosophy is "what if numerical algorithms were computed at compile-time?"

**Core Principles:**
- Correct over fast (algorithmic efficiency, not micro-optimization)
- constexpr-by-default (maximum compile-time evaluation)
- Zero STL dependencies in critical paths for pure compile-time metaprogramming
- Unicode embraced (code uses α, β, ∂ freely)
- Incremental development: **If functionality is missing, add it** - don't work around it

## Build Commands

### Environment Setup

```bash
nix develop  # Enter development shell (required for reproducible build environment)
```

**Requirements:** GCC 15+ or LLVM 20+, C++26 (`-std=c++2c`), AVX-512

### Building

Three build directories for different configurations:

```bash
# Debug build (default, with symbols)
cmake -B debug -G Ninja -DCMAKE_BUILD_TYPE=Debug
ninja -C debug

# Release build (aggressive optimizations: -O3, -march=native, -ffast-math)
cmake -B release -G Ninja -DCMAKE_BUILD_TYPE=Release
ninja -C release

# Standard build
cmake -B build -G Ninja
ninja -C build
```

### Running Tests

Uses custom `unit.h` testing framework (NOT GoogleTest/Catch2):

```bash
# Run all tests
ctest --test-dir build

# Run tests by pattern
ctest --test-dir build -R symbolic3
ctest --test-dir build -R matrix2

# Run tests by label
ctest --test-dir build -L base

# Run single test directly (useful for debugging)
./build/src/symbolic3/symbolic3_basic_test
```

**Test syntax:**
```cpp
"test description"_test = [] {
  static_assert(compile_time_check);  // Compile-time verification
  assert(runtime_check);               // Runtime verification
  expectEq(actual, expected);          // Custom matchers
};
```

### Running Single Tests

To run a specific test file directly (for debugging or focused testing):

```bash
# Build specific test
ninja -C build <test_name>

# Run it directly
./build/src/symbolic3/symbolic3_basic_test
./build/src/polynomial_test
```

### Command Auto-Approval Policy

**For Claude Code:** The following commands are auto-approved and don't require user confirmation:

**Build commands:**
- `cmake` (all variants: `-B build`, `-B debug`, `-B release`)
- `ninja -C build`, `ninja -C debug`, `ninja -C release`

**Test commands:**
- `ctest --test-dir build` (all variants with `-R`, `-L`, `--output-on-failure`, `--verbose`)

**Commands requiring user approval:**
- Running test binaries directly (e.g., `./build/src/bayes/bayes_exponential_test`)
- This allows users to control when tests execute and review output

**Recommendation:** Always prefer `ctest` over running binaries directly for faster workflow.

## Architecture

### Major Components

**`symbolic3/`** - **[CANONICAL]** Flagship symbolic computation system
- Combinator-based symbolic algebra with compile-time evaluation
- Pattern matching and rewriting system
- Two-stage oscillation-free simplification pipeline
- Exact arithmetic via `Fraction<N,D>`
- Transcendental functions (exp, log, trig, hyperbolic)
- Full constexpr support
- **Key insight:** Uses stateless lambdas for unique type identity, enabling zero-runtime-overhead symbolic computation

**`matrix2/`** - **[CANONICAL]** Constexpr-first matrix library
- Static and dynamic matrices with `MatRef<T>` for non-owning views
- Ownership explicit in types: `Transpose<Dense<>>` owns, `Transpose<MatRef<Dense<>>>` doesn't
- Interface libraries in `matrix2/storage/` (Dense, Banded, etc.)
- Full constexpr support

**`autodiff/`** - Automatic differentiation using dual numbers and computational graphs

**`meta/`** - Template metaprogramming utilities
- Value-based metaprogramming with `Meta<T>` type IDs
- Uses binary search and friend injection for compile-time unique type identification

**`optimization/`, `quadrature/`, `bayes/`, `special/`** - Numerical algorithms following Numerical Recipes approaches (modernized for C++26)

**`matrix3/`** - Experimental alternative (DO NOT USE - design exploration only)

**`cuda/`** - Exists but NOT actively developed (focus on CPU)

### Critical Design Patterns

1. **constexpr-by-default:** Nearly everything supports compile-time evaluation. Use `static_assert` liberally in tests.

2. **Interface libraries:** Most libraries are `INTERFACE` CMake targets (header-only). Pattern in `src/CMakeLists.txt`.

3. **Type-based ownership:** Constructors copy/move by default - users explicitly choose `MatRef` for non-owning views.

4. **Minimal stdlib for compile-time code:** `symbolic3/` and `meta/` minimize standard library usage for fast compilation and pure constexpr.

5. **Generic algorithms:** Operate on ranges/tuples/arrays, not hardcoded to specific types.

6. **Include paths:** Use `#include "matrix2/matrix.h"` not `#include "matrix.h"` - CMake sets `src/` as include root.

## Symbolic3 Architecture

The most complex component. Understanding this is critical for working with symbolic computation.

### Core Type System

**Expression representation:**
```cpp
Symbol<unique>                                  // Type-identified symbol (uses lambda for uniqueness)
Constant<42>                                    // Integer constant
Fraction<1,3>                                   // Exact rational (no float conversion)
Expression<AddOp, Symbol<0>, Constant<1>>       // x + 1
```

**Key headers:**
- `symbolic3.h` - Main entry point (includes all core functionality)
- `core.h` - `Symbol`, `Expression`, `Constant`, `Fraction`
- `operators.h` - Arithmetic and transcendental operators
- `simplify.h` - Simplification strategies and rule sets
- `pattern_matching.h` - Pattern matching and rewrite rules
- `strategy.h` - Strategy composition (`>>`, `|`, `when`)
- `traversal.h` - Tree traversal (innermost, outermost, fixpoint, fold, unfold)
- `evaluate.h` - Expression evaluation with type-safe bindings
- `derivative.h` - Symbolic differentiation
- `debug.h` - Compile-time debugging utilities

### Simplification Pipeline

Multi-stage pipeline using fixpoint iteration to avoid oscillation:

```cpp
// Full simplification (recommended)
auto result = simplify(expr, default_context());

// Individual rule sets (for debugging or custom pipelines)
auto result1 = algebraic_simplify(expr, ctx);       // Basic algebra
auto result2 = transcendental_simplify(expr, ctx);  // Trig/log/exp
auto result3 = collect_terms(expr, ctx);            // Like term collection
```

### Strategy Composition

Build complex transformations from simple rules:

```cpp
auto composed = strategyA >> strategyB;     // Sequential: A then B
auto fallback = strategyA | strategyB;      // Choice: try A, if unchanged try B
auto conditional = when(predicate, strat);   // Conditional application
auto fixed = fixpoint(strategy);             // Repeat until no change
auto bottom_up = innermost(strategy);        // Apply from leaves upward
auto top_down = outermost(strategy);         // Apply from root downward
```

**IMPORTANT:** Strategy ordering matters to avoid infinite loops. Use composition operators carefully.

### Pattern Matching

Wildcards for rewrite rules:

```cpp
x_, a_, b_, c_      // Match any single argument
AnyExpr             // Matches any expression
AnyConstant         // Matches any constant
AnySymbol           // Matches any symbol

// Example: x + 0 → x
if constexpr (match(expr, x_ + Constant<0>{})) {
  return get_matched_arg<0>(expr);  // Extract matched x
}
```

### Common Pitfalls

1. **Strategy ordering:** Some transformations must be applied in specific order to avoid infinite loops (see comments in `simplify.h`)
2. **Matrix extent handling:** `kDynamic` is `std::numeric_limits<int64_t>::min()`, not -1
3. **constexpr limitations:** Some functions have runtime fallbacks - check if context allows compile-time eval
4. **Expression complexity:** Large expressions increase compile times significantly

## Code Conventions

### Modern C++ Preferences

**Prefer modern C++20/C++26 over legacy alternatives:**
- `std::format` and `std::print` over `iostream` (type-safe, efficient)
- `std::span` over raw pointers (bounds information)
- `std::ranges` algorithms (composable, constexpr-friendly)
- `std::string_view` for non-owning string references
- Structured bindings: `auto [x, y] = get_pair()`

### Style Specifics

- Loosely follow Google C++ Style Guide with tempura flavors
- Use `&&`/`||` for logical operators (standard C++)
- Concepts for template constraints: `template<Symbolic S>` not `template<typename S> requires Symbolic<S>`
- `CHECK(condition)` macro for assertions (in `matrix2/matrix.h` - prints file/line, terminates)

### Naming Patterns

- `k` prefix for constants: `kDynamic`, `kRowMajor`, `kMeta<T>`
- Trailing `_` for pattern variables: `x_`, `a_`, `n_` in symbolic rewrite rules
- `T` suffix for concepts: `MatrixT`, `MatchingExtent`

## Debugging

### Symbolic3 Compile-Time Debugging

```cpp
#include "symbolic3/debug.h"

static_assert(expression_depth(expr) == 2);
static_assert(operation_count(expr) == 5);
static_assert(is_likely_simplified(expr));
force_type_display(expr);  // Causes error showing type
```

### Symbolic3 Runtime Debugging

```cpp
#include "symbolic3/to_string.h"

std::string str = toStringRuntime(expr);
std::print("Expression: {}\n", str);
```

### Debug Executables

Interactive debugging tools (development-only, not formal tests):

```bash
ninja -C build factoring_debug term_collecting_debug term_structure_debug
./build/src/symbolic3/factoring_debug
./build/src/symbolic3/term_collecting_debug
./build/src/symbolic3/term_structure_debug
```

### Template Error Debugging

Use compiler flags:

```bash
# GCC
g++ -ftime-report -ftemplate-depth=1024 -ftemplate-backtrace-limit=0 ...

# Clang
clang++ -ftime-trace ...
```

### Force Type Display

```cpp
template<typename T> struct ShowType;
ShowType<decltype(expr)> show;  // Error shows full type
```

## Examples

Key examples in `examples/`:

- `symbolic3_simplify_demo.cpp` - Simplification pipelines and strategies
- `symbolic3_debug_demo.cpp` - Debugging and visualization
- `advanced_simplify_demo.cpp` - Advanced symbolic transformations
- `normal_reverse_autodiff.cpp` - Autodiff computational graphs
- `idioms/` - Modern C++ design patterns (based on Klaus Iglberger's C++ Software Design)

## Testing Organization

Tests are organized by component, co-located with their implementations:

- `src/symbolic3/` - Symbolic computation tests
  - `basic_test.cpp` - Core symbolic operations
  - `simplification_*_test.cpp` - Simplification rules and pipelines
  - `pattern_matching_test.cpp` - Pattern matching
  - `derivative_test.cpp` - Symbolic differentiation
  - `evaluate_test.cpp` - Expression evaluation
  - `debug_test.cpp` - Debug utilities
  - `*_debug.cpp` - Interactive debug tools (NOT formal tests)

- `src/matrix2/` - Matrix library tests
- `src/bayes/` - Bayes/probability distribution tests
- `src/quadature/` - Numerical integration tests
- `src/optimization/` - Optimization algorithm tests
- `src/` - Top-level utility tests (polynomial, json, sequence, root_finding, interpolate, chebyshev, etc.)

## Documentation

- **Getting Started:** `src/symbolic3/README.md` (comprehensive API documentation)
- **Debugging Guide:** `src/symbolic3/DEBUGGING.md`
- **Architecture Details:** `.github/copilot-instructions.md` (detailed design patterns)
- **Next Steps:** `NEXT_STEPS_BRAINSTORM.md`
- **Code is canonical:** Header comments and tests are primary documentation source

## Development Philosophy

This is a **research/learning project**, not production code. When you need functionality:

1. **Extend existing libraries** rather than working around limitations
2. **Add missing functions** to the appropriate header
3. **Write tests** with `static_assert` for constexpr code
4. **Follow existing patterns** in the same directory

Example: Need matrix transpose? Add it to `matrix2/` following the `MatRef` ownership pattern.

## What This Project Is Not

- Production-ready numerical library
- Active GPU/CUDA development (on hold)
- Traditional runtime polymorphism (template-based compile-time polymorphism only)

When in doubt, check test files - they serve as executable documentation.
