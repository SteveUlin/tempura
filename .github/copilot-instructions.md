# Tempura AI Coding Guide

## Project Overview

Tempura is an experimental **C++26 numerical methods library** emphasizing compile-time computation through heavy template metaprogramming. Think "what if numerical algorithms were computed at compile-time?" The codebase prioritizes correctness over speed, constexpr-by-default, and algorithmic efficiency over micro-optimization.

## Core Architecture

### Major Components

- **`symbolic3/`**: **[CANONICAL]** Combinator-based symbolic computation with compile-time evaluation. Uses stateless lambdas for unique type identity, composable transformation strategies, and zero runtime overhead. Example: `auto f = x*x + 2_c*x; auto df = full_simplify.apply(diff(f, x), default_context());` produces `2·x + 2` at compile-time. Supports multiple recursion strategies (fixpoint, fold, unfold, innermost, outermost), context-aware transformations, and user-extensible strategies.

- **`matrix2/`**: **[CANONICAL]** Constexpr-first matrix library with static/dynamic extents. Uses `MatRef<T>` for non-owning views (ownership explicit in types). Interface libraries defined in `matrix2/storage/` for Dense, Banded, etc. Use this for all matrix work.

- **`matrix3/`**: Experimental alternative matrix implementation. **Do not use** - exists only for design exploration.

- **`autodiff/`**: Automatic differentiation using dual numbers and computational graphs.

- **`meta/`**: Value-based template metaprogramming utilities. Uses `Meta<T>` for compile-time unique type IDs via binary search and friend injection (see `meta/type_id.h`).

- **`bayes/`, `optimization/`, `quadature/`, `special/`**: Numerical algorithm implementations following Numerical Recipes approaches but modernized.

**Note on CUDA**: `cuda/` directory exists but is **not actively developed**. Focus on CPU implementations.

### Critical Design Patterns

1. **constexpr-by-default**: Nearly everything supports compile-time evaluation. Use `static_assert` liberally in tests to verify compile-time behavior.

2. **Interface libraries**: Most libraries are `INTERFACE` CMake targets (header-only). See pattern in `src/CMakeLists.txt`.

3. **Type-based ownership**: `Transpose<Dense<>>` owns, `Transpose<MatRef<Dense<>>>` doesn't. Constructors copy/move by default - users explicitly choose `MatRef` for non-owning.

4. **Unicode embraced**: Code uses Greek letters (`α`, `β`, `∂`) freely. Ensure your font supports mathematical Unicode.

5. **Generic containers**: Algorithms operate on ranges/tuples/arrays, not hardcoded to specific matrix types.

6. **Incremental development**: Libraries are built incrementally. **If functionality is missing, add it** - don't work around it. Extend existing APIs to support your needs.

7. **Minimal stdlib for compile-time code**: `symbolic3/` and `meta/` should depend on standard library **as little as possible** to minimize compile times and enable pure compile-time computation.

## Build & Test Workflow

### Environment Setup

Uses **Nix flakes** for reproducible builds. Critical dependencies:

- GCC 15 or LLVM 20 (C++26 features: `std=c++2c`)
- AVX-512 enabled (`-mavx512f -mavx512dq`)
- CUDA 12.1, Vulkan, GLFW for graphics examples

```bash
nix develop  # Enter development shell
```

### Build Commands

Three build directories for different configs:

```bash
# Debug build (default)
cmake -B debug -G Ninja -DCMAKE_BUILD_TYPE=Debug
ninja -C debug

# Release build (aggressive optimizations)
cmake -B release -G Ninja -DCMAKE_BUILD_TYPE=Release
ninja -C release

# Standard build
cmake -B build -G Ninja
ninja -C build
```

### Running Tests

Uses custom `unit.h` testing framework (see `src/unit.h`):

```cpp
// Test syntax: "description"_test = lambda
"Addition works"_test = [] {
  constexpr int result = 2 + 2;
  static_assert(result == 4);  // Compile-time check
  assert(result == 4);          // Runtime check
};

// Tests with runtime values
"Dynamic computation"_test = [] {
  std::vector<int> data = {1, 2, 3};
  assert(data.size() == 3);
};
```

**Key testing patterns**:

- Use `static_assert` for compile-time verification (most tempura code)
- Use `assert()` for runtime checks
- Test failures appear as compilation errors (for static_assert) or assertion failures
- No test fixtures - tests are standalone lambdas

**Running tests**:

```bash
ctest --test-dir build                    # All tests
ctest --test-dir build -R symbolic3       # Pattern match
ctest --test-dir build -L base            # By label
./build/src/symbolic3/test/basic_test     # Direct execution for debugging
```

**Test organization**:

- Most test files are in component directories: `src/symbolic3/test/`, `src/matrix2/test/`
- Some tests in `test/` directory: `test/polynomial_test.cpp`, `test/json_test.cpp`
- Each test file produces one executable with multiple test cases

## Code Conventions

### Style Specifics

- **Loosely follow Google C++ Style Guide** but with tempura flavors
- **Use `&&`/`||` for logical operators** (standard C++ operators preferred)
- **Concepts for template constraints**: `template<Symbolic S>` not `template<typename S> requires Symbolic<S>`
- **CHECK macro for assertions**: `CHECK(condition)` in `matrix2/matrix.h` prints file/line and terminates

### Modern C++ APIs

**Prefer modern C++20/C++26 standard library features over legacy alternatives:**

- **Use `std::format` and `std::print` over `iostream`**: Type-safe, efficient, and more readable. Example: `std::print("Value: {}\n", x)` instead of `std::cout << "Value: " << x << std::endl;`
- **Use `std::span` over raw pointers**: Type-safe views with bounds information
- **Use `std::ranges` algorithms**: Composable and constexpr-friendly
- **Use `std::expected` for error handling**: When appropriate (requires C++23 support)
- **Use structured bindings**: `auto [x, y] = get_pair()` for clarity
- **Use `std::string_view`**: For non-owning string references

The codebase targets C++26 - embrace modern features that improve safety, readability, and compile-time evaluation.

### Naming Patterns

- **`k` prefix for constants**: `kDynamic`, `kRowMajor`, `kMeta<T>`
- **Trailing `_` for pattern variables**: `x_`, `a_`, `n_` in symbolic rewrite rules
- **`T` suffix for concepts**: `MatrixT`, `MatchingExtent`

### Common Pitfalls

1. **symbolic3 strategy ordering matters**: Some transformations must be applied in specific order to avoid infinite loops. Use composition operators (`>>`, `|`) to control application order. See comments in `symbolic3/simplify.h`.

2. **Matrix extent handling**: `kDynamic` is `std::numeric_limits<int64_t>::min()`, not -1.

3. **Include paths**: Use `#include "matrix2/matrix.h"` not `#include "matrix.h"` - CMake sets `src/` as include root.

4. **constexpr limitations**: Some functions have runtime fallbacks. Check if context allows compile-time eval.

## Symbolic3 Deep Dive

The symbolic math system is the most complex component:

- **Core types** (`symbolic3/core.h`): `Symbol<unique>` uses stateless lambdas for type identity, `Expression<Op, Args...>` is pure type-system computation. Includes `Fraction<N, D>` for exact arithmetic.

- **Pattern matching** (`symbolic3/pattern_matching.h`): Wildcards `x_`, `a_`, `c_`, `AnyArg`, `AnyExpr`, `AnyConstant`, `AnySymbol`. Match with `match(expr, pattern)`.

- **Strategy system** (`symbolic3/strategy.h`, `symbolic3/traversal.h`): Composable transformation strategies using concepts. Combinators include `>>` (sequential), `|` (choice), `when` (conditional), plus traversal strategies like `fold`, `unfold`, `innermost`, `outermost`, `fixpoint`.

- **Context-aware transforms** (`symbolic3/context.h`): Type-safe context passing with compile-time tags and data-driven modes for domain-specific simplification rules.

- **Evaluation** (`symbolic3/evaluate.h`): `evaluate(expr, BinderPack{x = 5, y = 3.0})` - heterogeneous type-safe bindings with full constexpr support.

## Examples & Idioms

See `examples/` for usage patterns. Key examples:

- `examples/symbolic3_simplify_demo.cpp`: Symbolic3 simplification pipelines and strategies
- `examples/symbolic3_debug_demo.cpp`: Debugging and visualization of symbolic expressions
- `examples/advanced_simplify_demo.cpp`: Advanced simplification techniques
- `examples/normal_reverse_autodiff.cpp`: Autodiff computational graphs
- `examples/idioms/`: Modern C++ design patterns (reference: Klaus Iglberger's C++ Software Design)

## Debugging Tips

1. **Template errors**: Use `__PRETTY_FUNCTION__` or compiler's `-ftemplate-backtrace-limit=0` to see full instantiation chains.

2. **Compile-time debugging**: `static_assert(false, typeid(expr).name())` to print types (fails compilation but shows info).

3. **Performance profiling**: `profiler.h` provides simple benchmarking. See `benchmark.h` for more sophisticated timing.

4. **Symbolic simplification**: Add `static_assert(match(expected, actual))` in tests to verify symbolic equality at compile-time.

## Development Philosophy

**Incremental and Exploratory**: This is a research/learning project, not production code. When you need functionality:

1. **Extend existing libraries** rather than working around limitations
2. **Add missing functions** to the appropriate header
3. **Write tests** to verify behavior (especially `static_assert` for constexpr code)
4. **Follow existing patterns** in the same directory

**Example**: Need matrix transpose? Add it to `matrix2/` following the `MatRef` ownership pattern, write tests with `static_assert` for compile-time behavior.

## What's Not Here

- Production-ready numerical library (experimental playground)
- Active GPU/CUDA development (on hold)
- Comprehensive documentation (code comments and tests are canonical)
- Traditional runtime polymorphism (template-based compile-time polymorphism only)

When in doubt, check test files - they serve as executable documentation.
