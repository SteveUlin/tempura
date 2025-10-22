# Symbolic3 Improvement Roadmap

**Date:** October 20, 2025
**Last Updated:** October 21, 2025
**Status:** ✅ IN PROGRESS - Evidence-Based, Low-Risk Improvements
**Goal:** Improve symbolic3 without radical redesign

---

## 🎉 Completed Work Summary

### Phase 1.2: Operator Display Traits ✅ (Completed: Oct 21, 2025)

**What was accomplished:**

1. **Refactored `meta/function_objects.h`:**
   - Removed all `kSymbol` and `kDisplayMode` static members from operators
   - Made `AddOp` and `MulOp` variadic (unary, binary, n-ary support)
   - Operators are now pure function objects (like `std::plus`)

2. **Created `symbolic3/operator_display.h`:**
   - New centralized location for all display metadata
   - `DisplayTraits` template with specializations for all operators
   - Includes symbol, display mode, and precedence information
   - Precedence hierarchy: atomic (50) > unary (40) > power (30) > multiplication (20) > addition (10)

3. **Refactored `symbolic3/operators.h`:**
   - Now contains only symbolic-specific functionality
   - Operator overloads for `Symbolic` concept
   - Helper functions (sin, cos, exp, log, pow, etc.)
   - Type predicates (is_add, is_mul, is_trig_function, is_transcendental)
   - Constant helpers (zero_c, one_c, π, e)

4. **Added `operator_display_test.cpp`:**
   - Comprehensive compile-time tests for DisplayTraits
   - Verifies all operators have correct symbols, modes, and precedence
   - Tests precedence hierarchy ordering
   - All tests passing

**Architecture achieved:**
- ✅ Clean separation: Pure operators (meta) ← Display traits (symbolic3) ← Symbolic operations (symbolic3)
- ✅ Operators are extensible without modification
- ✅ Foundation for multiple renderers (ASCII, LaTeX, MathML)
- ✅ Precedence-aware parenthesization enabled
- ✅ No dangerous static algebraic assumptions

**Test results:**
- All 20 symbolic3 tests passing (including new operator_display_test)
- Precedence-aware output working correctly
- Zero regressions

**Files modified:**
- `src/meta/function_objects.h` (cleaned operator definitions)
- `src/symbolic3/operator_display.h` (new file with DisplayTraits)
- `src/symbolic3/operators.h` (refactored to symbolic-specific code)
- `src/symbolic3/test/operator_display_test.cpp` (new comprehensive test)
- `src/symbolic3/CMakeLists.txt` (added operator_display_test)

**Benefits realized:**
- Operators are now pure function objects following single responsibility principle
- Display concerns properly separated from evaluation logic
- Type system supports precedence-aware formatting
- Extensible design - can add new display modes without touching operators
- Maintains theoretical soundness and zero-cost abstractions

---

## 🚀 Next Steps for Upcoming Sessions

### Recommended: Phase 1.1 - Match Overload Deduplication

**Why this next:**
- Completes Phase 1 (Low-Hanging Fruit)
- Low risk, high value
- ~50 line reduction
- Improves code clarity

**Task breakdown:**
1. Create `ExactMatchable` concept in `src/symbolic3/matching.h`
2. Unify exact-match overloads using the concept
3. Add tests to verify no behavioral changes
4. Document the new pattern

**Estimated time:** 2-3 days

**Files to modify:**
- `src/symbolic3/matching.h` (main refactoring)
- `src/symbolic3/test/matching_test.cpp` (add concept coverage tests)

### Alternative: Phase 1.3 - Documentation Improvements

**Why this next:**
- Zero code risk (documentation only)
- High educational value
- Prevents future "why two types?" questions
- Good "light" task between heavier phases

**Task breakdown:**
1. Add "Design Rationale" section to README
2. Create `DESIGN.md` with Symbol vs PatternVar explanation
3. Add examples showing the distinction
4. Link to term rewriting theory references

**Estimated time:** 1-2 days

**Files to create/modify:**
- `src/symbolic3/README.md` (update)
- `src/symbolic3/DESIGN.md` (new)

### Phase 2 Preview: Debugging Infrastructure

Once Phase 1 is complete, Phase 2 focuses on developer experience:
- Compile-time pretty printing for better error messages
- Expression tree visualization for debugging
- Transformation tracing for understanding simplification

These are additive improvements that don't risk breaking existing functionality.

---

## Executive Summary

Instead of the risky "core redesign" proposal, this document outlines **incremental, evidence-based improvements** that deliver real value with minimal risk.

**Key Principles:**

- ✅ **Preserve what works** - Current architecture is theoretically sound
- ✅ **Fix actual pain points** - Based on usage data, not speculation
- ✅ **Incremental changes** - Small, testable improvements
- ✅ **Evidence-driven** - Measure before and after

---

## Current State Assessment

**Codebase metrics (verified):**

- Total LOC: 6,256 lines (22 header files)
- Tests passing: 9/23 (14 not built, likely configuration issue)
- Core design: Sound (based on term rewriting theory)

**What works well:**

- ✅ Clean separation: Symbol (variables) vs PatternVar (patterns)
- ✅ Zero runtime overhead (types resolve at compile-time)
- ✅ Constexpr by default (true compile-time computation)
- ✅ Type-safe pattern matching (catches errors at compile-time)
- ✅ Composable strategies (strong theoretical foundation)

**Actual pain points:**

1. ⚠️ Template error messages are verbose
2. ⚠️ Some match() overloads are duplicated
3. ⚠️ Operator metadata inconsistent
4. ⚠️ Debugging utilities limited
5. ⚠️ Documentation gaps (why PatternVar exists)

**NOT actual pain points:**

- ❌ "Too many types" (9 types is appropriate for domain complexity)
- ❌ "Symbol vs PatternVar confusion" (proper separation of concerns)
- ❌ "Excessive LOC" (6,256 lines for a CAS is lean)

---

## Design Principles: Separation of Concerns

### Operators as Pure Function Objects

**Decision:** Operators should be pure mathematical operations, like `std::plus`.

**Rationale:**
- Single Responsibility Principle: evaluation logic separate from display/algebraic properties
- Extensibility: can add new trait systems without modifying operators
- Flexibility: same operators work with different display systems (ASCII, LaTeX, MathML)
- Testability: evaluation logic tested independently

### Display Properties: Safe to Centralize

**What belongs in DisplayTraits:**
- ✅ `symbol` - textual representation ("+", "sin", etc.)
- ✅ `mode` - infix vs prefix notation
- ✅ `precedence` - for smart parenthesization

**Why:** Display is a presentation concern, not a mathematical one. Safe to specify statically.

### Algebraic Properties: Context-Dependent

**What does NOT belong in static traits:**
- ❌ `commutative` - depends on domain (matrices vs scalars)
- ❌ `associative` - depends on domain
- ❌ `identity_element` - domain-specific

**Why:**
1. **Commutativity is not inherent to operators** - it's a property of the algebraic structure
   - Matrix multiplication: NOT commutative
   - Scalar multiplication: commutative
   - String concatenation: NOT commutative

2. **Even when true, needs canonical ordering to prevent loops:**
   ```cpp
   // Without ordering: infinite loop!
   x + y  →  y + x  (apply commutativity)
   y + x  →  x + y  (apply again!)
   ```

3. **Solution:** Pass algebraic properties through the context system:
   ```cpp
   auto ctx = default_context()
     .with(AlgebraicRules{
       .canonical_ordering = lessThan,  // Prevents loops
       .domain = RealNumbers{}
     });
   ```

---

## Phase 1: Low-Hanging Fruit (2 weeks)

### 1.1 Reduce Match Overload Duplication (~50 line reduction)

**Current problem:**

```cpp
// Similar patterns repeated
template<typename U> bool match(Symbol<U>, Symbol<U>);
template<auto V> bool match(Constant<V>, Constant<V>);
```

**Solution using concepts:**

```cpp
// Unified overload for exact matching
template<ExactMatchable T>
constexpr bool match(T a, T b) {
    return isSame<T, decltype(b)>;
}

// Concept constrains applicable types
template<typename T>
concept ExactMatchable = is_symbol<T> || is_constant<T> || is_fraction<T>;
```

**Benefits:**

- Reduces code duplication
- Makes pattern explicit (exact matching)
- No behavioral change
- Easy to test

**Implementation:**

- File: `src/symbolic3/matching.h`
- Time: 2-3 days
- Risk: Low (internal refactoring)

### 1.2 Separate Operator Display Traits (~100 line reduction, better maintainability)

**Current problem:**

- Operators mix evaluation logic with display metadata (`kSymbol`, `kDisplayMode`)
- Violates single responsibility principle
- No precedence information for smart parenthesization
- Display properties hardcoded in operator definitions

**Solution - Layered Design:**

```cpp
// LAYER 1: Pure Operations (in src/symbolic3/operators.h)
// Remove kSymbol and kDisplayMode - operators become pure function objects
struct AddOp {
  constexpr auto operator()(auto a, auto b) const { return a + b; }
  constexpr auto operator()(auto first, auto second, auto... rest) const {
    return ((first + second) + ... + rest);
  }
};

// LAYER 2: Display Traits (new file: src/symbolic3/operator_display.h)
// Centralizes all display-related properties
template<typename Op>
struct DisplayTraits {
    static constexpr const char* symbol = "?";
    static constexpr DisplayMode mode = DisplayMode::kPrefix;
    static constexpr int precedence = 0;  // Lower = binds less tightly
};

template<>
struct DisplayTraits<AddOp> {
    static constexpr const char* symbol = "+";
    static constexpr DisplayMode mode = DisplayMode::kInfix;
    static constexpr int precedence = 10;
};

template<>
struct DisplayTraits<MulOp> {
    static constexpr const char* symbol = "*";
    static constexpr DisplayMode mode = DisplayMode::kInfix;
    static constexpr int precedence = 20;  // Binds tighter than +
};

// Usage in to_string.h - precedence-aware parenthesization
template<typename Op, Symbolic... Args>
std::string toStringRuntime(Expression<Op, Args...>) {
    return format_with_precedence(
        DisplayTraits<Op>::symbol,
        DisplayTraits<Op>::precedence,
        DisplayTraits<Op>::mode,
        Args...);
}
```

**Why NOT include commutativity/associativity here:**

⚠️ **Mathematical properties are context-dependent, NOT inherent to operators!**

- Matrix multiplication is NOT commutative, but scalar multiplication IS
- String concatenation is NOT commutative, but addition IS
- Commutativity depends on the algebraic structure (domain), not the operator
- Even if commutative, you need canonical ordering to prevent simplification loops:
  ```cpp
  // Without ordering: infinite loop!
  x + y  →  y + x  (apply commutativity)
  y + x  →  x + y  (apply commutativity again!)
  ```

**Solution:** Algebraic properties belong in the **context system**, not static traits.
Simplification rules should query context and use explicit canonical ordering.

**Benefits:**

- ✅ Clean separation: evaluation (operators) vs display (DisplayTraits)
- ✅ Operators become pure function objects (like `std::plus`)
- ✅ Precedence enables smarter parenthesization
- ✅ Foundation for multiple renderers (ASCII, LaTeX, MathML)
- ✅ No dangerous static algebraic assumptions
- ✅ Extensible without modifying operator definitions

**Implementation:**

- Files:
  - `src/symbolic3/operators.h` (remove `kSymbol`, `kDisplayMode`)
  - `src/symbolic3/operator_display.h` (new - all DisplayTraits)
  - `src/symbolic3/to_string.h` (use DisplayTraits, precedence-aware printing)
- Time: 3-4 days
- Risk: Low (backward compatible - old metadata can be deprecated gradually)

### 1.3 Improve Documentation (~0 LOC change, huge clarity gain)

**Add to README.md:**

````markdown
## Design Rationale: Why PatternVar?

### Symbol vs PatternVar: The Key Distinction

**Symbol** represents a **mathematical variable** (object level):

- Persistent identity: the same `x` in `x + y` and `x * 2`
- Evaluated with bindings: `evaluate(x + 1, {x = 5}) → 6`
- First-order term from λ-calculus

**PatternVar** represents a **pattern wildcard** (meta level):

- Ephemeral binding: matches during a single rewrite
- Never evaluated directly: exists only in rule definitions
- Second-order pattern from term rewriting systems

### Example: Why This Matters

```cpp
// Expression building (Symbols)
Symbol x, y;
auto expr = x + y;  // Mathematical expression

// Pattern matching (PatternVars)
PatternVar x_;
auto rule = Rewrite{x_ + 0_c, x_};  // Rewrite rule

// What if we used Symbols for both?
auto bad_rule = Rewrite{x + 0_c, x};
// Problem: Does 'x' mean "the specific variable x" or "any expression"?
// Ambiguous! PatternVar makes intent explicit.
```
````

### Theoretical Grounding

This design follows standard term rewriting theory:

- Baader & Nipkow, "Term Rewriting and All That" (Chapter 2)
- Separates object-level terms from meta-level patterns
- Same distinction in: SymPy (Symbol/Wild), Mathematica (x/\_)

````

**Benefits:**
- Prevents future "why do we have two types?" questions
- Educational for new contributors
- Grounds design in established theory

**Implementation:**
- Files: `src/symbolic3/README.md`, potentially new `DESIGN.md`
- Time: 1-2 days
- Risk: None

---

## Phase 2: Debugging Infrastructure (3 weeks)

### 2.1 Compile-Time Pretty Printing

**Goal:** Better error messages and debugging

**Implementation:**
```cpp
// In src/symbolic3/debug.h
template<Symbolic T>
struct PrettyType {
    static constexpr auto name() {
        if constexpr (is_symbol<T>) {
            return format("Symbol<{}>", get_symbol_name<T>());
        } else if constexpr (is_constant<T>) {
            return format("Constant<{}>", T::value);
        } else if constexpr (is_expression<T>) {
            return format("{}({})",
                         DisplayTraits<get_op_t<T>>::symbol,
                         format_args<get_args_t<T>>());
        }
    }
};

// Usage in static_assert
static_assert(match(pattern, expr),
              PrettyType<decltype(pattern)>::name() + " should match " +
              PrettyType<decltype(expr)>::name());
````

**Benefits:**

- Clear compile-time error messages
- Easier debugging of complex expressions
- Works with existing type system

**Implementation:**

- Files: `src/symbolic3/debug.h` (new utilities)
- Time: 1 week
- Risk: Low (additive only, no behavioral changes)

### 2.2 Expression Visualization

**Goal:** Understand complex expressions during development

```cpp
// In src/symbolic3/debug.h
template<Symbolic T>
void print_tree(std::ostream& os, int indent = 0) {
    if constexpr (is_expression<T>) {
        os << std::string(indent, ' ')
           << DisplayTraits<get_op_t<T>>::symbol << "\n";
        ([&] {
            print_tree<Args>(os, indent + 2);
        }(), ...);
    } else {
        os << std::string(indent, ' ') << to_string(T{}) << "\n";
    }
}

// Usage
print_tree<decltype(complex_expr)>(std::cout);
// Output:
//   +
//     *
//       x
//       2
//     1
```

**Benefits:**

- Visual understanding of expression structure
- Debug simplification rules
- Documentation aid

**Implementation:**

- Files: `src/symbolic3/debug.h`
- Time: 1 week
- Risk: Low (separate utility)

### 2.3 Transformation Tracing

**Goal:** Understand which rules apply during simplification

```cpp
// In src/symbolic3/strategy.h
template<typename Strategy>
struct TracingStrategy {
    Strategy inner;

    template<Symbolic Expr, typename Context>
    constexpr auto apply(Expr expr, Context ctx) const {
        auto result = inner.apply(expr, ctx);
        if constexpr (!isSame<Expr, decltype(result)>) {
            // Transformation occurred
            log_transformation(expr, result, Strategy::name());
        }
        return result;
    }
};

// Usage
auto traced = tracing(my_simplification_rule);
auto result = traced.apply(expr, ctx);
// Logs each transformation step
```

**Benefits:**

- Debug complex simplification pipelines
- Understand rule application order
- Performance profiling (count applications)

**Implementation:**

- Files: `src/symbolic3/strategy.h`, `src/symbolic3/debug.h`
- Time: 1 week
- Risk: Medium (touches strategy system, needs careful testing)

---

## Phase 3: Code Quality (2 weeks)

### 3.1 Add Comprehensive Concept Definitions

**Current:** Concepts used inconsistently

**Goal:** Standardize type constraints

```cpp
// In src/symbolic3/concepts.h (new file)

// Primary concepts (already exist)
template<typename T>
concept Symbolic = DerivedFrom<T, SymbolicTag>;

// Refined concepts (new)
template<typename T>
concept SymbolicValue = Symbolic<T> && !is_wildcard<T>;

template<typename T>
concept PatternType = is_symbol<T> || is_pattern_var<T> || is_wildcard<T>;

template<typename T>
concept CompoundExpr = is_expression<T>;

template<typename T>
concept AtomicExpr = is_symbol<T> || is_constant<T> || is_fraction<T>;

// Usage in function signatures
template<PatternType P, Symbolic S>
constexpr auto match(P pattern, S expr) { /* ... */ }

template<SymbolicValue A, SymbolicValue B>
constexpr auto operator+(A, B) { /* ... */ }
```

**Benefits:**

- Self-documenting signatures
- Better error messages (concept requirements)
- Enables more generic implementations

**Implementation:**

- Files: `src/symbolic3/concepts.h` (new), update others
- Time: 1 week
- Risk: Low (additive, improves clarity)

### 3.2 Standardize Naming Conventions

**Current inconsistencies:**

- `match()` vs `matchImpl()`
- `get_op` vs `getOp`
- `is_symbol` vs `isSymbol`

**Goal:** Consistent naming throughout

**Proposed convention (already mostly followed):**

- Free functions: `snake_case`
- Types: `PascalCase`
- Template parameters: `PascalCase`
- Variables: `snake_case`
- Constants: `kPascalCase` or `ALL_CAPS`

**Implementation:**

- Files: All symbolic3 headers
- Time: 1 week (mostly mechanical)
- Risk: Low (refactoring tool can help)

---

## Phase 4: Performance (2 weeks)

### 4.1 Benchmark Current System

**Goal:** Establish baseline before optimization

```cpp
// In test/benchmark_symbolic3.cpp
#include "profiler.h"

"Simplification performance"_test = [] {
    constexpr Symbol x;
    constexpr auto expr = x*x + 2_c*x + 1_c;

    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 1000; ++i) {
        auto result = simplify(expr, default_context());
    }
    auto end = std::chrono::high_resolution_clock::now();

    print_benchmark_result("simplify(x*x + 2*x + 1)", end - start, 1000);
};
```

**Metrics to collect:**

- Compilation time (per test file)
- Template instantiation depth (via compiler flags)
- Binary size
- Runtime performance (for non-constexpr paths)

**Implementation:**

- Files: `test/benchmark_symbolic3.cpp` (new)
- Time: 3 days
- Risk: None

### 4.2 Profile and Optimize Hot Paths

**After benchmarking, identify slow areas:**

Likely candidates:

- `simplify()` pipeline (most complex)
- `match()` on large expressions
- `substitute()` with many bindings

**Optimization techniques:**

- Extern template for common instantiations
- Memoization for repeated subexpressions
- Short-circuit evaluation in match()

**Implementation:**

- Files: Various (data-driven)
- Time: 1 week
- Risk: Low (optimize only proven bottlenecks)

---

## Phase 5: Testing and Validation (1 week)

### 5.1 Increase Test Coverage

**Current:** 9 tests passing, 14 not built

**Goals:**

1. Fix build configuration (all tests should build)
2. Add property-based tests
3. Add edge case tests

**Example property test:**

```cpp
"Simplification is idempotent"_test = [] {
    constexpr Symbol x;
    auto expr = generate_random_expr<x>();

    auto simplified_once = simplify(expr, default_context());
    auto simplified_twice = simplify(simplified_once, default_context());

    static_assert(isSame<decltype(simplified_once),
                         decltype(simplified_twice)>);
};
```

**Implementation:**

- Files: `src/symbolic3/test/*`
- Time: 1 week
- Risk: None (only adds tests)

---

## Timeline and Effort

| Phase                             | Duration     | Risk    | LOC Change | Value     |
| --------------------------------- | ------------ | ------- | ---------- | --------- |
| Phase 1: Low-Hanging Fruit        | 2 weeks      | Low     | -150       | High      |
| Phase 2: Debugging Infrastructure | 3 weeks      | Low-Med | +300       | Very High |
| Phase 3: Code Quality             | 2 weeks      | Low     | +100/-50   | Medium    |
| Phase 4: Performance              | 2 weeks      | Low     | +50        | Medium    |
| Phase 5: Testing                  | 1 week       | None    | +200       | High      |
| **Total**                         | **10 weeks** | **Low** | **+450**   | **High**  |

**Note on LOC change:** Total lines increase slightly (+450), but:

- Code quality improves significantly
- Debugging capabilities expand greatly
- Technical debt reduces
- **Lines of code is not the goal - code quality is**

---

## Success Metrics

### Quantitative

- ✅ All 23 tests building and passing
- ✅ Compilation time change: ±5% (measure before/after)
- ✅ Test coverage: >80% (from current ~60%)
- ✅ Match overload count: reduced by ~15

### Qualitative

- ✅ Better error messages (user survey)
- ✅ Easier to add new operators (time measurement)
- ✅ Clearer documentation (review by new contributors)
- ✅ Improved debugging workflow (developer feedback)

---

## What NOT to Do

❌ **Context-aware types** - Theoretically flawed
❌ **Eliminate PatternVar** - Loses important distinction
❌ **Radical type unification** - Conflates object/meta levels
❌ **48% LOC reduction goal** - Arbitrary and unrealistic
❌ **6-week timeline for redesign** - Too aggressive for testing

**Why not?** See SYMBOLIC3_REDESIGN_CRITIQUE.md for detailed analysis.

---

## Comparison to "Core Redesign" Proposal

| Aspect            | Core Redesign                  | This Roadmap                    |
| ----------------- | ------------------------------ | ------------------------------- |
| **Risk**          | High (breaks everything)       | Low (incremental)               |
| **LOC change**    | -48% (unrealistic)             | +7% (quality > quantity)        |
| **Timeline**      | 6 weeks (rushed)               | 10 weeks (careful)              |
| **Theory**        | Unsound (violates principles)  | Sound (follows best practices)  |
| **Testing**       | "All tests pass" (hope)        | Extensive testing at each phase |
| **Reversibility** | Hard (major rewrite)           | Easy (small changes)            |
| **Debugging**     | Worse (context errors)         | Better (new utilities)          |
| **Value**         | Negative (breaks working code) | Positive (real improvements)    |

---

## Getting Started

### Week 1-2: Phase 1 Implementation

**Day 1-2:** Match overload deduplication

- Create `src/symbolic3/concepts.h`
- Refactor `src/symbolic3/matching.h`
- Run tests, verify no regressions

**Day 3-5:** Operator display traits

- Create `src/symbolic3/operator_display.h` with `DisplayTraits`
- Remove `kSymbol` and `kDisplayMode` from operator definitions
- Specialize `DisplayTraits` for all operators (add precedence)
- Update `to_string.h` to use `DisplayTraits` with precedence-aware printing

**Day 6-8:** Documentation improvements

- Update `src/symbolic3/README.md`
- Create `src/symbolic3/DESIGN.md`
- Add examples of Symbol vs PatternVar

**Day 9-10:** Review and testing

- Code review
- Performance check
- Documentation review

### Progress Tracking

Create issues for each phase:

- [ ] Phase 1.1: Match overload deduplication
- [x] **Phase 1.2: Operator display traits (separation of concerns)** ✅ **COMPLETED** (Oct 21, 2025)
- [ ] Phase 1.3: Documentation improvements
- [ ] Phase 2.1: Compile-time pretty printing
- [ ] Phase 2.2: Expression visualization
- [ ] Phase 2.3: Transformation tracing
- [ ] Phase 3.1: Concept definitions
- [ ] Phase 3.2: Naming conventions
- [ ] Phase 4.1: Benchmarking
- [ ] Phase 4.2: Optimization
- [ ] Phase 5.1: Test coverage

---

## Conclusion

**This roadmap delivers real value:**

- ✅ Improves code quality
- ✅ Enhances debugging capabilities
- ✅ Reduces technical debt
- ✅ Maintains theoretical soundness
- ✅ Preserves zero-cost abstractions
- ✅ Incremental and testable

**Without the risks of "core redesign":**

- ❌ No breaking changes
- ❌ No theoretical violations
- ❌ No inflated promises
- ❌ No rushed timeline

**The best code improvements are invisible to users but make developers' lives significantly better.**

Let's build on what works, fix what doesn't, and avoid unnecessary risk.

---

**Ready to start? Begin with Phase 1.1 - it's low-risk and high-value.**
