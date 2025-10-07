# Symbolic3: Next Steps and Improvements

**Date:** October 5, 2025
**Status:** Phase 1 Complete - All Tests Passing (10/10)
**Purpose:** Living document for future development priorities
**Last Updated:** October 5, 2025 - Added exact mathematics priorities

---

## CRITICAL: Exact Mathematics Design Principles

### Core Philosophy

**Symbolic3 must maintain mathematical exactness by default.** Numerical approximations should be explicit opt-ins, not implicit conversions.

### Key Principles

1. **No Implicit Float Evaluation**

   - `1/3` should remain as a symbolic fraction, NOT evaluate to `0.333...`

2. **Fractions as First-Class Values**

   - Need `Fraction<numerator, denominator>` type (compile-time rational numbers)
   - Literal suffix: `0.5_c` → `Fraction<1, 2>` (not float `0.5`)
   - Automatic GCD reduction: `Fraction<4, 6>` → `Fraction<2, 3>`

3. **Type Absorption Rules for Numeric Constants**

   - Floats absorb integers: `2.5 * 3` → float result type
   - Floats absorb floats: `2.5 * 3.5` → float result type
   - BUT: Evaluation should still be delayed wth pi and other symbols unless explicitly requested
   - Fractions promote to floats only when mixed: `Fraction<1,2> * 2.5` → float

4. **Exact Symbolic Constants**
   - Added `π` and `e` as zero-argument expressions (not numeric approximations)
   - These should remain symbolic until explicit numerical evaluation
   - Example: `π * 2` → `2π` (symbolic), not `6.283...`

### Implementation Status

✅ **Completed:**

- π and e added as `Expression<PiOp>` and `Expression<EOp>` (Oct 5, 2025)
- Zero-arg operations infrastructure in place

✅ **COMPLETED (October 5, 2025):**

- [x] Implement `Fraction<N, D>` template type in `core.h`
- [x] Add GCD reduction for fraction simplification (automatic at construction)
- [x] Add literal suffix `_frac` for creating fractions: `1_frac / 2_frac`
- [x] Add tests for exact arithmetic behavior (`fraction_test.cpp`)
- [x] Fraction arithmetic operators (+, -, \*, /, negation)
- [x] Mixed arithmetic with Constants (Fraction + Constant)
- [x] Comparison operators for fractions

❌ **TODO (High Priority):**

- [ ] **Re-evaluate constant folding strategy for division**

  - **Current behavior**: Division of integers may fold to float (INCORRECT for exact math)
  - **Desired behavior**:
    - `int / int` where result is exact integer → fold to integer (e.g., `6 / 2` → `3`)
    - `int / int` where result is non-integer → **create `Fraction<N, D>`** (e.g., `5 / 2` → `Fraction<5, 2>`)
    - Integer division requires **explicit `floor()`** call: `floor(5 / 2)` → `2`
    - Float involved → fold to float (e.g., `5.0 / 2` → `2.5`)
  - **Philosophy**: Assume exact math by default; floats must be explicit
  - **Implementation**: Add `numeric_div_to_fraction` rewrite rule in `simplify.h`
  - **Pattern**: `Constant<n> / Constant<d>` where `n % d != 0` → `Fraction<n, d>`

- [ ] Update type absorption rules in `simplify.h` for float/int/fraction mixing
- [ ] Add symbolic division → fraction promotion during simplification
- [ ] Add `floor()`, `ceil()`, `round()` operators for explicit integer division

- [ ] **Explore specialized fraction routines leveraging reduced-form invariant**
  - **Observation**: `Fraction<N, D>` is ALWAYS in reduced form (GCD reduction at construction)
  - **Opportunity**: Can skip GCD computation in many operations since we know inputs are reduced
  - **Examples**:
    - Fraction equality: Just compare numerators and denominators (no cross-multiply needed)
    - Fraction comparison: Can optimize when both fractions share same denominator
    - Addition optimization: `a/b + c/b = (a+c)/b` (same denominator - no GCD needed)
  - **Trade-off**: Reduced form is guaranteed by template system, but operations may temporarily create unreduced intermediate results that get re-reduced
  - **Question**: Can we avoid redundant GCD calls? Or does compile-time optimization already handle this?
  - **Investigation**: Benchmark fraction arithmetic vs. naive implementation to measure actual overhead

### Files Modified

- **`src/symbolic3/core.h`**: Added `Fraction<N, D>` struct with GCD reduction ✅
- **`src/symbolic3/fraction.h`**: Added operators and `_frac` literal ✅
- **`src/symbolic3/test/fraction_test.cpp`**: Comprehensive test suite ✅

### Files to Modify

- **`src/symbolic3/simplify.h`**:
  - Modify `ConstantFold` to check divisibility before folding division
  - Add `FractionPromote` strategy: numeric division → Fraction when non-integer
- **`src/symbolic3/operators.h`**: Add `floor()`, `ceil()`, `round()` for explicit rounding

---

## Current State Assessment

### ✅ What's Working Well

1. **Core Infrastructure** (solid foundation)

   - Combinator-based strategy system with clean concept-based design
   - Type-safe context system with compile-time tags and data-driven modes
   - Pattern matching with wildcards and structural matching
   - Composition operators (`>>`, `|`, `when`) for building pipelines
   - Multiple traversal strategies (Fold, Unfold, Innermost, Outermost, FixPoint)

2. **Transformation Capabilities**

   - Constant folding
   - Algebraic simplification (identity laws, zero/one rules)
   - Trigonometric simplifications
   - Symbolic differentiation with chain rule support
   - String conversion and debugging utilities

3. **Test Coverage**
   - 10 test suites, all passing
   - Good coverage of core functionality
   - constexpr validation where appropriate

### 🔧 What Needs Work

#### 1. **Evaluation System** (Completed!)

- **Status:** ✅ Fully implemented (`evaluate.h` with BinderPack system)
- **Features:**
  - Heterogeneous binding system (e.g., `evaluate(expr, BinderPack{x=5, y=3.0})`)
  - Full compile-time and runtime evaluation support
  - Type-safe value substitution
- **Use Cases:**
  - Numerical optimization (evaluate objective functions)
  - Plotting (x→y mappings)
  - Verification (compare symbolic vs numeric results)

#### 2. **Simplification Gaps**

- **Missing Rules:**
  - Logarithm laws: `log(a*b) → log(a) + log(b)`, `log(a^n) → n*log(a)`
  - Exponential laws: `exp(a+b) → exp(a)*exp(b)`
  - Trig identities: `sin²+cos²=1`, double angle formulas
  - Rational function simplification
- **Ordering Issues:**
  - No canonical form for commutative operations (x+y vs y+x treated as different)
  - Term collection not implemented (2*x + 3*x doesn't simplify to 5\*x)
- **Context Sensitivity:**
  - Domain-specific rules (e.g., modular arithmetic) partially implemented but untested

#### 3. **Performance Concerns**

- **Compile-Time:**
  - Complex expressions likely cause template instantiation bloat
  - No measurement of compile-time performance yet
  - May need compile-time complexity budgets
- **Type Size:**
  - Deeply nested expressions create huge type names
  - No type compression or sharing (each subexpression is unique type)

#### 4. **User Experience**

- **Error Messages:**
  - Template errors are cryptic (inherent to approach but could improve)
  - No validation of malformed expressions at entry points
- **Documentation:**
  - Three README files in symbolic3/ directory (redundant)
  - Examples exist but could be more comprehensive
  - No tutorial for common tasks

---

## Priority Roadmap

### Phase 2: Core Functionality (Essential for Real Use)

#### P0: Exact Mathematics Foundation (6-8 hours) **NEW PRIORITY**

**Goal:** Implement exact arithmetic with fractions and prevent premature float evaluation

**Why This Matters:**

- Symbolic computation loses meaning if `1/3` becomes `0.333...`
- Exactness is fundamental to computer algebra systems
- Current constant folding is too aggressive

**Implementation Tasks:**

1. **Fraction Type** (2-3 hours)

   ```cpp
   template <std::intmax_t N, std::intmax_t D = 1>
   struct Fraction : SymbolicTag {
     static_assert(D != 0, "Denominator cannot be zero");
     static constexpr auto numerator = N;
     static constexpr auto denominator = D;
   };
   ```

2. **GCD-based Simplification** (1-2 hours)

   ```cpp
   // Reduce Fraction<4, 6> → Fraction<2, 3>
   constexpr auto gcd(auto a, auto b) { /* Euclidean algorithm */ }
   constexpr auto reduce_fraction = Rewrite{
     Fraction<n_, d_>,
     /* normalized form with gcd reduction */
   };
   ```

3. **Restrict Constant Folding** (2 hours)

   - Modify `foldConstants()` to skip divisions that produce non-integers
   - Add predicate: only fold if result is representable exactly
   - Example: `6 / 2` → `3` ✓, but `5 / 2` → `Fraction<5, 2>` ✓

4. **Literal Syntax** (1 hour)

   ```cpp
   constexpr auto operator""_frac(unsigned long long n) {
     return Fraction<n, 1>{};
   }
   // Usage: 1_frac / 2_frac → Fraction<1, 2>
   ```

5. **Type Absorption Rules** (1-2 hours)
   - Float + Int → Float (but keep symbolic)
   - Fraction + Int → Fraction (via implicit conversion)
   - Fraction + Float → Float (upgrade to float)

**Files to Create/Modify:**

- `src/symbolic3/core.h` (add Fraction)
- `src/symbolic3/simplify.h` (modify ConstantFold, add FractionSimplify)
- `src/symbolic3/operators.h` (literal suffixes)
- `src/symbolic3/test/exact_math_test.cpp` (new comprehensive test)

**Success Criteria:**

- `1 / 3` remains symbolic or becomes `Fraction<1, 3>` (NOT `0.333...`)
- `Fraction<4, 6>` simplifies to `Fraction<2, 3>`
- `π * 2` stays symbolic (no `6.283...`)
- All existing tests still pass

---

#### ✅ P1: Evaluation System - **COMPLETED**

**Status:** Fully implemented with BinderPack system

**Implementation:**

```cpp
// Working API:
Symbol x, y;
auto expr = x*x + y;
auto result = evaluate(expr, BinderPack{x=2.0, y=3.0});  // → 7.0

// Compile-time support:
constexpr auto result = evaluate(expr, BinderPack{x=Constant<2>{}, y=Constant<3>{}});
```

**Features:**

1. ✅ BinderPack concept with heterogeneous type support
2. ✅ Recursive evaluation through expression trees
3. ✅ Handles int, double, Constant<N> values
4. ✅ Full constexpr support for compile-time evaluation

**Files:**

- `src/symbolic3/evaluate.h` (75 lines, fully implemented)
- `src/symbolic3/test/evaluate_test.cpp` (tests passing)

---

#### ✅ P2: Term Collection and Simplification - **COMPLETED** (Oct 5, 2025)

**Status:** Fully implemented with constant folding integration

**Goal:** Collect like terms and create canonical forms ✅

```cpp
// Now working:
auto expr = 2_c*x + 3_c*x;
auto simplified = simplify(expr);  // → 5*x ✅

auto expr2 = x + y + x;
auto simplified2 = simplify(expr2);  // → 2*x + y ✅

// More complex cases:
auto expr3 = 2_c*x + 3_c*y + 4_c*x + 5_c*y + 6_c*x;
auto simplified3 = simplify(expr3);  // → 12*x + 8*y ✅
```

**Implementation:**

1. ✅ Bidirectional associativity rules with ordering predicates
2. ✅ Canonical ordering using `lessThan()` comparisons
3. ✅ Factoring rules: `x*a + x*b → x*(a+b)`
4. ✅ Constant folding using C++26 `evaluate(expr, BinderPack{})`
5. ✅ Outer FixPoint wrapper to handle newly created subexpressions

**Files Modified:**

- `src/symbolic3/simplify.h` - Added factoring, ordering, constant folding
- `src/symbolic3/ordering.h` - Fixed forward declarations
- `src/symbolic3/test/term_collecting_debug.cpp` - Comprehensive test suite (10 tests, all passing)

**Note:** With exact mathematics (P0), constant folding will need adjustment to respect fraction semantics.

---

#### P3: Enhanced Term Collection with Fractions (2-3 hours) **DEPENDS ON P0**

**Goal:** Extend term collection to work with fractional coefficients

---

#### ✅ P2: Extended Simplification Rules - **COMPLETED** (Oct 5, 2025)

**Status:** All requested rules implemented and tested (12/12 tests passing)

**Logarithm Laws:** ✅ Implemented

- `log(a * b) → log(a) + log(b)` (already existed)
- `log(a / b) → log(a) - log(b)` (newly added)
- `log(a^n) → n * log(a)` (already existed)
- `log(exp(x)) → x` (already existed)

**Exponential Laws:** ✅ Implemented

- `exp(a + b) → exp(a) * exp(b)` (newly added)
- `exp(a - b) → exp(a) / exp(b)` (newly added)
- `exp(n * log(a)) → a^n` (newly added)

**Trigonometric Identities:** ✅ Implemented

- `sin²(x) + cos²(x) → 1` (newly added - Pythagorean identity)
- `sin(2*x) → 2*sin(x)*cos(x)` (newly added)
- `cos(2*x) → cos²(x) - sin²(x)` (newly added)
- `tan(x) → sin(x) / cos(x)` (newly added)

**Implementation Details:**

- Added rules in `src/symbolic3/simplify.h`
- Created new `PythagoreanRuleCategories` namespace
- Extended `ExpRuleCategories`, `LogRuleCategories`, `SinRuleCategories`, `CosRuleCategories`, `TanRuleCategories`
- Integrated into existing `transcendental_simplify` pipeline
- Comprehensive test suite in `src/symbolic3/test/advanced_simplify_test.cpp` (28 test cases)
- All tests passing, no regressions

**Documentation:** See `/home/ulins/tempura/SIMPLIFICATION_GAPS_IMPLEMENTATION.md` for full details

---

### Phase 3: Integration and Usability (Nice to Have)

#### Integration with Autodiff

- **Goal:** Use symbolic3 for automatic differentiation instead of dual numbers
- **Benefit:** True higher-order derivatives, not just forward/reverse mode
- **Challenge:** Bridge compile-time (symbolic3) with runtime (autodiff graphs)

#### Matrix/Vector Support

- **Goal:** Symbolic matrix operations (Jacobians, Hessians)
- **API:**
  ```cpp
  SymbolicMatrix<2, 2> M = {{x, y}, {y, x}};
  auto det = determinant(M);  // → x² - y²
  ```
- **Challenge:** Blending symbolic3 with matrix2 abstractions

#### Optimization Integration

- **Goal:** Use symbolic derivatives in optimization algorithms
- **Use Case:** Newton's method with exact Hessians
- **Benefit:** No finite-difference approximations

#### Rational Function Simplification

- **Goal:** Simplify expressions like `(x²-1)/(x-1) → x+1`
- **Challenge:** Polynomial division and factorization at compile-time
- **Approach:** GCD algorithms, partial fraction decomposition

---

### Phase 4: Advanced Features (Research Territory)

#### Compile-Time Performance Optimization

- **Profile:** Measure template instantiation depth and time
- **Optimize:**
  - Expression sharing (common subexpression elimination in types)
  - Type compression (use integers instead of nested templates where possible)
  - Lazy evaluation (defer simplification until needed)

#### Automatic Rule Learning

- **Concept:** Let users define custom rewrite rules via pattern DSL
- **API:**
  ```cpp
  auto my_rule = rule(pattern(sin(x)*sin(x) + cos(x)*cos(x)), 1_c);
  auto my_simplify = algebraic_simplify | my_rule;
  ```

#### Computer Algebra System Features

- **Polynomial Manipulation:** Factorization, GCD, resultants
- **Series Expansion:** Taylor/Laurent series
- **Integration:** Pattern-matching integration (table of integrals)
- **Equation Solving:** Symbolic equation solver (limited scope)

#### Proof System Integration

- **Goal:** Generate proof certificates for transformations
- **Use:** Verify that simplifications preserve semantics
- **Approach:** Each transformation step records justification

---

## Technical Debt and Cleanup

### Code Organization

- [x] Three markdown files in symbolic3/ (this document consolidates them)
- [ ] Many debug test files (debug\_\*.cpp) - consolidate or document purpose
- [ ] Duplicate logic between v1 and v2 strategy systems?

### Documentation

- [ ] Consolidate README.md, DEBUG_QUICKREF.md, TO_STRING_README.md
- [ ] Add comprehensive examples/symbolic3_tutorial.cpp
- [ ] Document performance characteristics (compile-time complexity)
- [ ] Add performance comparison benchmarks (compile-time vs runtime overhead)

### Testing

- [ ] Add property-based tests (e.g., diff(integrate(f, x), x) ≈ f)
- [ ] Benchmark compile times for large expressions
- [ ] Add tests for error cases and edge conditions
- [ ] Test interaction between different simplification strategies

### API Refinement

- [ ] Decide on one strategy API (v1 CRTP vs v2 concepts) - v2 seems to have won
- [ ] Remove unused/experimental files (v2_design_test.cpp, etc.)
- [ ] Standardize naming: `diff` vs `differentiate`, `simplify` vs `simplify_expr`
- [ ] Consider operator overloading for common patterns (e.g., `∂(expr)/∂x` syntax)

---

## Research Questions

### 1. **How far can we push compile-time computation?**

- At what expression complexity do compile times become prohibitive?
- Can we use C++26 features (reflection, constexpr vectors) to improve this?
- Should we have a "complexity budget" and automatic runtime fallback?

### 2. **Type-based vs Value-based?**

- Current: Expressions are types (zero runtime overhead)
- Alternative: Expressions are values with type-erased storage (faster compilation)
- Hybrid: Small expressions as types, large ones as values?

### 3. **Type System Optimization**

- Can we reduce template instantiation depth?
- Is there a way to share common subexpressions at the type level?
- Should we explore value-based alternatives for large expressions?

### 4. **Connection to Meta Programming?**

- Can we generalize the pattern matching system for non-symbolic types?
- Is the rewrite system useful for other domains (e.g., type-level list manipulation)?
- Should we extract a standalone "meta-rewrite" library?

---

## Non-Goals (Explicit Scope Limitations)

### What Symbolic3 Should NOT Try to Do

1. **General-purpose CAS:** Not competing with Mathematica/Maple
2. **Numerical computation:** Use matrix2, autodiff, or specialized libraries
3. **Symbolic integration:** Too hard; focus on differentiation
4. **Theorem proving:** Out of scope (but proof certificates could be useful)
5. **Runtime flexibility:** Symbolic3 is compile-time; for runtime expressions, consider value-based alternatives

---

## Decision Log

### Decisions Made

- **2025-10-05:** Chose concept-based strategy (v2) over CRTP (v1)
  - Reason: Simpler, cleaner, better error messages
- **2025-10-05:** Implemented differentiation before integration
  - Reason: Differentiation is algorithmically simpler
- **2025-10-05:** Used pattern matching for simplification rules
  - Reason: More declarative than if-else chains

### Open Decisions

- **Evaluation API:** BinderPack vs named parameters vs something else?
- **Canonical forms:** Lexicographic ordering vs algebraic normal form?
- **Error handling:** static_assert vs SFINAE vs concepts?
- **Symbol naming:** Keep auto-generated IDs (x123) or user-provided names?

---

## How to Contribute

### Adding a New Simplification Rule

1. **Pattern:** Define what you're matching

   ```cpp
   // Example: sin(-x) → -sin(x)
   auto pattern = sin(neg(x_));
   ```

2. **Replacement:** Define the result

   ```cpp
   auto replacement = neg(sin(x_));
   ```

3. **Strategy:** Wrap in a strategy

   ```cpp
   struct SinNegRule {
     template <Symbolic S, typename Context>
     constexpr auto apply(S expr, Context ctx) const {
       if (auto [matched, x_val] = match(expr, pattern)) {
         return replacement; // bind x_ from match
       }
       return expr;
     }
   };
   ```

4. **Integrate:** Add to simplification pipeline

   ```cpp
   constexpr auto trig_rules = SinNegRule{} | ... | existing_rules;
   ```

5. **Test:** Add test cases
   ```cpp
   "sin(-x) simplifies to -sin(x)"_test = [] {
     Symbol x;
     auto expr = sin(neg(x));
     auto result = trig_rules.apply(expr, default_context());
     static_assert(match(result, neg(sin(x))));
   };
   ```

### Adding a New Operator

1. Define operator tag in `operators.h`
2. Add operator overload for symbolic types
3. Implement differentiation rule in `derivative.h`
4. Add simplification rules in `simplify.h`
5. Add to_string support in `to_string.h`
6. Write tests

---

## Learning Resources

### Internal References

- **Examples:**
  - `examples/symbolic3_simplify_demo.cpp` - Simplification pipelines
  - `examples/symbolic3_debug_demo.cpp` - Debugging and visualization
  - `examples/advanced_simplify_demo.cpp` - Advanced simplification techniques

### External References

- **Combinator Pattern:** "Thinking with Types" by Sandy Maguire (Haskell focus but applicable)
- **Term Rewriting:** "Term Rewriting and All That" by Baader & Nipkow
- **Symbolic Computation:** "Computer Algebra" by Cox, Little, O'Shea
- **Template Metaprogramming:** "C++ Templates: The Complete Guide" by Vandevoorde, Josuttis, Gregor

---

## Metrics and Success Criteria

### Phase 2 Success Metrics

- [ ] `evaluate()` function with heterogeneous bindings implemented and tested
- [ ] Like-term collection working: `2*x + 3*x → 5*x`
- [x] At least 10 additional simplification rules added ✅ (10 new rules added)
- [x] No test regressions (all existing tests still pass) ✅ (12/12 tests passing)
- [x] Compile time for test suite < 10 seconds (on reference machine) ✅ (<1 sec per test)

### Phase 3 Success Metrics

- [ ] Integration example with optimization library working
- [ ] Matrix symbolic operations demonstrated
- [ ] At least one real-world use case implemented

### Long-term Health Metrics

- Compile-time performance: Track over time, set budget
- Test coverage: Maintain > 80% of public API
- Documentation: Every public function has usage example
- Usability: External users can add custom rules without modifying core

---

## Appendix: Code Size Analysis

```
Total Lines of Code (src/symbolic3/):
  core.h                101
  operators.h           223
  pattern_matching.h    417
  matching.h            236
  strategy.h            309
  context.h             296
  traversal.h           374
  simplify.h            598
  derivative.h          422
  to_string.h           508
  transforms.h          214
  constants.h           64
  evaluate.h            12  (stub!)
  ordering.h            134
  --------------------------------
  Total:                ~3,908 lines

Test Code (src/symbolic3/test/):
  ~25 test files, ~2,000 lines total

Ratio: ~2:1 implementation to test code
Goal: Increase test coverage to 1:1 or better
```

---

## Contact and Collaboration

This is a living document. As symbolic3 evolves:

- Update priority rankings based on actual needs
- Move completed items from "Next Steps" to decision log
- Add new research questions as they arise
- Document surprising findings or gotchas

**Last Updated:** October 5, 2025
**Next Review:** When starting Phase 2 work

---

# Appendix: README.md Content

# Symbolic3: Combinator-Based Symbolic Computation

**Status:** ✅ Phase 1 Complete - All Tests Passing (10/10)
**Next Steps:** See `NEXT_STEPS.md` for development roadmap

---

## Quick Start

```cpp
#include "symbolic3/symbolic3.h"
using namespace tempura::symbolic3;

// Create symbols
Symbol x, y;

// Build expressions
auto expr = sin(x) + Constant<2>{} * y;

// Apply transformations
auto simplified = algebraic_simplify.apply(expr, default_context());

// Debug output
debug_print(simplified, "result");  // result: (sin(x123) + (2 * x124))
```

---

## Table of Contents

1. [Overview](#overview)
2. [Core Concepts](#core-concepts)
3. [API Reference](#api-reference)
4. [Examples](#examples)
5. [Architecture](#architecture)
6. [Debugging Guide](#debugging-guide)
7. [Testing](#testing)
8. [Design Rationale](#design-rationale)
9. [Next Steps](#next-steps)

---

## Overview

Symbolic3 is a complete redesign of tempura's symbolic computation system using a **combinator-based architecture**. It provides compile-time symbolic manipulation with zero runtime overhead.

### Key Features

- **Generic recursion combinators** (fixpoint, fold, unfold, innermost, outermost)
- **Composable transformation pipelines** (sequential `>>`, choice `|`, conditional `when`)
- **Context-aware transformations** (domain-specific rules, depth tracking)
- **User-extensible strategies** (add custom transformations without modifying core)
- **Full compile-time evaluation** (all operations are `constexpr`)

### What's Implemented (Phase 1)

✅ Core symbolic types (Symbol, Constant, Expression)
✅ Pattern matching with wildcards
✅ Strategy system with concepts (no CRTP)
✅ Context system with type-safe tags
✅ Traversal combinators (8 strategies)
✅ Basic simplification (algebraic, trigonometric)
✅ Symbolic differentiation with chain rule
✅ String conversion and debugging utilities
✅ 10 test suites with 100% pass rate

### What's Missing (See NEXT_STEPS.md)

🔧 Evaluation system (cannot substitute values yet)
🔧 Term collection (2*x + 3*x doesn't simplify to 5\*x)
🔧 Extended simplification rules (log laws, exp laws, advanced trig)
🔧 Integration with other tempura libraries

---

## Core Concepts

### 1. Symbolic Types

All symbolic values are **types**, not runtime values:

```cpp
Symbol<> x;              // Type: Symbol<lambda123>
Constant<42> c;          // Type: Constant<42>
auto expr = x + c;       // Type: Expression<AddOp, Symbol<...>, Constant<42>>
```

**Benefits:**

- Zero runtime overhead
- Compile-time computation
- Type-level uniqueness (each symbol is distinct type)

### 2. Strategy Pattern

Transformations are **strategies** implementing a simple concept:

```cpp
template <typename S>
concept Strategy = requires(S const& strat, Symbol<> sym, TransformContext<> ctx) {
  { strat.apply(sym, ctx) } -> Symbolic;
};
```

Any type with an `apply(expr, ctx)` method is a strategy.

**Example:**

```cpp
struct MyStrategy {
  template <Symbolic S, typename Context>
  constexpr auto apply(S expr, Context ctx) const {
    // Your transformation logic here
    return expr;  // or transformed version
  }
};
```

### 3. Context System

Context carries state through transformations:

```cpp
auto ctx = default_context()
  .increment_depth<1>()                    // Track recursion depth
  .with(SimplificationMode{
    .constant_folding_enabled = true,
    .algebraic_simplification_enabled = true
  });

// Check context
if constexpr (ctx.mode.constant_folding_enabled) { /* ... */ }
```

**Context factories:**

- `default_context()` - Balanced settings
- `numeric_context()` - Aggressive numeric evaluation
- `symbolic_context()` - Preserve symbolic forms

### 4. Composition Operators

Build complex transformations from simple pieces:

```cpp
// Sequential: apply in order
auto pipeline = strategy1 >> strategy2 >> strategy3;

// Choice: try first, fallback to second if no change
auto alternatives = try_this | or_this | or_that;

// Conditional: apply only if predicate holds
auto conditional = when([](auto expr) { return is_symbol(expr); }, strategy);
```

### 5. Traversal Combinators

Control how transformations recurse:

```cpp
fold(strategy)          // Bottom-up (children first)
unfold(strategy)        // Top-down (parent first)
innermost(strategy)     // Apply at leaves first, recursively
outermost(strategy)     // Apply at root first, recursively
fixpoint(strategy)      // Repeat until no change
bottomup(strategy)      // Post-order traversal
topdown(strategy)       // Pre-order traversal
```

---

## API Reference

### Creating Expressions

```cpp
// Symbols
Symbol x, y, z;

// Constants
Constant<42> c1;
auto c2 = 3_c;  // Constant<3> via user-defined literal

// Arithmetic
auto sum = x + y;
auto product = x * y;
auto power = pow(x, 2_c);
auto negation = -x;

// Trigonometric
auto s = sin(x);
auto c = cos(x);
auto t = tan(x);

// Exponential/Logarithm
auto e = exp(x);
auto l = log(x);

// Hyperbolic
auto sh = sinh(x);
auto ch = cosh(x);
auto th = tanh(x);
```

### Pattern Matching

```cpp
// Wildcards
AnyArg x_;        // Matches any symbolic value
AnyExpr expr_;    // Matches compound expressions only
AnyConstant c_;   // Matches constants only
AnySymbol s_;     // Matches symbols only

// Matching
if (auto [matched, bindings] = match(expr, pattern); matched) {
  // Extract bound values from bindings
}

// Example: match sin(x)
auto [matched, x_val] = match(expr, sin(x_));
if (matched) {
  // x_val contains the argument to sin
}
```

### Predefined Strategies

```cpp
// Basic transformations
Identity{}              // No-op
Fail{}                  // Always fails

// Algebraic simplification
FoldConstants{}         // Evaluate constant arithmetic
ApplyAlgebraicRules{}   // Apply identities (x+0→x, x*1→x, etc.)
algebraic_simplify      // Pre-built pipeline

// Recursive simplification
algebraic_simplify_recursive   // With bottom-up traversal
bottomup_simplify              // Bottom-up traversal
topdown_simplify               // Top-down traversal
full_simplify                  // Aggressive multi-pass

// Trigonometric
trig_aware_simplify     // Context-aware trig rules
```

### Differentiation

```cpp
Symbol x;
auto expr = x * x + 2_c * x + 1_c;

// Basic differentiation
auto deriv = diff(expr, x);
// Returns: 2*x + 2 (after simplification)

// Compile-time verification
constexpr auto deriv_constexpr = diff(pow(x, 3_c), x);
static_assert(match(deriv_constexpr, 3_c * pow(x, 2_c)));

// Supported operations
diff(sin(x), x);        // → cos(x)
diff(exp(x), x);        // → exp(x)
diff(log(x), x);        // → 1/x
diff(pow(x, n), x);     // → n * pow(x, n-1)
```

### String Conversion

```cpp
Symbol x;
auto expr = x + Constant<2>{};

// Runtime string conversion
std::string str = toStringRuntime(expr);
// Result: "(x123 + 2)"

// Debug printing
debug_print(expr, "my expr");
// Output: my expr: (x123 + 2)

// Compact with type info
debug_print_compact(expr, "expr");
// Output: expr: (x123 + 2) :: Expression<Add, Symbol<123>, Constant<2>>

// Tree visualization
debug_print_tree(expr, 0, "expr");
// Output:
// expr: Expression: (x123 + 2)
//   Op: Add (+)
//   Args:
//     [0]: Symbol: x123
//     [1]: Constant: 2
```

---

## Examples

### Example 1: Basic Simplification

```cpp
#include "symbolic3/symbolic3.h"
using namespace tempura::symbolic3;

"Basic algebraic simplification"_test = [] {
  Symbol x;

  // x + 0 → x
  auto expr1 = x + Constant<0>{};
  auto result1 = algebraic_simplify.apply(expr1, default_context());
  static_assert(match(result1, x));

  // x * 1 → x
  auto expr2 = x * Constant<1>{};
  auto result2 = algebraic_simplify.apply(expr2, default_context());
  static_assert(match(result2, x));

  // x * 0 → 0
  auto expr3 = x * Constant<0>{};
  auto result3 = algebraic_simplify.apply(expr3, default_context());
  static_assert(match(result3, Constant<0>{}));
};
```

### Example 2: Custom Strategy

```cpp
// Define custom transformation
struct DoubleNegationRemoval {
  template <Symbolic S, typename Context>
  constexpr auto apply(S expr, Context ctx) const {
    // Pattern: -(-x) → x
    if constexpr (is_expression<S>) {
      using Op = get_op_t<S>;
      if constexpr (std::same_as<Op, NegOp>) {
        using Args = get_args_t<S>;
        using Inner = std::tuple_element_t<0, Args>;

        if constexpr (is_expression<Inner>) {
          using InnerOp = get_op_t<Inner>;
          if constexpr (std::same_as<InnerOp, NegOp>) {
            using InnerArgs = get_args_t<Inner>;
            using InnerInner = std::tuple_element_t<0, InnerArgs>;
            return InnerInner{};
          }
        }
      }
    }
    return expr;
  }
};

// Use it
"Double negation removal"_test = [] {
  Symbol x;
  auto expr = -(-x);
  auto result = DoubleNegationRemoval{}.apply(expr, default_context());
  static_assert(match(result, x));
};
```

### Example 3: Differentiation

```cpp
"Chain rule in action"_test = [] {
  Symbol x;

  // d/dx[sin(x²)] = cos(x²) · 2x
  auto expr = sin(x * x);
  auto deriv = diff(expr, x);

  // After simplification, should be cos(x²) * 2x
  auto simplified = full_simplify.apply(deriv, default_context());

  debug_print(simplified, "d/dx[sin(x²)]");
};
```

### Example 4: Context-Aware Transformation

```cpp
"Numeric vs symbolic context"_test = [] {
  Symbol x;
  auto expr = Constant<2>{} * x + Constant<3>{} * x;

  // With numeric context: aggressive folding
  auto result1 = algebraic_simplify.apply(expr, numeric_context());
  // Note: term collection not yet implemented!

  // With symbolic context: preserve structure
  auto result2 = algebraic_simplify.apply(expr, symbolic_context());
};
```

---

## Architecture

### File Organization

```
symbolic3/
├── symbolic3.h           # Main header (include this)
├── core.h                # Symbol, Constant, Expression types
├── operators.h           # Expression builders (+, *, sin, etc.)
├── constants.h           # Common constants (pi, e, etc.)
├── context.h             # Context system with tags
├── matching.h            # Pattern matching utilities
├── pattern_matching.h    # Advanced pattern matching
├── strategy.h            # Strategy concept and basic combinators
├── traversal.h           # Traversal strategies (Fold, Unfold, etc.)
├── transforms.h          # Example transformation strategies
├── simplify.h            # Algebraic and trigonometric simplification
├── derivative.h          # Symbolic differentiation
├── evaluate.h            # Value substitution (stub - TODO)
├── to_string.h           # String conversion and debugging
├── ordering.h            # Term ordering utilities
├── README.md             # This file
├── NEXT_STEPS.md         # Development roadmap
└── test/                 # Test suite (10 test files)
```

### Dependency Graph

```
symbolic3.h
  ├─ core.h
  ├─ operators.h ──┬─ core.h
  │                └─ constants.h
  ├─ context.h ──── core.h
  ├─ strategy.h ──┬─ core.h
  │               └─ context.h
  ├─ matching.h ─── core.h
  ├─ pattern_matching.h ──┬─ core.h
  │                       └─ matching.h
  ├─ traversal.h ──┬─ core.h
  │                ├─ strategy.h
  │                └─ context.h
  ├─ simplify.h ───┬─ core.h
  │                ├─ operators.h
  │                ├─ pattern_matching.h
  │                ├─ strategy.h
  │                └─ traversal.h
  ├─ derivative.h ─┬─ core.h
  │                ├─ operators.h
  │                ├─ pattern_matching.h
  │                ├─ simplify.h
  │                └─ strategy.h
  ├─ to_string.h ──┬─ core.h
  │                └─ operators.h
  ├─ transforms.h ─┬─ strategy.h
  │                └─ matching.h
  └─ evaluate.h ─── core.h (stub)
```

### Type System

```cpp
// Core hierarchy
SymbolicTag              // Base tag for all symbolic types
  ├─ Symbol<unique>      // Symbolic variable
  ├─ Constant<val>       // Numeric constant
  ├─ Expression<Op, Args...>  // Compound expression
  └─ Wildcards           // Pattern matching
      ├─ AnyArg
      ├─ AnyExpr
      ├─ AnyConstant
      └─ AnySymbol

// Operators (tag types)
AddOp, MulOp, PowOp, NegOp, DivOp
SinOp, CosOp, TanOp
SinhOp, CoshOp, TanhOp
ExpOp, LogOp, SqrtOp
ASinOp, ACosOp, ATanOp
```

### Compilation Model

**Symbolic expressions are pure types:**

- No runtime representation (zero overhead)
- All computation at compile-time
- Type encoding: `x + 2` becomes `Expression<AddOp, Symbol<λ₁>, Constant<2>>`
- Compiler optimizes away all abstractions

**Performance characteristics:**

- Runtime: Zero overhead (types erased after compilation)
- Compile-time: Grows with expression complexity
- Template instantiation depth: Grows with recursion depth

---

## Debugging Guide

### Quick Reference

| Function                    | Purpose            | Example                                     |
| --------------------------- | ------------------ | ------------------------------------------- |
| `toStringRuntime(expr)`     | Convert to string  | `std::string s = toStringRuntime(x + 2_c);` |
| `debug_print(expr, label)`  | Simple print       | `debug_print(expr, "result");`              |
| `debug_print_compact(expr)` | Print with type    | Shows full type information                 |
| `debug_print_tree(expr)`    | Tree view          | Hierarchical structure visualization        |
| `debug_type_name(expr)`     | Compiler type name | Raw compiler type string                    |

### Common Debugging Patterns

**1. Print expression structure:**

```cpp
Symbol x;
auto expr = (x + Constant<1>{}) * (x - Constant<1>{});
debug_print_tree(expr);
// Output:
// Expression: ((x123 + 1) * (x123 - 1))
//   Op: Mul (*)
//   Args:
//     [0]: Expression: (x123 + 1)
//       Op: Add (+)
//       Args:
//         [0]: Symbol: x123
//         [1]: Constant: 1
//     [1]: Expression: (x123 - 1)
//       ...
```

**2. Check transformation steps:**

```cpp
auto step1 = strategy1.apply(expr, ctx);
debug_print(step1, "after strategy1");

auto step2 = strategy2.apply(step1, ctx);
debug_print(step2, "after strategy2");
```

**3. Verify pattern matching:**

```cpp
auto [matched, x_val] = match(expr, sin(x_));
if (matched) {
  debug_print(x_val, "matched argument");
} else {
  std::println("Pattern did not match");
}
```

**4. Inspect types at compile-time:**

```cpp
// Force compile error to see type
template <typename T> struct ShowType;
ShowType<decltype(expr)> show;  // Error message shows type
```

---

## Testing

### Running Tests

```bash
# Build
cmake -B build -G Ninja
ninja -C build

# Run all symbolic3 tests
ctest --test-dir build -R symbolic3 --output-on-failure

# Run specific test
./build/src/symbolic3/test/basic_test

# Run with verbose output
ctest --test-dir build -R symbolic3 -V
```

### Test Organization

```
src/symbolic3/test/
├── basic_test.cpp              # Core types and operators
├── matching_test.cpp           # Pattern matching
├── pattern_binding_test.cpp    # Pattern binding
├── strategy_v2_test.cpp        # Strategy system
├── simplify_test.cpp           # Simplification rules
├── traversal_test.cpp          # Traversal combinators
├── traversal_simplify_test.cpp # Traversal with simplification
├── full_simplify_test.cpp      # Full simplification pipeline
├── derivative_test.cpp         # Symbolic differentiation
├── transcendental_test.cpp     # Trig and exp/log functions
└── to_string_test.cpp          # String conversion
```

### Writing Tests

Tests use tempura's `unit.h` framework:

```cpp
#include "unit.h"
#include "symbolic3/symbolic3.h"

using namespace tempura::symbolic3;

"Test description"_test = [] {
  Symbol x;
  auto expr = x + Constant<0>{};
  auto result = algebraic_simplify.apply(expr, default_context());

  // Compile-time check
  static_assert(match(result, x));

  // Runtime check
  assert(match(result, x));
};

// Test groups
suite("simplification") = [] {
  "addition identity"_test = [] { /* ... */ };
  "multiplication identity"_test = [] { /* ... */ };
};
```

### Current Test Status

✅ **10/10 tests passing** (as of Phase 1 completion)

**Coverage:**

- Core types and concepts
- Pattern matching
- Strategy composition
- Traversal combinators
- Algebraic simplification
- Trigonometric simplification
- Differentiation
- String conversion

**Gaps (to be filled in Phase 2):**

- Evaluation with value substitution
- Term collection
- Advanced simplification rules
- Integration with other libraries

---

## Design Rationale

### Why Combinators?

**Problems with symbolic2:**

- Hardcoded fixpoint recursion only
- Flat if-else dispatch (hard to extend)
- No context awareness
- Monolithic design

**Benefits of symbolic3:**

1. **Modularity**: Compose strategies like LEGO bricks
2. **Extensibility**: Users add strategies without modifying core
3. **Flexibility**: Multiple traversal strategies, conditional application
4. **Context**: Pass state (depth, mode, domain) through transformations
5. **Performance**: Better compiler optimization opportunities

### Why Concepts Instead of CRTP?

**V1 used CRTP:**

```cpp
struct MyStrategy : Strategy<MyStrategy> {
  template <Symbolic S>
  constexpr auto apply_impl(S expr) const { /* ... */ }
};
```

**V2 uses concepts:**

```cpp
struct MyStrategy {
  template <Symbolic S>
  constexpr auto apply(S expr) const { /* ... */ }
};
```

**Benefits:**

- Simpler code (no CRTP boilerplate)
- Better error messages
- More flexible (any type matching concept works)
- Easier composition

### Why Types Instead of Values?

Expressions are encoded as types, not runtime values:

```cpp
// Type encoding
using ExprType = Expression<AddOp, Symbol<lambda123>, Constant<2>>;

// NOT runtime encoding
struct RuntimeExpr {
  OpType op;
  std::vector<RuntimeExpr> children;
};
```

**Benefits:**

- Zero runtime overhead
- Compile-time computation and verification
- Type-level uniqueness (impossible to confuse different expressions)

**Costs:**

- Large type names
- Slow compilation for complex expressions
- Cannot handle dynamic/user-input expressions

**When to use which:**

- Use symbolic3 for compile-time known expressions
- Use symbolic2 or runtime system for dynamic expressions

---

## Next Steps

See **`NEXT_STEPS.md`** for comprehensive development roadmap.

**Immediate priorities (Phase 2):**

1. **Evaluation System** - Substitute values into expressions
2. **Term Collection** - Simplify `2*x + 3*x` to `5*x`
3. **Extended Simplification** - Log laws, exp laws, advanced trig

**Longer-term (Phase 3+):**

- Integration with autodiff
- Matrix/vector symbolic operations
- Rational function simplification
- Performance optimization

---

## Contributing

### Adding a New Simplification Rule

1. **Define pattern and replacement:**

   ```cpp
   // Example: sin(-x) → -sin(x)
   auto pattern = sin(neg(x_));
   auto replacement = neg(sin(x_));
   ```

2. **Implement strategy:**

   ```cpp
   struct SinNegRule {
     template <Symbolic S, typename Context>
     constexpr auto apply(S expr, Context ctx) const {
       if (auto [matched, x_val] = match(expr, pattern); matched) {
         return evaluate_replacement(replacement, x_val);
       }
       return expr;
     }
   };
   ```

3. **Add to pipeline:**

   ```cpp
   constexpr auto trig_rules = SinNegRule{} | existing_trig_rules;
   ```

4. **Test:**
   ```cpp
   "sin(-x) simplifies"_test = [] {
     Symbol x;
     auto expr = sin(neg(x));
     auto result = trig_rules.apply(expr, default_context());
     static_assert(match(result, neg(sin(x))));
   };
   ```

### Adding a New Operator

1. Define operator tag in `operators.h`
2. Add expression builder function
3. Implement differentiation rule in `derivative.h`
4. Add simplification rules in `simplify.h`
5. Add to_string support in `to_string.h`
6. Write tests

---

## References

### Examples

- `examples/symbolic3_simplify_demo.cpp` - Simplification pipelines and strategies
- `examples/symbolic3_debug_demo.cpp` - Debugging and visualization techniques
- `examples/advanced_simplify_demo.cpp` - Advanced simplification patterns

### Prototypes

- `prototypes/combinator_strategies.cpp` - Combinator design exploration
- `prototypes/value_based_symbolic.cpp` - Alternative design patterns

---

**Last Updated:** October 5, 2025
**Version:** 1.0 (Phase 1 Complete)
**Authors:** Tempura Development Team
**License:** See repository LICENSE file
