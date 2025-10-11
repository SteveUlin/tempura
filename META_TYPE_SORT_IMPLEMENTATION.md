# Meta Type Sorting Implementation

## Summary

Created a simple, extensible API for compile-time type sorting in the `meta` library. This API can be easily updated with more efficient algorithms in the future without changing client code.

## Implementation

### New File: `src/meta/type_sort.h`

Provides a clean API for sorting TypeLists using custom comparison predicates:

```cpp
template <typename List, typename Compare>
using SortTypes_t = ...;
```

**Current Implementation:** Insertion sort

- Simple and correct
- O(n²) time complexity
- Suitable for small type lists (typical use case)
- Can be replaced with heapsort, mergesort, or other algorithms later

**API Design:**

- Comparison predicate is a type with `constexpr operator()`
- Returns true if first argument should come before second
- Works with any types that can be default-constructed
- Fully constexpr - all computation at compile-time

### Integration: `src/symbolic3/canonical.h`

Updated to use the new meta API:

```cpp
#include "meta/type_sort.h"

// Comparison predicate for symbolic3 ordering
struct LessThanComparator {
  template <typename A, typename B>
  constexpr bool operator()(A, B) const {
    return lessThan(A{}, B{});
  }
};

// Sort TypeList using symbolic3 ordering
template <typename List>
using SortTypes_t = SortTypes_t<List, LessThanComparator>;
```

**Benefits:**

- Removed ~150 lines of heapsort implementation from canonical.h
- Cleaner separation of concerns
- Type sorting is now reusable across the codebase
- Easy to swap implementations by updating meta/type_sort.h

## Testing

### New Test: `src/meta/test/type_sort_test.cpp`

Comprehensive compile-time tests:

- Empty list sorting
- Single element sorting
- Two element sorting (both sorted and unsorted)
- Three element sorting
- Five element reverse-order sorting

All tests use `static_assert` for compile-time verification.

### Test Results

```
✓ symbolic3_canonical - All tests passing
✓ meta_type_sort - All tests passing
```

## Design Philosophy

**Keep It Simple:**

- Insertion sort is simple to understand and maintain
- For small lists (typical case), performance difference is negligible
- Premature optimization avoided

**Extensible API:**

- Easy to replace implementation without changing client code
- Comparison predicate allows domain-specific ordering
- Can add specialized versions for specific use cases later

**Future Improvements:**

- Could add heapsort for large lists (O(n log n))
- Could add specialized sorting for specific predicates
- Could add stable vs unstable sort variants
- All changes would be internal to meta/type_sort.h

## Files Changed

**Created:**

- `src/meta/type_sort.h` - Type sorting API
- `src/meta/test/type_sort_test.cpp` - Test suite

**Modified:**

- `src/symbolic3/canonical.h` - Use meta API instead of local heapsort
- `src/meta/CMakeLists.txt` - Added type_sort_test

**Impact:**

- Reduced complexity in symbolic3/canonical.h
- Improved code reusability
- Better separation of concerns
- No performance regression (same algorithm, different location)
