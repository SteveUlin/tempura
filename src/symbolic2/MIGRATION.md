# Migration Guide: symbolic/ → symbolic2/

This guide helps you migrate from the old `symbolic/` implementation to the new `symbolic2/` implementation.

## Quick Reference

| Feature          | Old (symbolic/)                      | New (symbolic2/)                    |
| ---------------- | ------------------------------------ | ----------------------------------- |
| Include          | `#include "symbolic/symbolic.h"`     | `#include "symbolic2/symbolic.h"`   |
| Namespace        | `tempura::symbolic`                  | `tempura`                           |
| Symbol creation  | `TEMPURA_SYMBOL(x)`                  | `Symbol x;`                         |
| Multiple symbols | `TEMPURA_SYMBOLS(a, b, c)`           | `Symbol a; Symbol b; Symbol c;`     |
| Evaluation       | `expr(Substitution{a = 5})`          | `evaluate(expr, BinderPack{a = 5})` |
| Expression type  | `SymbolicExpression<Op, Args...>`    | `Expression<Op, Args...>`           |
| Matching         | `MatcherExpression<Op, Matchers...>` | Same `Expression` with wildcards    |
| Simplify         | `simplify(expr)`                     | `simplify(expr)` ✓ same             |
| Constants        | `3_c`                                | `3_c` ✓ same                        |

## Step-by-Step Migration

### 1. Update Includes

**Before:**

```cpp
#include "symbolic/symbolic.h"
#include "symbolic/operators.h"
#include "symbolic/simplify.h"
#include "symbolic/to_string.h"
```

**After:**

```cpp
#include "symbolic2/symbolic.h"
// Optional:
// #include "symbolic2/derivative.h"
// #include "symbolic2/to_string.h"
```

### 2. Update Namespace

**Before:**

```cpp
using namespace tempura::symbolic;
```

**After:**

```cpp
using namespace tempura;
```

### 3. Declare Symbols

**Before:**

```cpp
TEMPURA_SYMBOL(x);
TEMPURA_SYMBOL(y);
// or
TEMPURA_SYMBOLS(x, y, z);
```

**After:**

```cpp
Symbol x;
Symbol y;
Symbol z;
```

### 4. Update Evaluation

**Before:**

```cpp
auto result = expr(Substitution{x = 5, y = 3});
```

**After:**

```cpp
auto result = evaluate(expr, BinderPack{x = 5, y = 3});
```

### 5. Pattern Matching

**Before:**

```cpp
// Separate types for matching
if constexpr (match(expr, Any{} + Any{})) {
    auto lhs = expr.left();
    auto rhs = expr.right();
}
```

**After:**

```cpp
// Same concept, different wildcard names
if constexpr (match(expr, AnyArg{} + AnyArg{})) {
    auto lhs = left(expr);
    auto rhs = right(expr);
}
```

### 6. Custom Operators

**Before:**

```cpp
struct MyOp {
    static inline constexpr std::string_view kSymbol = "myop"sv;
    static inline constexpr DisplayMode kDisplayMode = DisplayMode::kPrefix;

    template <typename... Args>
    static constexpr auto operator()(Args... args) {
        // implementation
    }
};
```

**After:**

```cpp
// Import from meta/function_objects.h or define similarly
struct MyOp {
    static inline constexpr std::string_view kSymbol = "myop"sv;
    static inline constexpr DisplayMode kDisplayMode = DisplayMode::kPrefix;

    constexpr auto operator()(auto... args) const {
        // implementation
    }
};
```

## Complete Example

### Old Implementation

```cpp
#include "symbolic/symbolic.h"
#include "symbolic/operators.h"
#include "symbolic/simplify.h"

using namespace tempura::symbolic;

int main() {
    TEMPURA_SYMBOLS(x, y);

    auto expr = x * x + 2_c * x + y;
    auto simplified = simplify(expr);

    auto result = expr(Substitution{x = 5, y = 3});
    // result == 38
}
```

### New Implementation

```cpp
#include "symbolic2/symbolic.h"

using namespace tempura;

int main() {
    Symbol x;
    Symbol y;

    auto expr = x * x + 2_c * x + y;
    auto simplified = simplify(expr);

    auto result = evaluate(expr, BinderPack{x = 5, y = 3});
    // result == 38
}
```

## Breaking Changes

### 1. Symbol Identity

**Old:** Symbols with same label but different IDs are different

```cpp
Symbol<"x", 0> x1;
Symbol<"x", 1> x2;  // Different symbol!
```

**New:** Each `Symbol x;` declaration creates unique type

```cpp
Symbol x1;  // Type: Symbol<decltype([]{})>
Symbol x2;  // Type: Symbol<decltype([]{})>  // Different lambda!
// x1 and x2 are ALWAYS different types
```

**Impact:** If you were relying on symbol labels for identity, you need to use the same symbol variable instead.

### 2. Expression Accessors

**Old:**

```cpp
expr.left();   // Member function
expr.right();  // Member function
expr.operand(); // Member function
```

**New:**

```cpp
left(expr);    // Free function
right(expr);   // Free function
operand(expr); // Free function
```

### 3. Type Names

The underlying type names are different:

- Old: `SymbolicExpression<Plus, Symbol<"x", 0>, Symbol<"y", 1>>`
- New: `Expression<AddOp, Symbol<lambda_1>, Symbol<lambda_2>>`

**Impact:** Don't rely on specific type names in template specializations.

### 4. Matcher Wildcards

**Old:**

- `Any{}` - matches anything
- `AnyConstant{}` - matches any constant
- `AnySymbol{}` - matches any symbol
- `AnyConstantExpr{}` - matches constant expressions
- `AnyOp{}` - matches any operator

**New:**

- `AnyArg{}` - matches any argument
- `AnyConstant{}` - matches any constant ✓ same
- `AnySymbol{}` - matches any symbol ✓ same
- `AnyExpr{}` - matches any expression
- `Never{}` - never matches (opposite of Any)

### 5. Ordering

**Old:** Uses `typeName()` string comparison for canonical forms

```cpp
// Order determined by stringified type names
```

**New:** Uses compile-time type ID counter

```cpp
// Order determined by kMeta<T> values
// Earlier declared types have lower IDs
```

**Impact:** Canonical forms may differ, but behavior is more consistent.

## Advanced Features

### TypeList → Direct Pattern Matching

**Old:** Used TypeList for term manipulation

```cpp
auto terms = flatten(Plus{}, expr.terms());
auto sorted = sort<AdditionCmp>(terms);
```

**New:** Built-in simplification handles this

```cpp
auto simplified = simplify(expr);  // Automatic sorting and merging
```

### Derivative (Coming Soon)

The old `derivative.h` is being ported. Syntax will remain similar:

```cpp
// Old & New (same)
auto df_dx = derivative(expr, x);
```

## Performance Considerations

### Compile-Time Performance

The new implementation generally compiles faster because:

- Single header vs multiple includes
- Type ID comparison vs string comparison
- Simpler template instantiations
- No preprocessor macro expansion

### Runtime Performance

Both implementations have **zero runtime cost** - everything is compile-time.

## Testing Your Migration

1. **Side-by-side comparison**: Keep old code, create new version, verify same results
2. **Test evaluation**: Ensure numeric results match
3. **Test simplification**: Canonical forms may differ but should be equivalent
4. **Test custom operators**: Verify custom operators still work

## Common Pitfalls

### ❌ Reusing Symbol Variables

```cpp
// DON'T
Symbol x;
Symbol x;  // Error: redefinition!
```

### ❌ Wrong Evaluation Syntax

```cpp
Symbol x;
auto expr = x + 1_c;

// DON'T
auto result = expr(x = 5);  // Wrong!

// DO
auto result = evaluate(expr, BinderPack{x = 5});
```

### ❌ Mixing Old and New

```cpp
// DON'T mix includes
#include "symbolic/symbolic.h"
#include "symbolic2/symbolic.h"  // Conflict!
```

## Getting Help

- Check the examples in `symbolic2/examples/`
- Read the tests in `symbolic2/test/`
- See `symbolic2/README.md` for feature documentation

## Reporting Issues

If you encounter issues during migration:

1. Check this guide first
2. Look at the examples
3. File an issue with:
   - Old code that worked
   - New code that doesn't work
   - Error message or unexpected behavior
