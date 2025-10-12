# Symbolic3 Debugging Guide

Quick reference for debugging symbolic3 expressions and transformations.

## Compile-Time Debugging

### Using `debug.h` Utilities

```cpp
#include "symbolic3/debug.h"
#include "symbolic3/symbolic3.h"

using namespace tempura::symbolic3;

constexpr Symbol x;
constexpr auto expr = x*x + 2_c*x + 1_c;

// Check expression properties
static_assert(expression_depth(expr) == 2);
static_assert(operation_count(expr) == 4);
static_assert(is_likely_simplified(expr));

// Force compiler to display type (causes error)
force_type_display(expr);
```

### Expression Properties

**`expression_depth(expr)`** - Maximum tree depth

- Leaf nodes (symbols, constants): 0
- Single operation: 1
- Nested operations: increases with nesting

**`operation_count(expr)`** - Total number of operations

- Counts all operator applications in expression tree

**`is_likely_simplified(expr)`** - Heuristic check for simplification

- Checks for obvious non-simplified patterns (x+0, x\*1, etc.)
- Not comprehensive but catches common cases

**`force_type_display(expr)`** - Shows type in compiler error

- Useful for understanding expression structure
- Will cause compilation to fail

### Verifying Transformations

```cpp
// Verify simplification worked
constexpr auto before = x + Constant<0>{};
constexpr auto after = simplify(before, default_context());
static_assert(!std::is_same_v<decltype(before), decltype(after)>);
static_assert(is_likely_simplified(after));

// Compare expression structures
static_assert(match(after, x_));  // Verify it matches pattern
```

## Runtime Debugging

### String Conversion

```cpp
#include "symbolic3/to_string.h"

auto expr = x*x + 2_c*x + 1_c;

// Compile-time string (for simple expressions)
constexpr auto str = toString(expr);

// Runtime string (for complex expressions or runtime values)
std::string str_runtime = toStringRuntime(expr);
std::print("Expression: {}\n", str_runtime);
```

### Debug Executables

Several debug tools are available for interactive testing:

**Build and run:**

```bash
ninja -C build factoring_debug term_collecting_debug term_structure_debug

./build/src/symbolic3/test/factoring_debug
./build/src/symbolic3/test/term_collecting_debug
./build/src/symbolic3/test/term_structure_debug
```

**Purpose:**

- **`factoring_debug`** - Test factoring rules (`x*a + x*b → x*(a+b)`)
- **`term_collecting_debug`** - Test term collection across various patterns
- **`term_structure_debug`** - Test term ordering and canonical forms

These are development tools, not formal tests. Use them to explore simplification behavior interactively.

## Common Debugging Patterns

### Pattern Matching Issues

```cpp
// Check if pattern matches
constexpr auto expr = x + y;
static_assert(match(expr, x_ + y_));  // Should match

// Extract matched components
if constexpr (match(expr, x_ + y_)) {
  auto lhs = get_matched_arg<0>(expr);  // x
  auto rhs = get_matched_arg<1>(expr);  // y
}
```

### Simplification Not Working

```cpp
// 1. Check if expression is already simplified
static_assert(is_likely_simplified(expr));

// 2. Try individual rule sets
auto alg = algebraic_simplify(expr, ctx);
auto trig = transcendental_simplify(expr, ctx);
auto collected = collect_terms(expr, ctx);

// 3. Check traversal strategy
auto inner = innermost(algebraic_simplify).apply(expr, ctx);
auto outer = outermost(algebraic_simplify).apply(expr, ctx);

// 4. Use fixpoint explicitly
auto fixed = fixpoint(algebraic_simplify).apply(expr, ctx);
```

### Type Errors

```cpp
// Force compiler to show deduced type
template<typename T> struct ShowType;
ShowType<decltype(expr)> show;  // Error shows type

// Or use force_type_display from debug.h
force_type_display(expr);
```

## Performance Debugging

### Compile-Time Profiling

Use compiler flags to see template instantiation depth:

```bash
# GCC
g++ -ftime-report -ftemplate-depth=1024 ...

# Clang
clang++ -ftime-trace ...
```

### Measuring Expression Complexity

```cpp
constexpr auto simple = x + y;
constexpr auto complex = (x + y) * (x - y) + 2_c*x*y;

static_assert(operation_count(simple) == 1);
static_assert(operation_count(complex) == 9);  // More complex

static_assert(expression_depth(simple) == 1);
static_assert(expression_depth(complex) == 3);  // Deeper nesting
```

## Tips

1. **Use `static_assert` liberally** - Catch issues at compile-time
2. **Test incrementally** - Build complex expressions from simple ones
3. **Check intermediate results** - Don't just test final output
4. **Use debug tools** - The interactive debug executables are helpful
5. **Read compiler errors carefully** - Template errors show type structure
6. **Simplify first** - Test with simple expressions before complex ones

## See Also

- `src/symbolic3/test/debug_test.cpp` - Comprehensive test of debug utilities
- `examples/symbolic3_debug_demo.cpp` - Practical debugging examples
- `src/symbolic3/debug.h` - Full API documentation in comments
