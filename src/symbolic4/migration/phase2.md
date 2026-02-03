# Phase 2: Switch diff() to differentiate()

## Goal

Migrate all production code from the cata-based `diff()` (in `interpreter/diff.h`)
to the strategy-based `differentiate()` (in `strategy/diff.h`). Then delete the
old implementation.

## Prerequisites

Phase 1 complete: `Recursive::apply()` handles ReduceOver generically,
so `differentiate()` works through reductions without a standalone overload.

## Context

The strategy-based `differentiate()` already exists and works. It lives in
`strategy/diff.h` and uses `recursive(rrule(...) | rrule(...) | ...)`. It
already auto-simplifies results via `simplify()` on line 124.

The cata-based `diff()` lives in `interpreter/diff.h` and uses the
paramorphism scheme (`para`). There's also `indexed/sum_over_diff.h` which
adds a standalone `diff()` overload for SumOver.

Both systems produce the same mathematical results. The strategy version
is cleaner (rules read like math) and composes better.

## Files to Modify

### Call sites that use `diff()` (must change to `differentiate()`):

1. `src/symbolic4/infer.h:204`
   ```cpp
   // BEFORE:
   auto grads = std::make_tuple(simplify(diff(joint_lp, rvs.freeSym()))...);
   // AFTER:
   auto grads = std::make_tuple(differentiate(joint_lp, rvs.freeSym())...);
   ```
   Note: `differentiate()` already calls `simplify()` internally.

2. `src/symbolic4/model.h:173` and `model.h:333`
   Same pattern. Replace `simplify(diff(lp, ...))` with `differentiate(lp, ...)`.

3. `src/symbolic4/mcmc/posterior.h:146`
   Same pattern.

4. `src/symbolic4/mcmc/transforms.h:907`
   Same pattern.

5. `src/symbolic4/mcmc/plate_transforms.h:1467` and `:1495`
   Same pattern.

6. `src/symbolic4/matrix/diff.h:4` — includes `interpreter/diff.h`
   Read this file to understand what it does. It may define matrix-specific
   diff rules. If so, those rules need to be added to the strategy diff system
   or kept as standalone overloads that call `differentiate()`.

### Include changes:

All files above currently `#include "symbolic4/interpreter/diff.h"`.
Change to `#include "symbolic4/strategy/diff.h"`.

Also: `src/symbolic4/symbolic4.h:48` includes `interpreter/diff.h` —
change to `strategy/diff.h`.

### Handle the `diff` → `differentiate` name change:

Two options:
- **Option A (recommended):** Add `diff()` as an alias in `strategy/diff.h`:
  ```cpp
  template <Symbolic E, typename Var>
  constexpr auto diff(E expr, Var v) {
    return differentiate(expr, v);
  }
  ```
  This makes the migration zero-diff at call sites. Only includes change.

- **Option B:** Rename all call sites from `diff(...)` to `differentiate(...)`.
  Cleaner long-term but touches more lines. Up to you.

**Recommendation:** Option A for this phase, rename later as separate cleanup.

## Files to Delete After Migration

1. `src/symbolic4/interpreter/diff.h` — the old cata-based implementation
2. `src/symbolic4/indexed/sum_over_diff.h` — standalone SumOver overload (handled by Recursive now)
3. `src/symbolic4/interpreter/diff_test.cc` — tests for old diff (keep strategy/diff_test.cc)

Also remove from CMakeLists.txt:
- The `interpreter/diff.h` test target (around line 83)
- Any references to `sum_over_diff.h`

## Strategy diff.h Changes Needed

### Update strategy/diff.h to handle ReduceOver differentiation

After Phase 1, `Recursive::apply()` generically recurses into ReduceOver bodies.
For `SumReduce`, this is mathematically correct: d/dx Σf = Σ d/dx f.

But remove the standalone SumOver overload at `strategy/diff.h:133-137` since
`Recursive::apply()` now handles it. Verify that the recursive handler produces
the same results.

### Handle Sample atoms (RandomVarSymbol)

Check whether `strategy/diff.h` handles `Atom<Id, Sample<Dist>>` correctly.
The cata-based `interpreter/diff.h` has special handling for Sample atoms
(applies chain rule through the constraint transform). The strategy version
needs the same capability.

Look at `interpreter/diff.h:113-140` for the Sample atom handling.
In the strategy system, this would be a terminal rule:

```cpp
// If the atom being differentiated is a Sample with a constraint,
// the chain rule applies through the transform
// e.g., d/dx[exp(z)] when z is the variable = exp(z)
```

The existing `rule(Var{}, 1_c)` and `rule(AnySymbol{}, 0_c)` terminal
rules in `strategy/diff.h:109-113` should handle this IF the Sample atoms
are replaced by their constrained expressions before differentiation.

Verify this by running the existing tests. If Sample atom handling is missing,
add appropriate terminal rules.

### Handle IndexedSymbol

Similarly check that `IndexedSymbol<Id, Dims...>` is handled. The cata-based
diff has `is_indexed_random_var_atom_v` handling. The strategy version may need
an `AnyIndexedRVAtom` wildcard or specific rule.

## Testing Strategy

**Critical: run tests after EACH file migration, not all at once.**

Order of migration (safest first):
1. Add `diff()` alias to `strategy/diff.h`
2. Switch `symbolic4.h` include
3. Switch `infer.h` → run tests
4. Switch `model.h` → run tests
5. Switch `mcmc/posterior.h` → run tests
6. Switch `mcmc/transforms.h` → run tests
7. Switch `mcmc/plate_transforms.h` → run tests
8. Switch `matrix/diff.h` → run tests
9. Delete old files → run full test suite

```bash
# After each file change:
cmake --build build && ctest --test-dir build -R symbolic4
```

## Acceptance Criteria

- [ ] All `symbolic4` tests pass
- [ ] No file includes `interpreter/diff.h`
- [ ] No file includes `indexed/sum_over_diff.h`
- [ ] `interpreter/diff.h` is deleted
- [ ] `indexed/sum_over_diff.h` is deleted
- [ ] `strategy/diff.h` handles all cases that `interpreter/diff.h` handled
- [ ] Differentiation through SumOver still works (tested by plate tests)
