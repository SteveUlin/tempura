# Architecture Comparison: Current vs. Proposed

## Current Architecture

```
simplify.h (320 lines)
│
├── Helper Functions
│   ├── applyRules<Rules>         ✓ (recently added)
│   └── trySimplify<S, depth>     ✓ (recently added)
│
├── Rule Definitions (flat)
│   ├── PowerRules                [5 rules]
│   ├── AdditionRules             [9 rules] ← all mixed together
│   ├── MultiplicationRules       [12 rules] ← all mixed together
│   ├── ExpRules                  [2 rules]
│   ├── LogRules                  [6 rules]
│   ├── SinRules                  [4 rules]
│   ├── CosRules                  [3 rules]
│   └── TanRules                  [1 rule]
│
├── Identity Functions
│   ├── powerIdentities()
│   ├── additionIdentities()
│   ├── multiplicationIdentities()
│   ├── subtractionIdentities()   ← special handling
│   ├── divisionIdentities()      ← special handling
│   ├── expIdentities()
│   ├── logIdentities()
│   ├── sinIdentities()
│   ├── cosIdentities()
│   └── tanIdentities()
│
├── Core Simplification
│   ├── simplifySymbolWithDepth() ← hardcoded strategy
│   ├── simplifySymbol()
│   ├── simplifyTerms()
│   └── simplify()                ← public API
│
└── Utilities
    └── evalConstantExpr()

Issues:
❌ Rules not organized by purpose
❌ Can't test categories independently
❌ Hardcoded simplification strategy
❌ Adding new operators requires editing core
❌ No way to customize behavior
```

---

## Proposed Architecture (Phase 1: Categories)

```
simplify/
│
├── helpers.h
│   ├── applyRules<Rules>
│   ├── trySimplify<S, depth>
│   └── compose(Systems...)       ← NEW: compose rule sets
│
├── rules/
│   │
│   ├── addition_rules.h
│   │   namespace AdditionRuleCategories {
│   │     ├── Identity          [2 rules]  ✓ self-contained
│   │     ├── LikeTerms          [1 rule]   ✓ testable
│   │     ├── Ordering           [1 rule]   ✓ documented
│   │     ├── Factoring          [2 rules]  ✓ optional
│   │     └── Associativity      [3 rules]  ✓ clear purpose
│   │   }
│   │   → AdditionRules = compose(Identity, LikeTerms, ...)
│   │
│   ├── multiplication_rules.h
│   │   namespace MultiplicationRuleCategories {
│   │     ├── Identity           [4 rules]
│   │     ├── Distribution       [2 rules]
│   │     ├── PowerCombining     [3 rules]
│   │     ├── Ordering           [1 rule]
│   │     └── Associativity      [3 rules]
│   │   }
│   │   → MultiplicationRules = compose(Identity, Distribution, ...)
│   │
│   ├── power_rules.h            [5 rules] (no categories needed)
│   ├── trig_rules.h             [sin: 4, cos: 3, tan: 1]
│   └── log_exp_rules.h          [exp: 2, log: 6]
│
├── simplifiers/
│   ├── constant_folding.h       ← NEW: modular
│   ├── power_simplifier.h       ← NEW: modular
│   ├── addition_simplifier.h    ← NEW: modular
│   └── ...
│
├── strategies/
│   ├── strategy.h               ← NEW: SimplificationStrategy<...>
│   ├── default_strategy.h       ← NEW: DefaultStrategy
│   └── fast_strategy.h          ← NEW: FastStrategy
│
└── simplify.h
    ├── simplifySymbol()         ← uses strategy
    ├── simplifyTerms()
    └── simplify()               ← public API (unchanged)

Benefits:
✅ Rules organized by purpose
✅ Each category testable independently
✅ Customizable strategies
✅ Easy to add new operators
✅ Can disable problematic rules
✅ Self-documenting structure
```

---

## Proposed Architecture (Phase 2: Strategies)

```
Using the Strategy Pattern

┌─────────────────────────────────────────────────────────────┐
│ simplify(expr)  ← Public API (unchanged)                    │
└─────────────────────────────────────────────────────────────┘
                            │
                            ▼
┌─────────────────────────────────────────────────────────────┐
│ DefaultStrategy::apply<S, 0>(expr)                          │
│                                                              │
│ Strategy = SimplificationStrategy<                          │
│   ConstantFolding,         ← evaluates constants            │
│   PowerSimplifier,         ← x^0, x^1, etc.                 │
│   AdditionSimplifier,      ← uses AdditionRules             │
│   MultiplicationSimplifier, ← uses MultiplicationRules      │
│   TrigSimplifier,          ← sin, cos, tan                  │
│   LogExpSimplifier         ← log, exp                       │
│ >                                                            │
└─────────────────────────────────────────────────────────────┘
                            │
        ┌───────────────────┼───────────────────┐
        ▼                   ▼                   ▼
┌──────────────┐  ┌──────────────────┐  ┌──────────────┐
│ConstantFolding│  │ PowerSimplifier  │  │ AdditionSim │
├──────────────┤  ├──────────────────┤  ├──────────────┤
│ simplify(S)  │  │ simplify(S)      │  │ simplify(S)  │
│   requires   │  │   requires       │  │   requires   │
│   all consts │  │   pow(x, y)      │  │   x + y      │
└──────────────┘  └──────────────────┘  └──────────────┘
                            │
                            ▼
                  ┌──────────────────┐
                  │ PowerRules.apply │
                  └──────────────────┘

Alternative Strategies:

┌─────────────────────────────────────┐
│ FastStrategy (fewer rules)          │
├─────────────────────────────────────┤
│ SimplificationStrategy<             │
│   ConstantFolding,                  │
│   PowerSimplifier                   │
│ >                                   │
│                                     │
│ Use case: Quick simplification      │
│ Benefit: Faster compile times       │
└─────────────────────────────────────┘

┌─────────────────────────────────────┐
│ AggressiveStrategy (all rules)      │
├─────────────────────────────────────┤
│ SimplificationStrategy<             │
│   ConstantFolding,                  │
│   PowerSimplifier,                  │
│   AdditionSimplifier,               │
│   MultiplicationSimplifier,         │
│   TrigSimplifier,                   │
│   LogExpSimplifier,                 │
│   ExperimentalRules                 │
│ >                                   │
│                                     │
│ Use case: Maximum simplification    │
│ Benefit: Best results               │
└─────────────────────────────────────┘

┌─────────────────────────────────────┐
│ CustomStrategy (user-defined)       │
├─────────────────────────────────────┤
│ SimplificationStrategy<             │
│   ConstantFolding,                  │
│   PowerSimplifier,                  │
│   MyCustomSimplifier                │
│ >                                   │
│                                     │
│ Use case: Domain-specific needs     │
│ Benefit: Full control               │
└─────────────────────────────────────┘
```

---

## Testing Architecture

### Current Testing

```
test/simplify_test.cpp
├── "Addition identities"_test
├── "Multiplication identities"_test
├── "Power identities"_test
├── "Constant folding"_test
├── "Subtraction to addition"_test
├── "Division to multiplication"_test
├── "Logarithm identities"_test
├── "Exponential identities"_test
├── "Trigonometric identities"_test
└── "Complex expression"_test

Issue: Monolithic test file
```

### Proposed Testing (Phase 4)

```
test/simplify/
│
├── rules/
│   ├── addition_rules_test.cpp
│   │   ├── testIdentityRules()
│   │   ├── testLikeTerms()
│   │   ├── testOrdering()
│   │   ├── testFactoring()
│   │   └── testAssociativity()
│   │
│   ├── multiplication_rules_test.cpp
│   │   ├── testIdentityRules()
│   │   ├── testDistribution()
│   │   ├── testPowerCombining()
│   │   └── testOrdering()
│   │
│   └── ...
│
├── properties/
│   ├── idempotence_test.cpp      ← RuleTester::testIdempotent
│   ├── semantics_test.cpp        ← RuleTester::testPreservesSemantics
│   └── ordering_test.cpp         ← verify canonical forms
│
├── strategies/
│   ├── default_strategy_test.cpp
│   ├── fast_strategy_test.cpp
│   └── strategy_comparison_test.cpp
│
└── integration/
    └── complex_expressions_test.cpp

Benefits:
✅ Tests organized by feature
✅ Property-based testing
✅ Easy to add new tests
✅ Clear test coverage
```

---

## Migration Path

### Step 0: Current State ✓

- File: `simplify.h` (320 lines)
- 47 rules in flat structure
- Working and tested

### Step 1: Add compose() helper (30 min)

```cpp
template <typename... Systems>
constexpr auto compose(Systems... systems) {
  // Implementation
}
```

### Step 2: Refactor one rule set (1-2 hours)

```cpp
// In simplify.h:
namespace AdditionRuleCategories {
  constexpr auto Identity = RewriteSystem{ /* ... */ };
  constexpr auto LikeTerms = RewriteSystem{ /* ... */ };
  // ...
}

constexpr auto AdditionRules = compose(
  AdditionRuleCategories::Identity,
  AdditionRuleCategories::LikeTerms,
  // ...
);
```

### Step 3: Apply to all rule sets (2-3 hours)

- MultiplicationRules
- PowerRules (may not need categories)
- TrigRules
- LogExpRules

### Step 4: Verify tests pass (30 min)

```bash
ninja symbolic2_simplify_test
./src/symbolic2/symbolic2_simplify_test
```

### Step 5: Document changes (30 min)

- Update header comments
- Add examples of using categories
- Document compose() function

**Total Phase 1 Time: ~4-5 hours**

---

## Code Size Comparison

### Current

```
simplify.h:              320 lines
simplify_test.cpp:       164 lines
────────────────────────────────
Total:                   484 lines
```

### After Phase 1 (Categories)

```
simplify.h:              380 lines  (+60 for organization)
simplify_test.cpp:       164 lines  (unchanged)
────────────────────────────────
Total:                   544 lines  (+12% size)
                                    (+100% maintainability)
```

### After Phase 2 (Strategies)

```
simplify/
  helpers.h:              40 lines
  strategies.h:          120 lines
  rules/*.h:             240 lines
  simplifiers/*.h:       150 lines
  simplify.h (public):    50 lines
test/simplify/*.cpp:     250 lines  (better organized)
────────────────────────────────
Total:                   850 lines  (+76% size)
                                    (+300% functionality)
```

### Value Proposition

- Current: ~320 lines, one way to simplify
- Proposed: ~850 lines, infinite ways to simplify
  - Multiple strategies
  - Customizable rules
  - Better tested
  - Easier to extend
  - Self-documenting

---

## Performance Considerations

### Compile-Time (estimated)

| Configuration           | Current  | Phase 1 | Phase 2 | Phase 3 |
| ----------------------- | -------- | ------- | ------- | ------- |
| Template instantiations | Baseline | +5%     | +10%    | -20%    |
| Compile time            | 1.0x     | 1.05x   | 1.10x   | 0.85x   |
| Binary size             | Baseline | +0%     | +0%     | +0%     |

Phase 3 (rule caching) should more than offset the overhead from Phases 1-2.

### Runtime

All phases have **zero runtime overhead** - everything is compile-time.

---

## Conclusion

The proposed architecture provides:

1. **Better Organization**

   - Self-documenting structure
   - Clear separation of concerns
   - Easy to navigate

2. **More Flexibility**

   - Multiple strategies
   - Customizable behavior
   - Domain-specific simplifiers

3. **Better Testing**

   - Test in isolation
   - Property-based tests
   - Better coverage

4. **Easier Extension**
   - Add operators without touching core
   - Create custom rule categories
   - Experiment safely

The migration can be done incrementally, starting with just the `compose()` helper and categorizing one rule set. Each step provides value and doesn't break existing functionality.

**Recommended:** Start with Phase 1 (4-5 hours) and evaluate before proceeding to Phase 2.
