# Phase 5: Cleanup and Deduplication

## Goal

Remove all remaining rough edges: duplicated code, RTTI usage, inconsistent
APIs. This is pure quality-of-life work — no new features.

## Prerequisites

Phases 1-4 complete.

## Task 5.1: Deduplicate Constraint Transform Logic

The "apply constraint based on support type" logic currently appears in 4 places:
1. `interpreter/eval.h` — `eval_detail::applyConstraint<Support>(double z)` (numeric)
2. `indexed/indexed_eval.h` — `indexed_eval_detail::applyConstraint<Support>(double z)` (numeric, copy-pasted)
3. `distributions/collect_log_prob.h` — `constrainedExprFor<Support, Id>()` (symbolic)
4. `distributions/random_var.h` — `unconstrainedExpr()` (symbolic)

**Consolidate into one place.** Create `src/symbolic4/constraints.h`:

```cpp
namespace tempura::symbolic4::constraints {

// Numeric: apply constraint transform to a double value
template <typename Support>
constexpr auto apply(double z) -> double { ... }

// Symbolic: return the constraint expression for a given variable
template <typename Support, typename Id>
constexpr auto expr() { ... }

// Symbolic: return the log-Jacobian expression
template <typename Support, typename Id>
constexpr auto logJacobian() { ... }

}  // namespace tempura::symbolic4::constraints
```

Then replace all 4 usage sites with calls to this shared header.

## Task 5.2: Deduplicate Atom ID Matching

The concept "two atoms share the same Id regardless of effect" appears in:
1. `interpreter/diff.h` — `diff_detail::IsSameAtomId` (will be deleted in Phase 2)
2. `indexed/indexed_eval.h` — `indexed_eval_detail::HasSameAtomId`
3. Various ad-hoc `std::is_same_v<get_id_t<A>, get_id_t<B>>` checks

**Add to `core.h`:**

```cpp
// Check if two atoms have the same Id (regardless of effect)
template <typename A, typename B>
constexpr bool same_atom_id_v = false;

template <typename Id, typename E1, typename E2>
constexpr bool same_atom_id_v<Atom<Id, E1>, Atom<Id, E2>> = true;
```

Replace all ad-hoc checks with this trait.

## Task 5.3: Replace DimIndexMap with Compile-Time Typed Index Pack

**File to modify:** `src/symbolic4/indexed/indexed_eval.h`

The current `DimIndexMap` uses `std::unordered_map<std::type_index, SizeT>` —
runtime RTTI for compile-time known dimension tags. Replace with:

```cpp
// A compile-time typed dimension index pack
template <typename... DimEntries>
struct DimIndices : DimEntries... {
  using DimEntries::get...;   // Bring all get() overloads into scope
  using DimEntries::set...;   // Bring all set() overloads into scope
};

template <typename DimTag>
struct DimEntry {
  SizeT index = 0;
  SizeT size = 0;

  constexpr auto get(meta::TypeTag<DimTag>) const -> SizeT { return index; }
  constexpr void set(meta::TypeTag<DimTag>, SizeT i, SizeT s) { index = i; size = s; }
};
```

This eliminates the only RTTI usage in the codebase and is consistent with
the "types ARE the representation" philosophy.

**Challenge:** The set of active dimensions grows as we descend into nested
SumOver/ReduceOver. The current approach adds dimensions at runtime. A
compile-time approach would require the dimension set to be known statically,
which it IS (from the ReduceOver nesting structure). But threading this through
the recursive evaluation is more complex.

**Pragmatic compromise:** Use a compile-time typed pack for the LOOKUP
(eliminating `std::type_index` hashing) but keep runtime size for the
dimension set growth. Alternatively, use a fixed-size array with compile-time
indexing:

```cpp
template <typename... DimTags>
struct DimIndices {
  std::array<SizeT, sizeof...(DimTags)> indices{};
  std::array<SizeT, sizeof...(DimTags)> sizes{};

  template <typename DimTag>
  constexpr auto get() const -> SizeT {
    constexpr SizeT idx = indexOfType<DimTag, DimTags...>();
    return indices[idx];
  }

  template <typename DimTag>
  constexpr void set(SizeT i, SizeT s) {
    constexpr SizeT idx = indexOfType<DimTag, DimTags...>();
    indices[idx] = i;
    sizes[idx] = s;
  }
};
```

## Task 5.4: Remove Hardcoded ndims Specializations

**File to modify:** `src/symbolic4/indexed/indexed_eval.h`

Lines 193-218 have explicit branches for ndims in {1, 2, 3} plus a generic fallback.
The generic case already works — the specializations are premature optimization.
Remove them and keep only the generic case. Profile if there's a measurable
performance difference (there won't be).

Similarly in `src/symbolic4/indexed/dim.h`, consolidate the three IndexedBinding
specializations into one generic version.

## Task 5.5: Complete ReduceOver Operator Overloads

**File to check:** `src/symbolic4/indexed/reduce_over.h` (created in Phase 1)

Ensure ALL arithmetic operators work with ReduceOver, not just `+`:

```cpp
// ReduceOver op Symbolic
template <typename ROp, typename DimTag, Symbolic E1, Symbolic E2>
constexpr auto operator*(ReduceOver<ROp, DimTag, E1> r, E2 other) {
  return Expression<MulOp, ReduceOver<ROp, DimTag, E1>, E2>{r, other};
}

// etc. for -, /, and reversed operand order
// Also: ReduceOver op ReduceOver (same and different dim tags)
```

The original `sum_over.h` only defined `+`. This was a trap — `2 * SumOver<...>`
silently failed.

## Task 5.6: Unify `diff` / `differentiate` Naming

If Phase 2 used Option A (alias), now rename all call sites:
- Replace `diff(expr, var)` with `differentiate(expr, var)` everywhere
- Remove the `diff()` alias from `strategy/diff.h`

Or if the team prefers `diff` as the short name, rename `differentiate` to `diff`
in `strategy/diff.h` and update the strategy test file.

## Task 5.7: Clean Up sum_over.h

After all phases, `sum_over.h` should be reduced to:

```cpp
#pragma once
#include "symbolic4/indexed/reduce_over.h"
// SumOver is now an alias defined in reduce_over.h
// This header exists for backwards compatibility.
// New code should include reduce_over.h directly.
```

Or delete it entirely and update all includes to `reduce_over.h`.

## Acceptance Criteria

- [ ] `constraints.h` is the single source for constraint transform logic
- [ ] `same_atom_id_v` trait exists in `core.h` and is used everywhere
- [ ] No `std::type_index` or `typeid` in the codebase
- [ ] No hardcoded ndims={1,2,3} specializations
- [ ] All ReduceOver arithmetic operators work (*, -, /, not just +)
- [ ] Consistent naming for diff/differentiate
- [ ] All tests pass

## Build & Test

```bash
cmake --build build && ctest --test-dir build -R symbolic4
```
