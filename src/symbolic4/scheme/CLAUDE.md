# scheme/ - Recursion Schemes for Expression Traversal

## Purpose

This directory provides composable traversal patterns for expression trees, built on category-theoretic recursion schemes.

## Key Files

| File | Purpose |
|------|---------|
| `cata.h` | Unified catamorphism/paramorphism primitive |
| `fold.h` | Bottom-up traversal (catamorphism) |
| `para.h` | Traversal with access to originals (paramorphism) |
| `fold_unique.h` | DAG-aware traversal with caching |
| `transform.h` | Structure-preserving transformations |

## Core Concept: Catamorphism

A catamorphism "folds" a recursive structure bottom-up:
1. Process leaves (terminals)
2. Recursively process children
3. Combine results at each node

```
    +           fold           10
   / \         ----->    (= 3 + 7)
  3   *
     / \
    7   1
```

## cata.h - The Foundation

All schemes reduce to `cata<I, Mode>`:

```cpp
template <typename I, ChildMode Mode, Symbolic E, typename... Ctx>
constexpr auto cata(E expr, Ctx&... ctx);
```

- `I` - Interpreter defining `terminal()` and `combine()`
- `Mode` - `ResultOnly` (fold) or `WithOriginal` (para)
- `E` - Expression to traverse
- `Ctx` - Optional context passed through

## fold.h - Results Only

Standard catamorphism returning only computed results:

```cpp
struct SumConstants {
  using result_type = int;

  template <typename T>
  static constexpr auto terminal(T t) -> int {
    if constexpr (is_constant_v<T>) return T::value;
    return 0;
  }

  template <typename Op, typename... Rs>
  static constexpr auto combine(Rs... results) -> int {
    return (results + ...);
  }
};

int sum = fold<SumConstants>(expr);
```

Used by: `eval.h`, `simplify.h`, `to_string.h`

## para.h - With Originals

Paramorphism provides `Pair{original, result}` for each child:

```cpp
template <typename Op, typename... Pairs>
static constexpr auto combine(Pair<Orig1, Res1> p1, Pair<Orig2, Res2> p2) {
  // Can use both p1.first (original expr) and p1.second (recursive result)
}
```

Essential for:
- **Differentiation**: Product rule needs `f*dg + df*g` (original f, g)
- **Simplification**: Pattern matching on structure

Used by: `diff.h`

## fold_unique.h - DAG Awareness

Handles shared subexpressions without recomputation:

```cpp
TEMPURA_LET(t, x * x);
auto expr = t.sym() + t.sym() + t.sym();  // t appears 3 times
```

Without `foldUnique`, `x*x` would be processed 3 times. With it:
1. First visit to `t`: compute and cache
2. Subsequent visits: return cached value via `I::identity()`

Returns `std::pair{result, updated_visited_set}` for functional threading:

```cpp
template <typename I, Symbolic E, typename Visited = IdSet<>>
constexpr auto foldUnique(E expr, Visited visited = {});
```

Interpreter interface:
```cpp
struct MyInterpreter {
  // ... terminal, combine ...

  template <typename Visited>
  static constexpr auto identity(Visited) -> result_type {
    return ...;  // Value for re-visited nodes
  }

  template <typename Visited, typename Let, typename R>
  static constexpr auto on_let(Visited, Let, R result) -> result_type {
    return result;  // Optional: intercept first LetNode visit
  }
};
```

## IdSet - Compile-Time Visited Tracking

```cpp
template <typename... Ids>
struct IdSet { static constexpr SizeT size = sizeof...(Ids); };

id_set_contains_v<X, Set>;  // Check membership
id_set_insert_t<X, Set>;    // Insert (idempotent)
```

Used to track visited LetNode identities at compile time.

## LetNode - Identity Wrapper

Wraps an expression with a unique identity for DAG support:

```cpp
template <typename Id, Symbolic E>
struct LetNode : SymbolicTag {
  E expr_;
  static constexpr auto sym() { return Symbol<Id>{}; }
};

// Macro for convenience:
TEMPURA_LET(squared, x * x);  // squared.sym() can be used multiple times
```

## Interpreter Contract

All interpreters must provide:

```cpp
struct Interpreter {
  using result_type = ...;  // What the traversal produces

  // Handle leaf nodes
  template <typename T>
  static constexpr auto terminal(T term) -> result_type;
  // Or with context: terminal(T term, Ctx& ctx)

  // Combine child results (fold mode)
  template <typename Op, typename... Results>
  static constexpr auto combine(Results... children) -> result_type;
  // Or with context: combine(Ctx& ctx, Results... children)

  // For para mode, children are Pair<Original, Result>
};
```

## Design Principles

1. **Unified foundation**: All traversals reduce to `cata` with mode parameter
2. **Compile-time computation**: Traversal happens at compile time for constant expressions
3. **No heap allocation**: Everything on stack via template recursion
4. **Composable**: Interpreters are independent; combine via multiple passes
5. **Type-safe**: Results carry type information through traversal
