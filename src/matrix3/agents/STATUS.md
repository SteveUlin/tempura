# Migration Status

Last updated: 2024-12-10T10:00:00Z

## Current State

**Phase**: 2 - Storage Types
**Active Task**: InlineCoordinateList
**Blocking Issues**: None

---

## Assignment Queue

### Next Up
| Priority | Task | Assignee | Notes |
|----------|------|----------|-------|
| 2 | Complex wrapper | - | Simple, self-contained |
| 3 | Banded storage | - | Depends on nothing |

### In Progress
| Task | Assignee | Started | Notes |
|------|----------|---------|-------|
| InlineCoordinateList | Implementer | 2024-12-10T10:00:00Z | First sparse storage type |

### Pending Review
_None_

### Recently Completed
| Task | Completed | Commits |
|------|-----------|---------|
| Phase 1 Core | Pre-existing | - |

---

## Review Feedback

### Active Feedback
_None_

### Review Iteration Counter
| Task | Iteration | Max |
|------|-----------|-----|
| - | 0 | 3 |

### Addressed
_None_

---

## Blocking Issues

_None currently_

---

## Agent Activity Log

Format: `[TIMESTAMP] AGENT: Action`

```
[2024-12-10T00:00:00Z] SYSTEM: Status file initialized
[2024-12-10T10:00:00Z] DIRECTOR: Assigned InlineCoordinateList to Implementer
```

---

## Handoff Notes

### To: Implementer (InlineCoordinateList)
**From**: Director
**Date**: 2024-12-10T10:00:00Z

**Task Overview:**
Migrate InlineCoordinateList from matrix2 to matrix3 architecture. This is the first sparse storage type, so it will establish patterns for future sparse implementations.

**Source Analysis** (`/home/ulins/workspace/tempura/src/matrix2/storage/inline_coordinate_list.h`):
- COO (Coordinate List) format with 3 parallel arrays: row_indices, col_indices, values
- Template params: `<T, Row, Col, Capacity, IndexOrder>`
- Currently implements only `IndexOrder::kNone` (unsorted, newest-value-wins on lookup)
- Key features:
  - Reverse linear search for lookups (newest first)
  - Constant-time insertion (append-only)
  - Returns zero for missing elements
  - Factory function `makeInlineCoordinateList` for deduction
  - Triplet struct for {i, j, value}
  - Range constructor for initialization

**Matrix3 Architecture Patterns:**
- Extents: Use `Extents<IndexT, Ns...>` instead of `Row, Col` template params
  - Static sizes: `Extents<std::size_t, 2, 3>`
  - Dynamic dimension: `kDynamic` constant
- Layout: Custom layout needed for COO sparse indexing
  - Passthrough pattern: see `LayoutPassthrough` (returns tuple of indices)
  - Will need coordinate-list-specific layout
- Accessor: Custom accessor needed for sparse data
  - See `RangeAccessor` pattern for data storage
  - See commented `CompressedSparseAccessor` for sparse hints
  - Needs to handle: lookup, insertion, zero-value returns
- Base: Inherit from `GenericMatrix<ExtentsT, LayoutT, AccessorT>`

**Migration Strategy:**
1. Create custom `CoordinateListLayout` that handles COO indexing
2. Create custom `CoordinateListAccessor` that:
   - Stores three parallel arrays (rows, cols, values)
   - Implements reverse search for reads
   - Implements append for writes
   - Returns zero for missing elements
3. Define `InlineCoordinateList` as specialization of `GenericMatrix`
4. Preserve compile-time capacity via template param
5. Consider: How to handle `Capacity` in new architecture (maybe as non-type template param on accessor?)

**Potential Challenges:**
- Matrix3 uses `operator[](i, j)` which returns a reference - COO can't return mutable ref to missing elements
  - May need proxy object or different access pattern
  - Check if `operator[]` can return by value for const access
- Insertion API: matrix3 doesn't have `.insert()` method on GenericMatrix
  - May need to expose accessor methods directly
  - Or add methods to InlineCoordinateList derived class
- Factory function: Deduction guides work differently in matrix3
  - Check existing Dense/InlineDense patterns

**Notes:**
- Keep the TODOs about std::inplace_vector
- Preserve constexpr-friendliness
- Consider whether sorted variants (kRowMajor, kColMajor) should be migrated now or later
- Test file will be needed but don't create it yet

---

## Metrics

| Metric | Value |
|--------|-------|
| Tasks Completed | 0 |
| Tasks Remaining | 15 |
| Review Cycles | 0 |
| Avg Reviews/Task | - |
| Commits Since Checkpoint | 0 |
| Checkpoint Target | 5 |
