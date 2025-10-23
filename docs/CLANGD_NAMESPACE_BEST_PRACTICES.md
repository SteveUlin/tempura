# clangd Namespace Best Practices for Tempura

## TL;DR - Quick Fix

If you're getting "toString is ambiguous" errors in nvim/clangd:

```cpp
// ❌ DON'T DO THIS
using namespace tempura;
using namespace tempura::symbolic3;

// ✅ DO THIS INSTEAD
using namespace tempura::symbolic3;
using tempura::StaticString;
using tempura::operator""_cts;
```

## The Problem

Tempura has multiple `toString` functions in different namespaces:

```cpp
namespace tempura {
  // toString for JSON objects
  constexpr auto toString(const JsonValue& obj, size_t indentLevel = 0);

  // toString for matrices
  constexpr auto toString(const MatT& m) -> std::string;
}

namespace tempura::symbolic3 {
  // toString for symbolic expressions
  constexpr auto toString(Constant<N>);
  constexpr auto toString(Symbol<Tag>);
  constexpr auto toString(Expression<Op, Args...>);
}
```

When you use both `using namespace tempura;` and `using namespace tempura::symbolic3;`, you bring **both** sets of `toString` functions into scope, causing ambiguity.

## Why clangd is Different from GCC

**clangd is stricter:**
- Catches ambiguity earlier in compilation
- More aggressive about reporting potential errors
- Doesn't rely on error recovery like GCC sometimes does
- Uses different name lookup heuristics

**GCC may compile successfully because:**
- Argument-dependent lookup (ADL) may resolve the ambiguity
- Error recovery mechanisms may pick "best match"
- Template instantiation happens at different stages

**Best practice:** Fix clangd errors! They indicate potential issues even if GCC compiles.

## Recommended Namespace Usage Patterns

### Pattern 1: Symbolic Expression Files (Recommended)

```cpp
#include "symbolic3/debug.h"
#include "symbolic3/simplify.h"

// Only import what you need from symbolic3
using namespace tempura::symbolic3;

// Selectively import from tempura namespace
using tempura::operator""_cts;
using tempura::operator""_test;  // For unit tests

// If nvim/clangd still shows ambiguity, fully qualify toString:
constexpr auto str = tempura::symbolic3::toString(expr);  // Most reliable
// or use unqualified if your clangd config allows:
constexpr auto str2 = toString(expr);  // May cause ambiguity in some configs
```

### Pattern 2: Minimal Imports (Most Explicit)

```cpp
#include "symbolic3/debug.h"

// No namespace imports - use fully qualified names
using tempura::symbolic3::Symbol;
using tempura::symbolic3::Constant;
using tempura::symbolic3::toString;
using tempura::operator""_cts;
```

### Pattern 3: Mixed Code (Symbolic + Matrix)

```cpp
#include "symbolic3/symbolic3.h"
#include "matrix2/matrix.h"

// Use aliases to disambiguate
namespace sym = tempura::symbolic3;
namespace mat = tempura::matrix2;

void example() {
  auto expr = sym::Symbol<decltype([] {})>{};
  auto expr_str = sym::toString(expr);

  auto m = mat::Dense<double, 2, 2>{};
  auto mat_str = mat::toString(m);
}
```

### Pattern 4: Explicit Disambiguation (When Necessary)

```cpp
#include "symbolic3/to_string.h"
#include "json.h"

using namespace tempura;
using namespace tempura::symbolic3;

void example() {
  auto expr = Symbol<decltype([] {})>{};

  // Explicitly qualify to avoid ambiguity
  auto str = symbolic3::toString(expr);  // ✅ Unambiguous

  // Or use fully qualified name
  auto str2 = tempura::symbolic3::toString(expr);  // ✅ Also works
}
```

## Common Scenarios

### Unit Tests with symbolic3

```cpp
#include "symbolic3/symbolic3.h"
#include "unit.h"

using namespace tempura::symbolic3;
using tempura::operator""_test;
using tempura::operator""_cts;

auto main() -> int {
  "expression simplification"_test = [] {
    constexpr auto x = Symbol<decltype([] {})>{};
    constexpr auto result = toString(x + x);  // Unambiguous
    static_assert(result == "x0 + x0"_cts);
  };
  return 0;
}
```

### Examples with Multiple Components

```cpp
#include <print>
#include "symbolic3/symbolic3.h"
#include "matrix2/matrix.h"

// Namespace aliases for clarity
namespace sym = tempura::symbolic3;
namespace mat = tempura::matrix2;

int main() {
  auto x = sym::Symbol<decltype([] {})>{};
  auto expr_str = sym::toString(x * x);

  auto M = mat::Dense<double, 2, 2>{{1, 2}, {3, 4}};
  auto mat_str = mat::toString(M);

  std::print("Expression: {}\n", expr_str.c_str());
  std::print("Matrix: {}\n", mat_str);
}
```

### Debugging with ShowStaticString

```cpp
#include "symbolic3/debug.h"

using namespace tempura::symbolic3;
using tempura::operator""_cts;

void debug() {
  constexpr auto x = Symbol<decltype([] {})>{};
  constexpr auto expr = x * x + Constant<5>{};

  // toString is unambiguous here
  constexpr auto str = toString(expr);

  // Uncomment to see compile-time string display:
  // ShowStaticString<str> show;
}
```

## What to Import from `tempura` Namespace

Only import specific items you need:

```cpp
// User-defined literals
using tempura::operator""_cts;     // StaticString literal
using tempura::operator""_test;    // Unit test literal

// Types (if needed)
using tempura::StaticString;
using tempura::CompileTimeString;

// Operators (rarely needed directly)
using tempura::AddOp;
using tempura::MulOp;
// ... etc
```

**Never import everything:** `using namespace tempura;` brings in too much!

## clangd Configuration

If you're still having issues, check your `.clangd` config:

```yaml
CompileFlags:
  Add:
    - -std=c++2c
    - -I/path/to/tempura/src
Diagnostics:
  UnusedIncludes: Strict
  ClangTidy:
    Add:
      - readability-*
      - modernize-*
```

## Summary

1. **Use `using namespace tempura::symbolic3;`** for symbolic code
2. **Selectively import** from `tempura::` namespace with `using tempura::X;`
3. **Never use `using namespace tempura;`** when working with symbolic3
4. **Fix clangd errors** - they're more reliable than GCC's error recovery
5. **Use namespace aliases** (`namespace sym = tempura::symbolic3;`) for mixed code

Following these patterns ensures your code works correctly in both clangd and GCC!
