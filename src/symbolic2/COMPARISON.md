# Comparison: symbolic/ vs meta/symbolic.h (now symbolic2/)

## Summary

I've analyzed both implementations and created a comprehensive migration plan. The key finding is that your new `meta/symbolic.h` is significantly cleaner and more modern than the existing `symbolic/` directory. I've now set up `symbolic2/` as a development area to eventually replace `symbolic/`.

## Key Improvements in New Implementation

### 1. **Simpler User Experience**

**Old**: `TEMPURA_SYMBOL(x)` with macro expansion
**New**: `Symbol x;` - just works!

### 2. **Better Architecture**

**Old**: 8 separate header files with complex dependencies
**New**: 1 core header, self-contained

### 3. **Modern C++26**

**Old**: Pre-C++20 style with workarounds
**New**: Concepts, fold expressions, lambda-based uniqueness

### 4. **Cleaner Pattern Matching**

**Old**: Separate `MatcherExpression` type
**New**: Unified system with wildcards (AnyArg, AnyExpr, etc.)

### 5. **Better Performance**

**Old**: String comparison for ordering
**New**: Type ID counter (faster compilation)

## What I've Created

```
src/symbolic2/
├── symbolic.h              # Core (copied from meta/symbolic.h)
├── README.md              # Comprehensive feature documentation
├── MIGRATION.md           # Step-by-step migration guide
├── PLAN.md               # Development roadmap
├── CMakeLists.txt        # Build configuration
├── test/
│   ├── symbolic_test.cpp       # Core functionality tests
│   └── simplify_test.cpp       # Simplification tests
└── examples/
    └── basic_usage.cpp         # Introductory example
```

## Detailed Comparisons

### Symbol Creation

```cpp
// OLD (symbolic/)
TEMPURA_SYMBOL(x);
TEMPURA_SYMBOLS(a, b, c);  // Macro gymnastics

// NEW (symbolic2/)
Symbol x;
Symbol a, b, c;  // Simple, clean, type-safe
```

### Evaluation

```cpp
// OLD
auto result = expr(Substitution{x = 5, y = 3});

// NEW
auto result = evaluate(expr, BinderPack{x = 5, y = 3});
```

### Pattern Matching

```cpp
// OLD
if constexpr (match(expr, Any{} + Any{})) {
    auto left = expr.left();   // member function
}

// NEW
if constexpr (match(expr, AnyArg{} + AnyArg{})) {
    auto left = left(expr);    // free function
}
```

## Features Present in Both

✅ Symbolic variables
✅ Constants with `_c` literal
✅ Expression building with operators
✅ Pattern matching
✅ Simplification rules
✅ Evaluation system
✅ Math functions (sin, cos, log, exp, pow, etc.)
✅ Special constants (π, e)

## Features to Port from Old to New

🔄 **derivative.h** - Symbolic differentiation (important!)
🔄 **to_string.h** - Expression formatting (partially done)
🔄 **Complete operator set** - Some operators missing
🔄 **Advanced simplification** - Distribution, factoring

## Migration Strategy

### Phase 1: Setup ✅ (DONE)

- Created symbolic2/ directory structure
- Documentation (README, MIGRATION, PLAN)
- Basic tests and examples
- Build system

### Phase 2: Testing (NEXT)

- Port all tests from symbolic/\*\_test.cpp
- Verify feature parity
- Fix any issues

### Phase 3: Feature Enhancement

- Port derivative.h
- Enhance toString
- Add missing operators
- Advanced simplification

### Phase 4: Integration

- Update project build
- Documentation
- Examples

### Phase 5: Deprecation

- Mark symbolic/ as deprecated
- Parallel support period
- Help users migrate

### Phase 6: Replacement

- symbolic2/ becomes symbolic/
- Remove old implementation

## Recommendations

1. **Continue developing in symbolic2/**

   - Don't touch meta/symbolic.h anymore
   - All new work goes in symbolic2/

2. **Focus on derivative.h next**

   - Most important missing feature
   - Used by existing code

3. **Test thoroughly**

   - Port all existing tests
   - Add new edge case tests

4. **Don't rush migration**

   - Support both versions during transition
   - Give users time to migrate

5. **Keep documentation updated**
   - Update README as features are added
   - Maintain MIGRATION.md

## Technical Details

### Why Lambda-Based Symbols Work

```cpp
template <typename unique = decltype([] {})>
struct Symbol : SymbolicTag { ... };
```

Each `Symbol x;` creates a unique type because:

- Lambda types are unique per lambda expression
- Default template argument gets a new lambda each time
- No two symbols can have the same type

### Why It's Faster

- **Type ID**: O(1) integer comparison
- **String names**: O(n) string comparison
- **Less templates**: Fewer instantiations
- **Single header**: Less parsing

### Pattern Matching Rank System

The new implementation uses "ranked overload resolution":

```cpp
constexpr auto matchImpl(Rank5, ...) // Highest priority
constexpr auto matchImpl(Rank4, ...) // Type match
constexpr auto matchImpl(Rank3, ...) // Wildcards
constexpr auto matchImpl(Rank2, ...) // Value comparison
constexpr auto matchImpl(Rank1, ...) // Expression recursion
constexpr auto matchImpl(Rank0, ...) // Default fallback
```

This is cleaner than separate Matcher/Symbolic type system.

## Files to Review

1. **symbolic2/README.md** - Feature overview
2. **symbolic2/MIGRATION.md** - How to migrate code
3. **symbolic2/PLAN.md** - Development roadmap
4. **symbolic2/symbolic.h** - Core implementation

## Next Steps

1. Review the created documentation
2. Test the basic examples (once build system is updated)
3. Begin porting tests from symbolic/
4. Start derivative.h port
5. Update main CMakeLists.txt to include symbolic2/

## Questions?

Feel free to:

- Modify any of the documentation
- Adjust the roadmap
- Change the directory structure
- Add or remove features from the plan

The foundation is now in place for a cleaner, more maintainable symbolic mathematics library!
