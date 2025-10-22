# Symbolic3 Improvement Roadmap

**Date:** October 20, 2025
**Status:** ✅ RECOMMENDED - Evidence-Based, Low-Risk Improvements
**Goal:** Improve symbolic3 without radical redesign

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

### 1.2 Add Operator Metadata (~100 line reduction, better maintainability)

**Current problem:**

- `to_string()` has hardcoded operator symbols
- Precedence scattered across ordering functions
- Commutativity checked in multiple places

**Solution:**

```cpp
// In src/symbolic3/operators.h
template<typename Op>
struct OperatorTraits {
    static constexpr const char* symbol = "?";
    static constexpr int precedence = 0;
    static constexpr bool commutative = false;
    static constexpr bool associative = false;
};

// Specializations
template<>
struct OperatorTraits<AddOp> {
    static constexpr const char* symbol = "+";
    static constexpr int precedence = 10;
    static constexpr bool commutative = true;
    static constexpr bool associative = true;
};

// Usage in to_string.h
template<typename Op, Symbolic... Args>
std::string to_string(Expression<Op, Args...>) {
    return format_with_precedence(OperatorTraits<Op>::symbol,
                                   OperatorTraits<Op>::precedence,
                                   Args...);
}
```

**Benefits:**

- Single source of truth for operator properties
- Simplifies to_string, ordering, simplification
- Easier to add new operators
- Self-documenting

**Implementation:**

- Files: `src/symbolic3/operators.h`, `src/symbolic3/to_string.h`
- Time: 3-4 days
- Risk: Low (additive change, backward compatible)

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
                         OperatorTraits<get_op_t<T>>::symbol,
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
           << OperatorTraits<get_op_t<T>>::symbol << "\n";
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

**Day 3-5:** Operator metadata

- Add `OperatorTraits` to `src/symbolic3/operators.h`
- Specialize for all operators
- Update `to_string.h` to use traits

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
- [ ] Phase 1.2: Operator metadata
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
