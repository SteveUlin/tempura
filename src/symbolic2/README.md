# Tempura Symbolic 2.0

**Modern C++26 compile-time symbolic mathematics library**

This is a complete rewrite of the original `symbolic/` directory, incorporating lessons learned and leveraging modern C++26 features for cleaner, more maintainable code.

## Overview

Tempura Symbolic 2.0 is a header-only library for compile-time symbolic mathematics and expression manipulation. It uses expression templates and type-based programming to represent and manipulate mathematical expressions at compile time.

## Key Features

- **Auto-unique symbols**: Simply declare `Symbol x;` - no macros needed
- **Intuitive syntax**: Write natural mathematical expressions: `x + y * 2`
- **Compile-time evaluation**: Bind values and evaluate expressions at compile time
- **Pattern matching**: Match and transform expressions using wildcards
- **Automatic simplification**: Built-in rules for algebraic simplification
- **Symbolic differentiation**: Compute derivatives symbolically
- **Type-safe**: All errors caught at compile time

## Quick Start

```cpp
#include "symbolic2/symbolic.h"

using namespace tempura;

// Define symbols
Symbol x;
Symbol y;

// Build expressions
auto expr = x * x + 2_c * x + 1_c;

// Evaluate
constexpr auto result = evaluate(expr, BinderPack{x = 5});
static_assert(result == 36);  // 5*5 + 2*5 + 1 = 36

// Simplify
auto simplified = simplify(x + x);  // Results in: x * 2
```

## Architecture Improvements over v1

### Simpler Symbol Creation

**v1 (old):**

```cpp
TEMPURA_SYMBOL(x);
TEMPURA_SYMBOLS(a, b, c);  // Macro with expansion magic
```

**v2 (new):**

```cpp
Symbol x;
Symbol y;
Symbol z;
```

Each symbol is automatically unique via lambda types: `Symbol<decltype([]{})>`

### Unified Type System

- **Single Expression type**: `Expression<Op, Args...>` (no separate MatcherExpression)
- **Tag-based operators**: Reuses `meta/function_objects.h` operators
- **Integrated matching**: Wildcards (AnyArg, AnyExpr, AnyConstant) work seamlessly

### Robust Ordering

- Uses compile-time type ID counter (`kMeta<T>`) instead of string comparison
- Deterministic canonical forms
- Better compilation performance

### Self-Contained

- No external TypeList dependency
- All features in one header (symbolic.h)
- Derivatives and toString in separate optional headers

## Components

- **symbolic.h**: Core symbolic system (symbols, constants, expressions, evaluation, simplification)
- **derivative.h**: Symbolic differentiation
- **to_string.h**: Expression formatting and printing

## Migration from v1

See [MIGRATION.md](MIGRATION.md) for a detailed migration guide from the old `symbolic/` implementation.

## Examples

See the `examples/` directory for comprehensive examples:

- `basic_usage.cpp`: Introduction to basic features
- `advanced_simplify.cpp`: Complex simplification patterns
- `derivatives.cpp`: Symbolic differentiation examples

## Status

🚧 **Active Development** - This is the future of Tempura Symbolic

- ✅ Core symbolic system
- ✅ Pattern matching
- ✅ Simplification rules (basic)
- ✅ Evaluation system
- 🚧 Advanced simplification (in progress)
- 🚧 Derivative support (planned)
- 🚧 Enhanced toString (planned)
- 📋 Full operator coverage (planned)

## Known Limitations

**Template Instantiation Depth**: Complex expressions with deep simplification may exceed compiler template instantiation limits (~900 levels). This affects:

- Repeated distribution: `(x+1)*(x+1)*(x+1)...`
- Transcendental functions with constants: `log(1)`, `exp(log(x))`, `sin(π)`
- Long expression chains with many simplification steps

**Workaround**: Simplify expressions incrementally or increase template depth: `-ftemplate-depth=2000`

**Note**: This is a known limitation of compile-time metaprogramming. Future improvements will add depth limiting and better simplification strategies.

## Deprecation Timeline

The original `symbolic/` directory will be deprecated once symbolic2 reaches feature parity:

1. **Phase 1** (Current): Parallel development
2. **Phase 2**: Mark symbolic/ as deprecated
3. **Phase 3**: symbolic2/ becomes symbolic/
4. **Phase 4**: Remove old implementation

## Design Philosophy

1. **Simplicity**: Minimize boilerplate and macros
2. **Type Safety**: Leverage C++ type system for compile-time guarantees
3. **Modern C++**: Use C++26 features where they improve clarity
4. **Single Responsibility**: Each component does one thing well
5. **Zero Runtime Cost**: Everything happens at compile time

## Contributing

When adding features, please:

- Add comprehensive tests
- Document new patterns in examples
- Update this README
- Follow the existing code style
- Ensure all operations are constexpr

## License

Same as Tempura project
