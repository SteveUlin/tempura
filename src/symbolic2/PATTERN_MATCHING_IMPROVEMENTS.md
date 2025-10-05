# Pattern Matching Simplifications for symbolic2

This document demonstrates how the new pattern matching system in `pattern_matching.h` can be used to simplify and improve the code in `simplify.h` and provides notes on `derivative.h`.

## Summary of Improvements

### `simplify_v2.h` - Using RewriteSystem

The new pattern matching system allows us to replace verbose if-else chains with declarative `RewriteSystem` definitions. This makes the code more concise, readable, and maintainable.

#### Before (from `simplify.h`):

```cpp
template <Symbolic S>
  requires(match(S{}, pow(AnyArg{}, AnyArg{})))
constexpr auto powerIdentities(S expr) {
  constexpr auto base = left(expr);
  constexpr auto exponent = right(expr);

  // x ^ 0 == 1
  if constexpr (match(exponent, 0_c)) {
    return 1_c;
  }
  // x ^ 1 == x
  else if constexpr (match(exponent, 1_c)) {
    return base;
  }
  // 1 ^ x == 1
  else if constexpr (match(base, 1_c)) {
    return 1_c;
  }
  // 0 ^ x == 0
  else if constexpr (match(base, 0_c)) {
    return 0_c;
  }
  // (x ^ a) ^ b == x ^ (a * b)
  else if constexpr (match(expr, pow(pow(AnyArg{}, AnyArg{}), AnyArg{}))) {
    constexpr auto x = left(left(expr));
    constexpr auto a = right(left(expr));
    constexpr auto b = right(expr);
    return pow(x, a * b);
  }
  else {
    return expr;
  }
}
```

#### After (from `simplify_v2.h`):

```cpp
constexpr auto PowerRules = RewriteSystem{
  Rewrite{pow(x_, 0_c), 1_c},                    // x^0 → 1
  Rewrite{pow(x_, 1_c), x_},                     // x^1 → x
  Rewrite{pow(1_c, x_), 1_c},                    // 1^x → 1
  Rewrite{pow(0_c, x_), 0_c},                    // 0^x → 0
  Rewrite{pow(pow(x_, a_), b_), pow(x_, a_ * b_)} // (x^a)^b → x^(a*b)
};

template <Symbolic S>
  requires(match(S{}, pow(AnyArg{}, AnyArg{})))
constexpr auto powerIdentities(S expr) {
  return PowerRules.apply(expr);
}
```

**Benefits:**
- **70% less code** - The new version is dramatically shorter
- **Declarative** - Rules are stated as transformations, not as control flow
- **Self-documenting** - Each rule has a clear pattern → replacement structure
- **Easier to extend** - Adding new rules is just adding a line to the RewriteSystem
- **No manual binding extraction** - Pattern variables (x_, a_, b_) automatically bind

#### More Examples

**Logarithm identities:**

Before (multiple if-else statements):
```cpp
template <Symbolic S>
  requires(match(S{}, log(AnyArg{})))
constexpr auto logIdentities(S expr) {
  if constexpr (match(expr, log(1_c))) {
    return 0_c;
  } else if constexpr (match(expr, log(e))) {
    return 1_c;
  }
  else if constexpr (match(expr, log(pow(AnyArg{}, AnyArg{})))) {
    constexpr auto x = left(operand(expr));
    constexpr auto a = right(operand(expr));
    return a * log(x);
  }
  // ... more cases
}
```

After (clean RewriteSystem):
```cpp
constexpr auto LogRules = RewriteSystem{
  Rewrite{log(1_c), 0_c},                      // log(1) → 0
  Rewrite{log(e), 1_c},                        // log(e) → 1
  Rewrite{log(pow(x_, a_)), a_ * log(x_)},     // log(x^a) → a*log(x)
  Rewrite{log(x_ * y_), log(x_) + log(y_)},    // log(a*b) → log(a)+log(b)
  Rewrite{log(x_ / y_), log(x_) - log(y_)},    // log(a/b) → log(a)-log(b)
  Rewrite{log(exp(x_)), x_}                    // log(exp(x)) → x
};

template <Symbolic S>
  requires(match(S{}, log(AnyArg{})))
constexpr auto logIdentities(S expr) {
  return LogRules.apply(expr);
}
```

**Trigonometric identities:**

```cpp
constexpr auto SinRules = RewriteSystem{
  Rewrite{sin(π * 0.5_c), 1_c},               // sin(π/2) → 1
  Rewrite{sin(π), 0_c},                       // sin(π) → 0
  Rewrite{sin(π * 1.5_c), Constant<-1>{}}     // sin(3π/2) → -1
};

constexpr auto CosRules = RewriteSystem{
  Rewrite{cos(π * 0.5_c), 0_c},               // cos(π/2) → 0
  Rewrite{cos(π), Constant<-1>{}},            // cos(π) → -1
  Rewrite{cos(π * 1.5_c), 0_c}                // cos(3π/2) → 0
};

constexpr auto TanRules = RewriteSystem{
  Rewrite{tan(π), 0_c}                        // tan(π) → 0
};
```

## Why Pattern Matching Works Well for Simplification

1. **Structural Transformations**: Simplification rules are purely structural - they don't depend on external context
2. **First-Match Semantics**: `RewriteSystem` applies the first matching rule, which aligns with simplification priority
3. **Composability**: Rules can be easily added, removed, or reordered
4. **Type Safety**: All transformations are type-checked at compile time
5. **Zero Runtime Cost**: Everything happens at compile time, no runtime overhead

## Notes on `derivative.h`

The `derivative.h` file is **already well-structured** using template specialization with `requires` clauses. Pattern matching doesn't provide significant benefits here because:

1. **Context Dependency**: Differentiation requires knowing which variable we're differentiating with respect to, which is external context that pattern matching doesn't handle
2. **Already Clean**: The current template-based approach is clear and idiomatic C++
3. **Type-Based Dispatch**: The `requires` clauses already provide excellent pattern matching at the type level

The current approach in `derivative.h` is optimal for this use case:

```cpp
// Sum rule: d/dx(f + g) = df/dx + dg/dx
template <Symbolic Expr, Symbolic Var>
  requires(match(Expr{}, AnyArg{} + AnyArg{}))
constexpr auto diff(Expr expr, Var var) {
  return diff(left(expr), var) + diff(right(expr), var);
}
```

This is already clean, self-documenting, and efficient. Pattern matching would not improve it.

## Performance Characteristics

All pattern matching operations in `simplify_v2.h` happen at **compile time**:

- Pattern matching: Compile-time type checking
- Binding extraction: Compile-time template recursion  
- Substitution: Compile-time expression tree construction
- RewriteSystem application: Compile-time rule selection

**Zero runtime overhead** compared to the original implementation.

## Test Results

All tests in `simplify_v2_test.cpp` pass successfully:

```
=== Testing simplify_v2.h with Pattern Matching ===

Test 1: Power identities
  pow(x, 0) → 1 ✓
  pow(x, 1) → x ✓
  pow(1, x) → 1 ✓
  ✓ Power identities work!

Test 2: Addition identities
  0 + x → x ✓
  x + 0 → x ✓
  ✓ Addition identities work!

Test 3: Multiplication identities
  0 * x → 0 ✓
  1 * x → x ✓
  ✓ Multiplication identities work!

Test 4: Logarithm identities
  log(1) → 0 ✓
  log(e) → 1 ✓
  ✓ Logarithm identities work!

Test 5: Exp-log cancellation
  exp(log(x)) → x ✓
  ✓ Exp-log cancellation works!

Test 6: Trigonometric identities
  sin(π/2) → 1 ✓
  sin(π) → 0 ✓
  cos(π/2) → 0 ✓
  ✓ Trigonometric identities work!

Test 7: Constant folding
  2 + 3 → 5 ✓
  10 * 5 → 50 ✓
  ✓ Constant folding works!

=== All Tests Passed! ===
```

## Conclusion

The new pattern matching system provides significant benefits for **structural transformation** code like simplification rules. It makes the code:

- More concise (up to 70% reduction in lines of code)
- More declarative and readable
- Easier to maintain and extend
- Just as efficient (zero runtime overhead)

However, it's not a silver bullet - the existing template-based approaches in `derivative.h` remain optimal for their use case. Pattern matching shines when you have **purely structural transformations without external context**.
