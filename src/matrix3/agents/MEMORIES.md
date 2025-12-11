# Shared Memory Bank

> This file contains accumulated knowledge for agents. Keep entries concise.
> Memory Curator reviews this file periodically to maintain quality.

Last curated: 2024-12-10T22:45:00Z

---

## Code Patterns

### matrix3 GenericMatrix Pattern
```cpp
template <typename ExtentsT, typename LayoutT, typename AccessorT>
class GenericMatrix { ... };

// Concrete type = GenericMatrix<Extents, Layout, Accessor>
// Example: Dense<Scalar, Ns...> extends GenericMatrix
```

### Standalone Wrapper Pattern (Complex, Banded, BlockRow)
```cpp
// For wrappers/views that don't fit GenericMatrix inheritance:
template <typename... Params>
class WrapperType {
  static constexpr int64_t kRows = ...;
  static constexpr int64_t kCols = ...;

  constexpr auto operator[](this auto&& self, Indices... indices) -> ReturnType {
    // Custom indexing logic
  }

  constexpr int64_t rows() const { return kRows; }
  constexpr int64_t cols() const { return kCols; }
};
```

### MatrixTraits Helper Pattern
```cpp
// Extract dimensions from InlineDense template parameters:
template <typename T> struct MatrixTraits;

template <typename Scalar, std::size_t... Ns>
struct MatrixTraits<InlineDense<Scalar, Ns...>> {
  using ValueType = Scalar;
  static constexpr int64_t kRows = [&] {
    std::size_t i = 0;
    return ((i++ == 0) ? Ns : 0) + ...;
  }();
  static constexpr int64_t kCols = [&] {
    std::size_t i = 0;
    return ((i++ == 1) ? Ns : 0) + ...;
  }();
};
```

### Short-Circuit Fold Expression for Routing
```cpp
// Pattern for BlockRow indexing:
std::apply([&](auto&... mats) {
  int64_t offset = 0;
  return ((j < offset + MatrixCols ?
           (result = &mats[i, j - offset], true) :
           (offset += MatrixCols, false)) or ...);
}, tuple);
```

### Bounds Checking Pattern
```cpp
// constexpr-friendly bounds checking:
constexpr auto operator[](this auto const& self, Indices... indices) {
  if (std::is_constant_evaluated()) {
    assert(i >= 0 && i < kRows && j >= 0 && j < kCols);
  }
  // Implementation
}
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

### Three-Tier Arithmetic Operation Pattern
```cpp
// For element-wise operations (addition, subtraction):

// Tier 1: In-place modification (returns Lhs&)
template <typename Lhs, typename Rhs>
constexpr auto operator+=(Lhs& left, const Rhs& right) -> Lhs& {
  checkSameExtent(left, right);
  for (std::size_t i = 0; i < left.extent().extent(0); ++i)
    for (std::size_t j = 0; j < left.extent().extent(1); ++j)
      left[i, j] += right[i, j];
  return left;
}

// Tier 2: Explicit output type (returns Out)
template <typename Out, typename Lhs, typename Rhs>
constexpr auto add(const Lhs& left, const Rhs& right) -> Out {
  checkSameExtent(left, right);
  Out out;
  for (std::size_t i = 0; i < left.extent().extent(0); ++i)
    for (std::size_t j = 0; j < left.extent().extent(1); ++j)
      out[i, j] = left[i, j] + right[i, j];
  return out;
}

// Tier 3: Auto-inference (returns InlineDense with promoted type)
template <typename Lhs, typename Rhs>
constexpr auto operator+(const Lhs& left, const Rhs& right) {
  checkSameExtent(left, right);
  using ScalarT = decltype(left[0, 0] + right[0, 0]);
  constexpr auto kRow = Lhs::ExtentsType::staticExtent(0);
  constexpr auto kCol = Lhs::ExtentsType::staticExtent(1);
  InlineDense<ScalarT, kRow, kCol> out;
  // ... element-wise operation
  return out;
}
```

### Block-Based Tiling Algorithm Pattern
```cpp
// For matrix multiplication with cache optimization:
template <std::size_t block_size = 16, typename Out, typename Lhs, typename Rhs>
constexpr auto tileMultiply(const Lhs& left, const Rhs& right) -> Out {
  checkMultiplyExtent(left, right);
  Out out{};  // Zero-initialized

  // Five nested loops: jblock → i → kblock → j → k
  for (std::size_t jblock = 0; jblock < right.extent().extent(1); jblock += block_size) {
    for (std::size_t i = 0; i < left.extent().extent(0); ++i) {
      for (std::size_t kblock = 0; kblock < left.extent().extent(1); kblock += block_size) {
        auto jend = std::min(jblock + block_size, right.extent().extent(1));
        auto kend = std::min(kblock + block_size, left.extent().extent(1));
        for (std::size_t j = jblock; j < jend; ++j) {
          for (std::size_t k = kblock; k < kend; ++k) {
            out[i, j] += left[i, k] * right[k, j];
          }
        }
      }
    }
  }
  return out;
}
```

### Extent Validation Helpers
```cpp
// Shape validation for element-wise operations:
template <typename Lhs, typename Rhs>
constexpr void checkSameExtent(const Lhs& lhs, const Rhs& rhs) {
  static_assert(Lhs::ExtentsType::rank() == Rhs::ExtentsType::rank());
  assert(lhs.extent().extent(0) == rhs.extent().extent(0));
  assert(lhs.extent().extent(1) == rhs.extent().extent(1));
}

// Dimension validation for matrix multiplication:
template <typename Lhs, typename Rhs>
constexpr void checkMultiplyExtent(const Lhs& lhs, const Rhs& rhs) {
  static_assert(Lhs::ExtentsType::rank() == 2 && Rhs::ExtentsType::rank() == 2);
  assert(lhs.extent().extent(1) == rhs.extent().extent(0));  // Lhs cols == Rhs rows
}
```

### Type Promotion Pattern
```cpp
// Automatic type deduction from operation result:
using ScalarT = decltype(left[0, 0] + right[0, 0]);  // For addition
using ScalarT = decltype(left[0, 0] * right[0, 0]);  // For multiplication

// Handles mixed types: int + double → double, int * double → double
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

### When to use GenericMatrix inheritance vs standalone wrapper?
- **GenericMatrix**: Use for storage types that can provide mutable references (Dense, etc.)
- **Standalone**: Use for wrappers/views with special semantics (Complex, Banded, BlockRow, InlineCoordinateList)
- Standalone pattern needed when: read-only, sparse, mathematical view, or compositional

---

## Gotchas & Pitfalls

1. **NOLINT for C-arrays**: Initialization helpers use C-arrays; suppress clang-tidy
2. **requires clauses**: matrix3 uses C++20 concepts heavily; ensure constraints match
3. **constexpr vectors**: std::vector not constexpr in older standards; Dense uses runtime
4. **Column-major default**: LayoutLeft = column-major (Fortran style)
5. **Out-of-band writes**: Using kZero member for reference returns enables UB if written to
6. **Type consistency**: Use int64_t for indices (not size_t) - consistent with matrix2
7. **Deduction conflicts**: Explicitly declare return types in operator[] to avoid ambiguity
8. **Loop indices**: Use std::size_t for extent API consistency (extent().extent(i) returns size_t)
9. **constexpr extent()**: Required making GenericMatrix::extent() constexpr for constexpr algorithms
10. **Static extent extraction**: Use `Lhs::ExtentsType::staticExtent(0)` not `Lhs::kRow` in matrix3
11. **Zero initialization**: InlineDense constructor zero-initializes; use `Out out{}` for tileMultiply

---

## Useful References

- `src/matrix3/base_test.cpp` - Examples of all current matrix3 types
- `src/matrix2/storage/inline_dense.h` - Pattern for inline storage
- `src/matrix3/complex.h` - Simple standalone wrapper example
- `src/matrix3/banded.h` - MatrixTraits helper + kZero trick example
- `src/matrix3/block.h` - Short-circuit fold + compositional pattern
- `src/matrix3/addition.h` - Three-tier pattern + checkSameExtent validation
- `src/matrix3/multiplication.h` - Block-based tiling + checkMultiplyExtent validation
- `src/unit.h` - Testing framework reference

---

## Questions for Humans

_Agents can post questions here for human review_

---

## Deprecated/Removed

_Track things we tried but removed_
