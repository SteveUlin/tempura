# interpreter/ - Expression Evaluation and Transformation

## Purpose

Interpreters traverse expression trees to produce results: numeric values, derivatives, simplified expressions, or strings.

## Key Files

| File | Purpose |
|------|---------|
| `eval.h` | Numeric evaluation with bindings |
| `diff.h` | Symbolic differentiation (forward-mode) |
| `simplify.h` | Algebraic simplification |
| `to_string.h` | Expression rendering |
| `partial_eval.h` | Constant folding |

## Interpreter Pattern

All interpreters follow the fold/para pattern from `scheme/cata.h`:

```cpp
struct MyInterpreter {
  using result_type = ...;

  // Handle leaf nodes
  template <typename T>
  static constexpr auto terminal(T term, Context& ctx) -> result_type;

  // Combine child results
  template <typename Op, typename... ChildResults>
  static constexpr auto combine(Context& ctx, ChildResults... results) -> result_type;
};
```

## eval.h - Numeric Evaluation

Evaluates expressions with type-safe bindings:

```cpp
Symbol<struct X> x;
Symbol<struct Y> y;
auto expr = x * x + y;
double result = evaluate(expr, x = 3.0, y = 1.0);  // Returns 10.0
```

Key insight: Operators are callable functors, so `AddOp{}(3.0, 4.0)` returns `7.0`. The `combine` function just invokes the operator on child results.

Binding mechanism uses `BinderPack` with overloaded `operator[]`:
```cpp
auto bindings = BinderPack{x = 5.0, y = 3.0};
bindings[x];  // Type-based dispatch, returns 5.0
```

## diff.h - Symbolic Differentiation

Forward-mode automatic differentiation producing symbolic derivatives:

```cpp
auto expr = x * x;
auto deriv = diff(expr, x);  // Returns expression for 2*x
```

Uses **paramorphism** (para) to access both original subexpressions and their derivatives - essential for chain rule:

```cpp
// Product rule: d(f*g) = f*dg + df*g
template <>
struct Rule<MulOp, Pair<F, DF>, Pair<G, DG>> {
  static auto apply(...) {
    return f * dg + df * g;  // Need original f, g
  }
};
```

Derivative rules:
- `d/dx(x) = 1`, `d/dx(c) = 0`
- Sum: `d(f+g) = df + dg`
- Product: `d(f*g) = f*dg + df*g`
- Quotient: `d(f/g) = (df*g - f*dg) / g^2`
- Chain: `d(sin(f)) = cos(f) * df`
- Power: `d(f^g) = f^g * (dg*ln(f) + g*df/f)`

## simplify.h - Algebraic Simplification

Two-phase simplification:

**Phase 1: Structural (exact)**
- Identity: `x + 0 → x`, `x * 1 → x`
- Annihilation: `x * 0 → 0`
- Self-cancellation: `x - x → 0`, `x / x → 1`
- Double negation: `-(-x) → x`
- Inverse pairs: `exp(log(x)) → x`
- Trig at zero: `sin(0) → 0`, `cos(0) → 1`

**Phase 2: Partial evaluation (may introduce floats)**
- Ground expressions (no free symbols) are evaluated
- All-constant → `Constant<value>`
- With literals → `lit(value)`

```cpp
auto simplified = simplify(x + lit(0));  // Returns x (exact)
auto folded = simplifyFull(lit(2) + lit(3));  // Returns lit(5)
```

## to_string.h - Expression Rendering

Renders expressions with proper precedence and parenthesization:

```cpp
auto expr = x * (y + lit(1));
std::string s = toString(expr, name(x, "x"), name(y, "y"));
// Returns "x * (y + 1)"
```

Precedence levels:
- 50: Atomic (symbols, constants)
- 40: Unary (negation, sin, cos)
- 30: Power
- 20: Multiplication, division
- 10: Addition, subtraction

## Adding New Interpreters

1. Define interpreter struct following the pattern:
   ```cpp
   struct CountNodes {
     using result_type = int;

     template <typename T>
     static constexpr auto terminal(T, Context&) -> int { return 1; }

     template <typename Op, typename... Results>
     static constexpr auto combine(Context&, Results... counts) -> int {
       return 1 + (counts + ...);
     }
   };
   ```

2. Use via fold:
   ```cpp
   int count = fold<CountNodes>(expr);
   ```

For transformations needing original subexpressions (like diff), use para instead of fold.
