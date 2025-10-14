# Tempura

**Tempura** is an experimental C++26 numerical methods library emphasizing compile-time computation through heavy template metaprogramming. Think "what if numerical algorithms were computed at compile-time?"

## Core Philosophy

- **Correct over fast**: Algorithmic efficiency, not micro-optimization
- **constexpr-by-default**: Maximum compile-time evaluation
- **Zero STL dependencies** (in critical paths): Pure compile-time metaprogramming
- **Unicode embraced**: Code uses mathematical notation (α, β, ∂) freely

## Major Components

### ⭐ **symbolic3/** - Compile-Time Symbolic Computation

**[CANONICAL]** The flagship component - combinator-based symbolic algebra with:

- Pattern matching and rewriting system
- Two-stage simplification pipeline (oscillation-free)
- Exact arithmetic via fractions
- Transcendental functions (exp, log, trig, hyperbolic)
- Full constexpr evaluation at compile-time

**Documentation**: See `src/symbolic3/README.md` for comprehensive API docs

**Example**:

```cpp
constexpr Symbol x;
constexpr auto f = pow(x, 2_c) + 2_c * x + 1_c;
constexpr auto df = simplify(diff(f, x), default_context());
// Result: 2·x + 2 (computed at compile-time!)
```

### **matrix2/** - Constexpr Matrix Library

**[CANONICAL]** Static and dynamic matrices with:

- `MatRef<T>` for non-owning views (ownership explicit in types)
- Interface libraries for Dense, Banded, etc.
- Full constexpr support

### **autodiff/** - Automatic Differentiation

Dual numbers and computational graphs for gradient computation

### **meta/** - Template Metaprogramming Utilities

Value-based metaprogramming with `Meta<T>` type IDs via binary search and friend injection

### **optimization/**, **quadrature/**, **bayes/**, **special/**

Numerical algorithms following Numerical Recipes approaches (modernized for C++26)

## Build System

Uses **Nix flakes** for reproducible builds:

```bash
nix develop          # Enter development shell
cmake -B build       # Configure
ninja -C build       # Build
ctest --test-dir build -L symbolic3  # Run tests
```

**Requirements**: GCC 15+ or LLVM 20+ (C++26: `-std=c++2c`), AVX-512

## Project Status

**Phase**: Core infrastructure complete, well-documented, stable
**Focus**: Symbolic algebra maturity
**Recent**: Comprehensive documentation improvements and oscillation prevention tests

## Documentation

- **Getting Started**: `src/symbolic3/README.md`
- **Debugging Guide**: `src/symbolic3/DEBUGGING.md`
- **Architecture**: `.github/copilot-instructions.md`
- **Next Steps**: `NEXT_STEPS_BRAINSTORM.md`

## Design Decisions

**Incremental Development**: Libraries are built incrementally. **If functionality is missing, add it** - don't work around it. Extend existing APIs to support your needs.

**Type-Based Ownership**: `Transpose<Dense<>>` owns data, `Transpose<MatRef<Dense<>>>` doesn't. Users explicitly choose `MatRef` for non-owning views.

**Generic Algorithms**: Operate on ranges/tuples/arrays, not hardcoded to specific types.

## Notable Patterns

- **constexpr testing**: Use `static_assert` liberally to verify compile-time behavior
- **Interface libraries**: Most libraries are `INTERFACE` CMake targets (header-only)
- **Minimal stdlib**: `symbolic3` and `meta` avoid STL for fast compilation

## Example: Symbolic Simplification

```cpp
#include "symbolic3/simplify.h"

using namespace tempura::symbolic3;

constexpr Symbol x, y;

// Algebraic simplification
constexpr auto expr1 = x + x + x;
constexpr auto result1 = simplify(expr1, default_context());
static_assert(match(result1, 3_c * x));  // Compile-time verification!

// Transcendental functions
constexpr auto expr2 = exp(log(y));
constexpr auto result2 = simplify(expr2, default_context());
static_assert(match(result2, y));  // exp and log cancel

// Exact arithmetic
constexpr auto expr3 = Fraction<1,2>{} + Fraction<1,3>{};
constexpr auto result3 = simplify(expr3, default_context());
static_assert(match(result3, Fraction<5,6>{}));  // Exact rational arithmetic
```

## Contributing

This is primarily a research/learning project. The codebase prioritizes:

1. Correctness and clarity
2. Algorithmic efficiency
3. Compile-time evaluation
4. Readable code with good examples

## License

See `LICENSE` file.
