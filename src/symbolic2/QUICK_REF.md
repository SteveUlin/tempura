# Symbolic2 Quick Reference

## Symbol Creation

```cpp
Symbol x;  // Unique symbol
Symbol y;
Symbol z;
```

## Constants

```cpp
auto c1 = 5_c;           // Constant<5>
auto c2 = 3.14_c;        // Constant<3.14>
constexpr auto pi = π;   // Special constant π
constexpr auto e_const = e;  // Special constant e
```

## Building Expressions

```cpp
auto expr1 = x + y;           // Addition
auto expr2 = x * y;           // Multiplication
auto expr3 = x - y;           // Subtraction
auto expr4 = x / y;           // Division
auto expr5 = pow(x, 2_c);     // Power
auto expr6 = sin(x);          // Sine
auto expr7 = cos(x);          // Cosine
auto expr8 = log(x);          // Natural logarithm
auto expr9 = exp(x);          // Exponential
```

## Evaluation

```cpp
Symbol x, y;
auto expr = x + y;

// Evaluate with specific values
constexpr auto result = evaluate(expr, BinderPack{x = 5, y = 3});
static_assert(result == 8);

// Multiple variables
auto expr2 = x * x + 2_c * y;
constexpr auto result2 = evaluate(expr2, BinderPack{x = 3, y = 4});
static_assert(result2 == 17);  // 3*3 + 2*4 = 9 + 8 = 17
```

## Pattern Matching

```cpp
Symbol x, y;
auto expr = x + y;

// Match any arguments
static_assert(match(expr, AnyArg{} + AnyArg{}));

// Match specific types
static_assert(match(expr, AnySymbol{} + AnySymbol{}));
static_assert(match(x + 1_c, AnySymbol{} + AnyConstant{}));

// Match expressions
static_assert(match(sin(x), sin(AnyArg{})));
static_assert(!match(sin(x), cos(AnyArg{})));

// Access parts
constexpr auto l = left(expr);   // Gets x
constexpr auto r = right(expr);  // Gets y
```

## Simplification

```cpp
Symbol x;

// Identity simplifications
auto s1 = simplify(x + 0_c);     // → x
auto s2 = simplify(x * 1_c);     // → x
auto s3 = simplify(x * 0_c);     // → 0
auto s4 = simplify(pow(x, 1_c)); // → x
auto s5 = simplify(pow(x, 0_c)); // → 1

// Constant folding
auto s6 = simplify(2_c + 3_c);   // → 5
auto s7 = simplify(10_c * 5_c);  // → 50

// Algebraic simplification
auto s8 = simplify(x + x);       // → 2 * x (or x * 2)
```

## Wildcards

### AnyArg

Matches any single argument (Symbol or Constant or Expression)

```cpp
match(x, AnyArg{});        // true
match(5_c, AnyArg{});      // true
match(x + y, AnyArg{});    // true
```

### AnyExpr

Matches any Expression (not Symbol or Constant)

```cpp
match(x + y, AnyExpr{});   // true
match(x, AnyExpr{});       // false
match(5_c, AnyExpr{});     // false
```

### AnySymbol

Matches any Symbol

```cpp
match(x, AnySymbol{});     // true
match(5_c, AnySymbol{});   // false
```

### AnyConstant

Matches any Constant

```cpp
match(5_c, AnyConstant{}); // true
match(x, AnyConstant{});   // false
```

## Common Patterns

### Quadratic Expression

```cpp
Symbol x;
auto expr = x * x + 2_c * x + 1_c;  // x² + 2x + 1

constexpr auto at_5 = evaluate(expr, BinderPack{x = 5});
static_assert(at_5 == 36);  // 25 + 10 + 1 = 36
```

### Checking Expression Structure

```cpp
Symbol x;
auto expr = sin(x + 1_c);

// Check if it's sin of something
static_assert(match(expr, sin(AnyArg{})));

// Check if the argument is an addition
constexpr auto arg = left(expr);  // Gets x + 1_c
static_assert(match(arg, AnyArg{} + AnyArg{}));
```

### Transforming Expressions

```cpp
Symbol x;
auto expr = x - 1_c;

// Subtraction is converted to addition
auto s = simplify(expr);  // → x + (-1) * 1_c
```

## Known Limitations

⚠️ **Complex simplifications may exceed template depth**

Avoid in compile-time code:

- Deep distribution: `(x+1)*(x+1)*(x+1)*...`
- Transcendental with constants: `log(1)`, `exp(log(x))`
- Very long expression chains

**Workaround**: Use `-ftemplate-depth=2000` compiler flag or simplify incrementally.

## Tips

1. **Always use `constexpr auto`** for expressions you want to evaluate at compile-time
2. **Use `static_assert`** to verify compile-time results
3. **Pattern matching is powerful** - combine wildcards for complex patterns
4. **Simplification is automatic** - expressions simplify when constructed
5. **Each Symbol is unique** - even if they have the same name in source

## Example: Complete Workflow

```cpp
#include "symbolic2/symbolic.h"
using namespace tempura;

int main() {
  // 1. Create symbols
  Symbol x, y;

  // 2. Build expression
  auto expr = x * x + 2_c * x + 1_c;

  // 3. Simplify (happens automatically)
  auto simplified = simplify(expr);

  // 4. Match pattern
  static_assert(match(simplified, AnyExpr{}));

  // 5. Evaluate
  constexpr auto result = evaluate(simplified, BinderPack{x = 5});
  static_assert(result == 36);

  return 0;
}
```

## Useful Static Assertions

```cpp
// Verify expression type
static_assert(Symbolic<decltype(x + y)>);

// Verify simplification
auto s = simplify(x + 0_c);
static_assert(match(s, x));

// Verify evaluation
constexpr auto val = evaluate(x + 1_c, BinderPack{x = 5});
static_assert(val == 6);

// Verify pattern matching
static_assert(match(sin(x), sin(AnyArg{})));
```

---

For more examples, see `examples/basic_usage.cpp`
For detailed documentation, see `README.md`
For migration from v1, see `MIGRATION.md`
