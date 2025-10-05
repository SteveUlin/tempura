# How to Add New Differentiation Rules

## Before RecursiveRewriteSystem (Old Way)

To add a new differentiation rule, you needed to:

1. Write a template function with `requires` clause
2. Use accessor functions to extract subexpressions
3. Manually call `diff()` recursively
4. Understand C++ template metaprogramming

**Example: Adding hyperbolic sine (sinh) rule**

```cpp
// Hyperbolic sine: d/dx(sinh(f)) = cosh(f) * df/dx
template <Symbolic Expr, Symbolic Var>
  requires(match(Expr{}, sinh(AnyArg{})))
constexpr auto diff(Expr expr, Var var) {
  constexpr auto f = operand(expr);
  return cosh(f) * diff(f, var);
}
```

**Problems:**

- Requires understanding template metaprogramming
- Pattern is hidden in the `requires` clause
- Need to know about `operand()` accessor
- Verbose and hard to read
- Easy to make mistakes with template syntax

## After RecursiveRewriteSystem (New Way)

To add a new differentiation rule, you just:

1. Define a `RecursiveRewrite` with pattern and lambda
2. Add it to the `DiffRules` system
3. Done!

**Example: Adding hyperbolic sine (sinh) rule**

### Step 1: Define the rule

In `derivative.h`, add:

```cpp
// Hyperbolic sine: d/dx(sinh(f)) = cosh(f) * df/dx
constexpr auto DiffSinh = RecursiveRewrite{
    sinh(x_),
    [](auto ctx, auto diff_fn, auto var) {
      constexpr auto f = get(ctx, x_);
      return cosh(f) * diff_fn(f, var);
    }};
```

### Step 2: Add to the system

Update the `DiffRules` definition:

```cpp
constexpr auto DiffRules = RecursiveRewriteSystem{
    DiffSum,    DiffDifference, DiffNegation, DiffProduct, DiffQuotient,
    DiffPower,  DiffSqrt,       DiffExp,      DiffLog,     DiffSin,
    DiffCos,    DiffTan,        DiffAsin,     DiffAcos,    DiffAtan,
    DiffSinh};  // <-- Just add it here!
```

### Step 3: Test it!

```cpp
"Derivative of sinh(x)"_test = [] {
  Symbol x;
  auto expr = sinh(x);
  auto d = diff(expr, x);
  static_assert(match(d, cosh(x) * 1_c));
};
```

**That's it!**

## Benefits

### 1. No Template Expertise Needed

- Just pattern + lambda
- No `requires` clauses
- No template syntax to get wrong

### 2. Self-Documenting

```cpp
DiffSinh = RecursiveRewrite{ sinh(x_), ... }
//                           ^^^^^^^^
//                           Pattern is obvious!
```

### 3. Follows Mathematical Notation

```cpp
// Mathematical notation:
// d/dx(sinh(f)) = cosh(f) * df/dx

// Code notation (almost identical!):
return cosh(f) * diff_fn(f, var);
```

### 4. Easy to Test in Isolation

Each rule is a standalone object that can be tested independently:

```cpp
"Test DiffSinh rule"_test = [] {
  Symbol x;
  auto expr = sinh(x);
  auto result = DiffSinh.apply(expr, diff, x);
  static_assert(match(result, cosh(x) * 1_c));
};
```

## More Examples

### Adding Second Derivative Rules

Want to add `d/dx(d/dx(f))` as a shorthand?

```cpp
constexpr auto DiffSecondDerivative = RecursiveRewrite{
    diff(f_, x_),  // Pattern: diff(f, x)
    [](auto ctx, auto diff_fn, auto var) {
      constexpr auto f = get(ctx, f_);
      constexpr auto x = get(ctx, x_);
      // Take derivative of f with respect to x, then take derivative again
      auto first = diff_fn(f, x);
      return diff_fn(first, x);
    }};
```

### Adding Parametric Differentiation

Want to handle `d/dt(f(t), t)` with explicit variable?

```cpp
constexpr auto DiffParametric = RecursiveRewrite{
    diff(f_, t_),  // Pattern matches diff(expr, var)
    [](auto ctx, auto diff_fn, auto var) {
      constexpr auto f = get(ctx, f_);
      constexpr auto t = get(ctx, t_);
      return diff_fn(f, t);  // Apply differentiation
    }};
```

### Adding Conditional Rules with Predicates

Want to only differentiate if variable appears in expression?

```cpp
constexpr auto DiffConditional = RecursiveRewrite{
    f_,
    [](auto ctx, auto diff_fn, auto var) {
      constexpr auto f = get(ctx, f_);
      // Only differentiate if f contains var
      if constexpr (contains(f, var)) {
        return diff_fn(f, var);
      } else {
        return 0_c;
      }
    },
    // Predicate: only match if expression contains the variable
    [](auto ctx) {
      constexpr auto f = get(ctx, f_);
      constexpr auto var = /* get var somehow */;
      return contains(f, var);
    }};
```

## Comparison Table

| Task                        | Old Way     | New Way    |
| --------------------------- | ----------- | ---------- |
| **Add new rule**            | ~10 lines   | ~6 lines   |
| **Template expertise**      | Required    | Not needed |
| **Pattern visibility**      | Hidden      | Explicit   |
| **Mathematical similarity** | Low         | High       |
| **Compile-time safety**     | Full        | Full       |
| **Runtime overhead**        | Zero        | Zero       |
| **Time to implement**       | 10+ minutes | 2 minutes  |
| **Chance of mistakes**      | Moderate    | Low        |

## Summary

With RecursiveRewriteSystem, adding new differentiation rules is:

✅ **Fast** - Just pattern + lambda
✅ **Easy** - No template metaprogramming
✅ **Clear** - Self-documenting code
✅ **Safe** - Compile-time type checking
✅ **Efficient** - Zero runtime overhead

Rule writing is now accessible to anyone who understands:

- Basic C++ lambdas
- The mathematical transformation they want to implement

No template metaprogramming expertise required!
