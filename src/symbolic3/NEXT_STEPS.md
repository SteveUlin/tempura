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

❌ **TODO (High Priority):**

- [ ] Implement `Fraction<N, D>` template type
- [ ] Add GCD reduction for fraction simplification
- [ ] Modify constant folding to NOT evaluate divisions that produce non-integers
- [ ] Add literal suffix `_frac` for creating fractions: `(1_frac / 2_frac)`
- [ ] Update type absorption rules in `simplify.h` for float/int/fraction mixing
- [ ] Add tests for exact arithmetic behavior

### Files to Modify

- **`src/symbolic3/core.h`**: Add `Fraction<N, D>` struct
- **`src/symbolic3/simplify.h`**:
  - Modify `ConstantFold` to skip division unless result is integer
  - Add `FractionSimplify` strategy with GCD reduction
- **`src/symbolic3/operators.h`**: Add fraction literal helpers
- **`src/symbolic3/test/exact_math_test.cpp`**: New test suite

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

#### 1. **Evaluation System** (High Priority)

- **Status:** Stub only (`evaluate.h` is ~12 lines)
- **Issue:** Cannot substitute values into expressions
- **Needed:**
  - Heterogeneous binding system like symbolic2 (e.g., `evaluate(expr, x=5, y=3.0)`)
  - Support for compile-time and runtime evaluation
  - Type-safe value substitution
- **Use Cases:**
  - Numerical optimization (need to evaluate objective functions)
  - Plotting (need x→y mappings)
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

#### P1: Evaluation System (4-6 hours)

**Goal:** Make symbolic expressions actually computable

```cpp
// Target API:
Symbol x, y;
auto expr = x*x + y;
auto result = evaluate(expr, x=2.0, y=3.0);  // → 7.0

// Should support:
constexpr auto result = evaluate(expr, x=Constant<2>{}, y=Constant<3>{});
static_assert(result.value == 7);
```

**Implementation Strategy:**

1. Port `BinderPack` concept from symbolic2/evaluate.h
2. Create `eval_impl` recursive strategy similar to `diff_impl`
3. Handle heterogeneous types (int, double, Constant<N>)
4. Add tests for compile-time and runtime evaluation

**Files to Create/Modify:**

- `src/symbolic3/evaluate.h` (expand from stub)
- `src/symbolic3/test/evaluate_test.cpp` (new)

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
- [ ] Add comparison table: symbolic2 vs symbolic3 (when to use each)

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

### 3. **Relationship to symbolic2?**

- Keep both? (symbolic2 for lightweight, symbolic3 for complex transformations)
- Migrate fully to symbolic3?
- Extract common core and specialize both?

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
5. **Runtime flexibility:** Symbolic3 is compile-time; for runtime, use symbolic2 or other tools

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

- **Design Docs (root):**
  - `SYMBOLIC2_COMBINATORS_DESIGN.md` - Original architecture
  - `SYMBOLIC2_GENERALIZATION_COMPLETE.md` - Implementation guide
- **Examples:**
  - `examples/symbolic3_demo.cpp` - Basic usage
  - `examples/symbolic3_v2_demo.cpp` - V2 strategy system
  - `examples/symbolic3_simplify_demo.cpp` - Simplification examples

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
