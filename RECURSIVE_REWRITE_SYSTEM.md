# Recursive Pattern Matching for Differentiation

## Problem Statement

Differentiation rules inherently require recursive transformations:

```
d/dx(f + g) = d/dx(f) + d/dx(g)
              ^^^^^^^^   ^^^^^^^^
              Recursive calls to diff()
```

The standard `RewriteSystem` from `simplify.h` works great for simple pattern → replacement transformations (like `pow(x, 0) → 1`), but it cannot express recursive calls in the replacement expression.

## Solution: RecursiveRewriteSystem

I've implemented a new `RecursiveRewriteSystem` that allows replacement expressions to be **lambdas that can call back to the recursive function**.

### Architecture

#### 1. RecursiveRewrite

A rewrite rule that supports recursive transformations:

```cpp
template <typename Pattern, typename Replacement, typename Predicate = NoPredicate>
struct RecursiveRewrite {
  Pattern pattern;
  Replacement replacement;  // Can be a lambda!
  Predicate predicate = {};

  template <Symbolic S, typename RecursiveFn, typename... Args>
  static constexpr auto apply(S expr, RecursiveFn recursive_fn, Args... args);
};
```

The `Replacement` can be:

1. A symbolic expression (like regular Rewrite)
2. **A lambda that takes `(context, recursive_fn, ...args)` and returns an expression**

#### 2. RecursiveRewriteSystem

Applies a sequence of recursive rewrite rules:

```cpp
template <typename... Rules>
struct RecursiveRewriteSystem {
  template <std::size_t Index = 0, Symbolic S, typename RecursiveFn, typename... Args>
  static constexpr auto apply(S expr, RecursiveFn recursive_fn, Args... args);
};
```

## Usage Example: Differentiation

### BEFORE - Manual Template Specializations

The old approach required verbose template specializations:

```cpp
// Product rule: d/dx(f * g) = df/dx * g + f * dg/dx
template <Symbolic Expr, Symbolic Var>
  requires(match(Expr{}, AnyArg{} * AnyArg{}))
constexpr auto diff(Expr expr, Var var) {
  constexpr auto f = left(expr);
  constexpr auto g = right(expr);
  return diff(f, var) * g + f * diff(g, var);
}
```

Problems:

- Requires understanding C++ template metaprogramming
- Verbose `requires` clauses and accessor functions
- Each rule is a separate template function
- Hard to see the pattern at a glance

### AFTER - Declarative Recursive Rules

The new approach uses simple, declarative rules:

```cpp
// Product rule: d/dx(f * g) = df/dx * g + f * dg/dx
constexpr auto DiffProduct = RecursiveRewrite{
    x_ * y_,  // Pattern - clear and explicit!
    [](auto ctx, auto diff_fn, auto var) {
      constexpr auto f = get(ctx, x_);
      constexpr auto g = get(ctx, y_);
      return diff_fn(f, var) * g + f * diff_fn(g, var);
    }
};
```

Benefits:

- **Pattern is explicit**: `x_ * y_` clearly shows what we're matching
- **Transformation is clear**: The lambda shows the mathematical formula
- **Self-contained**: Each rule is a single, complete definition
- **Easy to understand**: Matches the mathematical notation

### Complete Example

```cpp
// Define all differentiation rules
constexpr auto DiffSum = RecursiveRewrite{
    x_ + y_,
    [](auto ctx, auto diff_fn, auto var) {
      return diff_fn(get(ctx, x_), var) + diff_fn(get(ctx, y_), var);
    }
};

constexpr auto DiffProduct = RecursiveRewrite{
    x_ * y_,
    [](auto ctx, auto diff_fn, auto var) {
      constexpr auto f = get(ctx, x_);
      constexpr auto g = get(ctx, y_);
      return diff_fn(f, var) * g + f * diff_fn(g, var);
    }
};

constexpr auto DiffPower = RecursiveRewrite{
    pow(x_, y_),
    [](auto ctx, auto diff_fn, auto var) {
      constexpr auto f = get(ctx, x_);
      constexpr auto n = get(ctx, y_);
      return n * pow(f, n - 1_c) * diff_fn(f, var);
    }
};

// Create rewrite system
constexpr auto DiffRules = RecursiveRewriteSystem{
    DiffSum, DiffProduct, DiffPower
};

// Main diff function
template <Symbolic Expr, Symbolic Var>
constexpr auto diff(Expr expr, Var var) {
  // Base cases
  if constexpr (match(expr, var)) return 1_c;
  if constexpr (match(expr, AnySymbol{})) return 0_c;
  if constexpr (match(expr, AnyConstant{})) return 0_c;

  // Apply rules with recursive callback
  auto diff_fn = []<Symbolic E, Symbolic V>(E e, V v) { return diff(e, v); };
  return DiffRules.apply(expr, diff_fn, var);
}
```

## Implementation Details

### Lambda Replacement Detection

The system automatically detects whether the replacement is a lambda or a symbolic expression:

```cpp
if constexpr (requires { Replacement{}(bindings_ctx, recursive_fn, args...); }) {
  // Lambda replacement: call it with context and recursive function
  return Replacement{}(bindings_ctx, recursive_fn, args...);
} else {
  // Symbolic expression replacement: just substitute
  return substitute(Replacement{}, bindings_ctx);
}
```

This means RecursiveRewrite is **backward compatible** with regular Rewrite rules!

### Compile-Time Evaluation

Everything is `constexpr`:

- Pattern matching happens at compile time
- Lambda replacement functions are evaluated at compile time
- The recursive diff calls are all compile-time template instantiations
- Zero runtime overhead!

### Pattern Variables

Uses the standard pattern variables from `pattern_matching.h`:

- `x_`, `y_`, `z_` - for general expressions
- `a_`, `b_`, `c_` - for constants/coefficients
- `n_`, `m_`, `p_`, `q_` - for exponents/indices

## Comparison Table

| Aspect                 | Old Approach            | New Approach                 |
| ---------------------- | ----------------------- | ---------------------------- |
| **Syntax**             | Template specialization | Declarative rule             |
| **Pattern visibility** | Hidden in `requires`    | Explicit in first argument   |
| **Verbosity**          | ~8 lines per rule       | ~5 lines per rule            |
| **Understandability**  | Requires C++ expertise  | Readable by mathematicians   |
| **Composability**      | Each rule is isolated   | Rules collected in system    |
| **Testing**            | Test each overload      | Test the rule system         |
| **Debugging**          | Template error messages | Clear pattern match failures |

## Benefits for Rule Writers

### 1. Lower Barrier to Entry

You don't need to understand:

- C++ template metaprogramming
- `requires` clauses
- Accessor functions like `left()`, `right()`, `operand()`

You just need to understand:

- Pattern matching (which is intuitive)
- Lambda syntax (which is standard C++)
- The mathematical transformation

### 2. Self-Documenting Code

Compare:

```cpp
// Old: What pattern does this match?
template <Symbolic Expr, Symbolic Var>
  requires(match(Expr{}, AnyArg{} * AnyArg{}))
constexpr auto diff(Expr expr, Var var) { ... }

// New: Pattern is obvious!
constexpr auto DiffProduct = RecursiveRewrite{ x_ * y_, ... };
```

### 3. Easy to Extend

Adding a new rule:

```cpp
// Just add one more RecursiveRewrite and add it to the system!
constexpr auto DiffMyRule = RecursiveRewrite{
    my_pattern,
    [](auto ctx, auto diff_fn, auto var) { return my_transformation; }
};

constexpr auto DiffRules = RecursiveRewriteSystem{
    DiffSum, DiffProduct, DiffPower, DiffMyRule  // <-- Add here
};
```

## Testing Results

All existing differentiation tests pass with the new approach:

- ✅ Basic derivatives (constants, variables)
- ✅ Arithmetic rules (sum, product, quotient)
- ✅ Power rule and chain rule
- ✅ Trigonometric functions
- ✅ Complex nested expressions

Both implementations produce identical results!

## Implementation Status

**Files Created:**

- `src/symbolic2/recursive_rewrite.h` - Core RecursiveRewrite system
- `src/symbolic2/derivative2.h` - Derivative implementation using RecursiveRewrite
- `src/symbolic2/test/recursive_rewrite_test.cpp` - Basic functionality tests
- `src/symbolic2/test/derivative_compare_test.cpp` - Comparison between old and new

**Next Steps:**

1. Decide whether to replace `derivative.h` with `derivative2.h`
2. Consider extending RecursiveRewrite to other recursive transformations
3. Add more documentation and examples

## Conclusion

The `RecursiveRewriteSystem` successfully solves the problem of expressing recursive pattern matching in a declarative, easy-to-understand way.

**For rule writers, this is a game-changer:**

- Write mathematical transformations that look like math
- No template metaprogramming expertise required
- Self-documenting, maintainable code
- Full compile-time evaluation and type safety

The complexity is hidden in the implementation (where it belongs), and the user API is clean and intuitive.
