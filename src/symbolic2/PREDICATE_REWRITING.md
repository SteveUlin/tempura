# Predicate-Based Rewriting in Pattern Matching

## Overview

The pattern matching rewrite system now supports optional predicates that allow conditional transformations based on properties of matched pattern variables. This enables expressing complex rewriting rules like canonical ordering, associative reordering, and conditional simplifications.

## Motivation

Many simplification rules require checking conditions on matched expressions before applying a transformation. For example:

- **Canonical ordering**: Reorder `x + y` to `y + x` only if `y < x` to maintain a consistent ordering
- **Associative reordering**: Transform `(a + c) + b` to `(a + b) + c` only if `b < c`
- **Type-based rules**: Apply transformations only when matched values are symbols (not constants)

Previously, these rules had to be written with manual `if constexpr` checks. With predicates, they can be expressed declaratively within the rewrite system.

## Usage

### Basic Syntax

```cpp
// Without predicate (always applies if pattern matches)
constexpr auto rule1 = Rewrite{pow(x_, 0_c), 1_c};

// With predicate (applies only if predicate returns true)
constexpr auto rule2 = Rewrite{
  x_ + y_,                    // Pattern
  y_ + x_,                    // Replacement
  [](auto ctx) {              // Predicate
    return symbolicLessThan(get(ctx, y_), get(ctx, x_));
  }
};
```

### Accessing Matched Values

Use the `get(ctx, pattern_var)` helper function to retrieve the expression bound to a pattern variable:

```cpp
constexpr auto rule = Rewrite{
  x_ * y_,
  y_ * x_,
  [](auto ctx) {
    constexpr auto x_val = get(ctx, x_);
    constexpr auto y_val = get(ctx, y_);
    return symbolicLessThan(y_val, x_val);
  }
};
```

### Complex Predicates

Predicates can combine multiple conditions:

```cpp
// Reorder multiplication only for non-constant symbols
constexpr auto rule = Rewrite{
  x_ * y_,
  y_ * x_,
  [](auto ctx) {
    constexpr auto x_val = get(ctx, x_);
    constexpr auto y_val = get(ctx, y_);
    return symbolicLessThan(y_val, x_val) &&
           !match(x_val, AnyConstant{}) &&
           !match(y_val, AnyConstant{});
  }
};
```

### In RewriteSystem

Predicates work seamlessly with `RewriteSystem`:

```cpp
constexpr auto AdditionRules = RewriteSystem{
  // Identity rules (no predicates)
  Rewrite{0_c + x_, x_},
  Rewrite{x_ + 0_c, x_},

  // Ordering rule (with predicate)
  Rewrite{x_ + y_, y_ + x_, [](auto ctx) {
    return symbolicLessThan(get(ctx, y_), get(ctx, x_));
  }}
};
```

## Examples

### Canonical Addition Ordering

```cpp
constexpr auto CanonicalAdd = Rewrite{
  x_ + y_,
  y_ + x_,
  [](auto ctx) {
    return symbolicLessThan(get(ctx, y_), get(ctx, x_));
  }
};

// b + a → a + b (since a < b)
constexpr auto expr1 = b + a;
constexpr auto result1 = CanonicalAdd.apply(expr1);  // a + b

// a + b → a + b (already ordered)
constexpr auto expr2 = a + b;
constexpr auto result2 = CanonicalAdd.apply(expr2);  // a + b (unchanged)
```

### Canonical Multiplication Ordering

```cpp
constexpr auto CanonicalMul = Rewrite{
  x_ * y_,
  y_ * x_,
  [](auto ctx) {
    return symbolicLessThan(get(ctx, y_), get(ctx, x_));
  }
};

// b * a → a * b
constexpr auto expr = b * a;
constexpr auto result = CanonicalMul.apply(expr);  // a * b
```

### Associative Reordering

```cpp
// Reorder (a + c) + b → (a + b) + c when b < c
constexpr auto AssocReorder = Rewrite{
  (a_ + c_) + b_,
  (a_ + b_) + c_,
  [](auto ctx) {
    return symbolicLessThan(get(ctx, b_), get(ctx, c_));
  }
};

// (a + c) + b → (a + b) + c (since b < c)
constexpr auto expr = (a + c) + b;
constexpr auto result = AssocReorder.apply(expr);  // (a + b) + c
```

## Implementation Details

### NoPredicate Type

Rules without predicates use a `NoPredicate` type that always returns `true`:

```cpp
struct NoPredicate {
  template <typename Context>
  static constexpr bool operator()(Context) { return true; }
};
```

### Rewrite Template

The `Rewrite` template now accepts an optional third template parameter for the predicate:

```cpp
template <typename Pattern, typename Replacement, typename Predicate = NoPredicate>
struct Rewrite {
  Pattern pattern;
  Replacement replacement;
  [[no_unique_address]] Predicate predicate = {};

  // ... implementation
};
```

The `[[no_unique_address]]` attribute ensures that rules without custom predicates have zero overhead.

### Predicate Evaluation

Predicates are evaluated during both the `matches` check and the `apply` operation:

1. **During `matches()`**: The predicate is evaluated to determine if the rule should be considered for application
2. **During `apply()`**: The predicate is re-evaluated before performing the substitution

This ensures correctness in both sequential rule matching and direct rule application.

### BindingContext

The predicate receives a `BindingContext` containing all matched pattern variables. The `get()` helper function provides convenient access:

```cpp
template <typename Unique, typename... Entries>
constexpr auto get(detail::BindingContext<Entries...> ctx, PatternVar<Unique>) {
  return ctx.template lookup<PatternVar<Unique>::id>();
}
```

## Benefits

1. **Declarative**: Rules express both pattern and condition in one place
2. **Composable**: Predicates work with `RewriteSystem` for sequential rule application
3. **Zero overhead**: Rules without predicates have no runtime or compile-time cost
4. **Type-safe**: Compile-time evaluation ensures correctness
5. **Readable**: Self-documenting rules make code easier to understand

## Migration Guide

### Before (Manual Conditionals)

```cpp
template <Symbolic S>
  requires(match(S{}, AnyArg{} + AnyArg{}))
constexpr auto additionIdentities(S expr) {
  // ... identity rules ...

  // Canonical ordering: smaller term first
  else if constexpr (symbolicLessThan(right(expr), left(expr))) {
    return right(expr) + left(expr);
  }

  else {
    return expr;
  }
}
```

### After (With Predicates)

```cpp
constexpr auto AdditionRules = RewriteSystem{
  Rewrite{0_c + x_, x_},
  Rewrite{x_ + 0_c, x_},
  Rewrite{x_ + y_, y_ + x_, [](auto ctx) {
    return symbolicLessThan(get(ctx, y_), get(ctx, x_));
  }}
};

template <Symbolic S>
  requires(match(S{}, AnyArg{} + AnyArg{}))
constexpr auto additionIdentities(S expr) {
  return AdditionRules.apply(expr);
}
```

## Performance

Predicates are evaluated at compile-time using `constexpr` evaluation. There is:

- **Zero runtime overhead**: All predicate evaluation happens at compile-time
- **No size overhead**: The `[[no_unique_address]]` attribute ensures `NoPredicate` occupies no space
- **Fast compilation**: Predicates are only evaluated when their pattern matches

## See Also

- `pattern_matching.h` - Core pattern matching implementation
- `ordering.h` - `symbolicLessThan()` and ordering functions
- `simplify.h` - Uses of predicate-based rewriting for simplification
- `test/pattern_matching_predicate_test.cpp` - Comprehensive test examples
