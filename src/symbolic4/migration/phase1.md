# Phase 1: Strategy Infrastructure

## Goal

Make the strategy system ReduceOver-aware and variadic so that ALL strategy
combinators (All, BottomUp, TopDown, Innermost, Outermost, Recursive) work
with indexed reductions automatically. No production code changes — this phase
is pure preparation.

## Prerequisites

None. This is the first phase.

## Files to Read First

- `src/symbolic4/strategy/combinator.h` — All<S>, traversals, composition
- `src/symbolic4/strategy/recursive.h` — Recursive<Rules>, substituteRecImpl
- `src/symbolic4/strategy/pattern.h` — match(), extractBindings(), substitute()
- `src/symbolic4/indexed/sum_over.h` — current SumOver definition
- `src/symbolic4/core.h` — Expression<Op, Args...>, SymbolicTag, is_terminal_v

## Tasks

### 1.1 Create ReduceOver<ReduceOp, DimTag, Expr>

**File to create:** `src/symbolic4/indexed/reduce_over.h`

Define the generalized reduction node:

```cpp
// Reduction operation traits
struct SumReduce {
  static constexpr double identity() { return 0.0; }
  static constexpr double combine(double a, double b) { return a + b; }
  static constexpr const char* symbol() { return "Sum"; }
};

struct ProdReduce {
  static constexpr double identity() { return 1.0; }
  static constexpr double combine(double a, double b) { return a * b; }
  static constexpr const char* symbol() { return "Prod"; }
};

struct MaxReduce {
  static constexpr double identity() { return -std::numeric_limits<double>::infinity(); }
  static constexpr double combine(double a, double b) { return std::max(a, b); }
  static constexpr const char* symbol() { return "Max"; }
};

struct LogSumExpReduce {
  static constexpr double identity() { return -std::numeric_limits<double>::infinity(); }
  static constexpr double combine(double a, double b) {
    double m = std::max(a, b);
    return m + std::log(std::exp(a - m) + std::exp(b - m));
  }
  static constexpr const char* symbol() { return "LogSumExp"; }
};

// The unified reduction node
template <typename ReduceOp, typename DimTag, Symbolic Expr>
struct ReduceOver : SymbolicTag {
  using reduce_op = ReduceOp;
  using dim_tag = DimTag;
  using expr_type = Expr;

  [[no_unique_address]] Expr expr_;

  constexpr ReduceOver() = default;
  constexpr explicit ReduceOver(Expr e) : expr_{e} {}

  constexpr auto expr() const { return expr_; }

  template <Symbolic NewExpr>
  static constexpr auto rebuild(NewExpr new_expr) {
    return ReduceOver<ReduceOp, DimTag, NewExpr>{new_expr};
  }
};

// Type traits
template <typename T>
struct IsReduceOver : std::false_type {};

template <typename ROp, typename DimTag, typename Expr>
struct IsReduceOver<ReduceOver<ROp, DimTag, Expr>> : std::true_type {};

template <typename T>
constexpr bool is_reduce_over_v = IsReduceOver<T>::value;

// Backwards-compatible alias
template <typename DimTag, Symbolic Expr>
using SumOver = ReduceOver<SumReduce, DimTag, Expr>;

// Factory functions
template <typename DimTag, Symbolic Expr>
constexpr auto sumOver(Expr expr) {
  return ReduceOver<SumReduce, DimTag, Expr>{expr};
}

template <typename DimTag, Symbolic Expr>
constexpr auto prodOver(Expr expr) {
  return ReduceOver<ProdReduce, DimTag, Expr>{expr};
}
```

**Keep `sum_over.h` temporarily** with `#include "reduce_over.h"` and deprecation comments.
Ensure the `SumOver` alias is binary-compatible — existing code must compile unchanged.

Copy the operator overloads from `sum_over.h` but generalize them to work with
any `ReduceOver`, not just SumOver. Add the missing operators (`*`, `-`, `/`)
that `sum_over.h` was missing (currently only `+` is defined).

### 1.2 Make All<S> ReduceOver-aware and variadic

**File to modify:** `src/symbolic4/strategy/combinator.h`

Replace the hardcoded arity-0/1/2/3 `applyToChildren` overloads with:

```cpp
template <typename S>
struct All {
  [[no_unique_address]] S s;

  template <Symbolic E>
  constexpr auto apply(E e) const {
    if constexpr (is_reduce_over_v<E>) {
      // ReduceOver has one child: the body expression
      auto r = s.apply(e.expr());
      if constexpr (isSame<decltype(r), Never>) {
        return Never{};
      } else {
        return e.rebuild(r);
      }
    } else if constexpr (!is_expression_v<E>) {
      return e;  // True terminals: atoms, constants
    } else {
      return applyToChildren(e, MakeIndexSequence<E::arity>{});
    }
  }

private:
  template <typename Op, Symbolic... Args, SizeT... Is>
  constexpr auto applyToChildren(Expression<Op, Args...> e,
                                  IndexSequence<Is...>) const {
    return applySequential<Op>(e, std::index_sequence<Is...>{});
  }

  // Apply s to each child sequentially, propagating Never
  // We need sequential application because each result feeds into the next
  // type computation. Use a fold pattern with early exit.
  template <typename Op, typename Expr>
  constexpr auto applySequential(Expr e, std::index_sequence<>) const {
    return Expression<Op>{};
  }

  template <typename Op, typename Expr, SizeT I0, SizeT... IRest>
  constexpr auto applySequential(Expr e, std::index_sequence<I0, IRest...>) const {
    auto r0 = s.apply(e.template arg<I0>());
    if constexpr (isSame<decltype(r0), Never>) {
      return Never{};
    } else if constexpr (sizeof...(IRest) == 0) {
      return Expression<Op, decltype(r0)>{r0};
    } else {
      // Recurse for remaining children, then combine
      // This needs care to build the full Expression type
      return applyRemaining<Op>(r0, e, std::index_sequence<IRest...>{});
    }
  }
};
```

NOTE: The variadic implementation is tricky because Expression<Op, Args...> needs
ALL arg types at once. An alternative approach: apply s to each child, collect
results in a tuple, check for Never, then unpack into Expression. See the approach
sketched in README.md. Use whichever compiles cleanly.

**Keep the existing arity-specific overloads as a fallback** during development.
Delete them once the variadic version passes all tests.

### 1.3 Make substituteRecImpl variadic + ReduceOver-aware

**File to modify:** `src/symbolic4/strategy/recursive.h`

Same pattern as All<S>. The current code has overloads for Expression<Op> (nullary),
Expression<Op, A0> (unary), Expression<Op, A0, A1> (binary), Expression<Op, A0, A1, A2>
(ternary). Replace with variadic + add ReduceOver branch.

Also generalize the SumOver special case in `Recursive::apply()`:

```cpp
// BEFORE (line 292):
if constexpr (is_sum_over_v<E>) {

// AFTER:
if constexpr (is_reduce_over_v<E>) {
  auto inner_result = apply(expr.expr());
  return expr.rebuild(inner_result);  // Works for ANY ReduceOver
}
```

### 1.4 Write tests

**File to create:** `src/symbolic4/strategy/reduce_over_test.cc`

Test that:
1. `SumOver` alias works identically to before (backwards compat)
2. `All<S>` traverses into ReduceOver bodies
3. `Innermost<S>` simplifies inside ReduceOver nodes
4. `Recursive<Rules>` recurses into ReduceOver bodies
5. Operator overloads work: `ReduceOver + Symbolic`, `Symbolic * ReduceOver`, etc.
6. `ProdReduce`, `MaxReduce` type traits work

Add to CMakeLists.txt.

## Acceptance Criteria

- [ ] All existing `symbolic4` tests pass unchanged
- [ ] `is_sum_over_v<SumOver<D, E>>` still returns true (backwards compat)
- [ ] `is_reduce_over_v<SumOver<D, E>>` returns true
- [ ] `innermost(rules).apply(ReduceOver<SumReduce, D, x + 0_c>)` simplifies inner to `x`
- [ ] `Recursive::apply()` recurses into any ReduceOver, not just SumOver
- [ ] New test file compiles and passes

## Build & Test

```bash
cmake --build build && ctest --test-dir build -R symbolic4
```
