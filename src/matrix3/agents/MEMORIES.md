# Shared Memory Bank

> This file contains accumulated knowledge for agents. Keep entries concise.
> Memory Curator reviews this file periodically to maintain quality.

Last curated: _Never_

---

## Code Patterns

### matrix3 GenericMatrix Pattern
```cpp
template <typename ExtentsT, typename LayoutT, typename AccessorT>
class GenericMatrix { ... };

// Concrete type = GenericMatrix<Extents, Layout, Accessor>
// Example: Dense<Scalar, Ns...> extends GenericMatrix
```

### Test Pattern
```cpp
#include "unit.h"
using namespace tempura;

"Test name"_test = [] {
  // static_assert for compile-time
  // expectEq/expectTrue for runtime
};

return TestRegistry::result();
```

### Initialization Pattern
```cpp
// CTAD from initializer lists:
InlineDense mat{{1, 2}, {3, 4}};  // Deduces InlineDense<int, 2, 2>
```

---

## Architecture Decisions

### Why mdspan-style design?
- Separates storage (Accessor) from indexing (Layout) from shape (Extents)
- Matches C++23 std::mdspan philosophy
- Enables zero-cost views and slices

### kDynamic sentinel
- matrix3: `kDynamic = std::dynamic_extent` (size_t max)
- matrix2: `kDynamic = int64_t min` (different!)
- Must convert carefully when porting

---

## Gotchas & Pitfalls

1. **NOLINT for C-arrays**: Initialization helpers use C-arrays; suppress clang-tidy
2. **requires clauses**: matrix3 uses C++20 concepts heavily; ensure constraints match
3. **constexpr vectors**: std::vector not constexpr in older standards; Dense uses runtime
4. **Column-major default**: LayoutLeft = column-major (Fortran style)

---

## Useful References

- `src/matrix3/base_test.cpp` - Examples of all current matrix3 types
- `src/matrix2/storage/inline_dense.h` - Pattern for inline storage
- `src/unit.h` - Testing framework reference

---

## Questions for Humans

_Agents can post questions here for human review_

---

## Deprecated/Removed

_Track things we tried but removed_
