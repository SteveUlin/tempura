# Simplify.h Analysis & Improvement Recommendations

**Date:** October 4, 2025
**File:** `src/symbolic2/simplify.h`
**Status:** Post-refactoring review

## Executive Summary

The current implementation is **well-structured and functional**, with successful migration to the RewriteSystem pattern. The recent refactoring reduced ~50-60 lines of repeated code. However, there are opportunities to improve:

1. **Readability**: Rule organization and documentation
2. **Performance**: Compile-time optimization opportunities
3. **Extensibility**: Making it easier to add new rules and operators
4. **Maintainability**: Better separation of concerns

---

## Current Strengths ✓

### 1. **Declarative Rule System**

- RewriteSystem provides clean, pattern-based transformations
- Rules are self-documenting (e.g., `Rewrite{x_ + 0_c, x_}`)
- Predicate support enables conditional rules

### 2. **Type Safety**

- All operations are compile-time verified
- No runtime overhead
- Pattern matching ensures correctness

### 3. **Recent Refactoring Success**

- `applyRules` helper eliminated 8 wrapper functions
- `trySimplify` helper consolidated repetitive recursion pattern
- Code is more DRY (Don't Repeat Yourself)

---

## Improvement Opportunities

### 1. **Rule Organization & Discovery** 🎯 HIGH IMPACT

**Current Issue:**

```cpp
constexpr auto AdditionRules = RewriteSystem{
  Rewrite{0_c + x_, x_},         // 0 + x → x
  Rewrite{x_ + 0_c, x_},         // x + 0 → x
  Rewrite{x_ + x_, x_ * 2_c},
  // ... 6 more rules
};
```

Problems:

- Rules are flat and unorganized by purpose
- Hard to see which rules handle identity, ordering, factoring, etc.
- No way to selectively enable/disable rule categories

**Proposed Solution: Rule Categories**

```cpp
// Define rule categories as composable units
namespace Rules {
  namespace Addition {
    constexpr auto Identity = RewriteSystem{
      Rewrite{0_c + x_, x_},
      Rewrite{x_ + 0_c, x_}
    };

    constexpr auto Combining = RewriteSystem{
      Rewrite{x_ + x_, x_ * 2_c}
    };

    constexpr auto Ordering = RewriteSystem{
      Rewrite{x_ + y_, y_ + x_, [](auto ctx) {
        return symbolicLessThan(get(ctx, y_), get(ctx, x_));
      }}
    };

    constexpr auto Factoring = RewriteSystem{
      Rewrite{x_ * a_ + x_, x_ * (a_ + 1_c)},
      Rewrite{x_ * a_ + x_ * b_, x_ * (a_ + b_)}
    };

    constexpr auto Associativity = RewriteSystem{
      Rewrite{(a_ + c_) + b_, (a_ + b_) + c_, [](auto ctx) {
        return symbolicLessThan(get(ctx, b_), get(ctx, c_));
      }},
      Rewrite{(a_ + b_) + c_, a_ + (b_ + c_)}
    };
  }
}

// Compose into complete rule set
constexpr auto AdditionRules = compose(
  Rules::Addition::Identity,
  Rules::Addition::Combining,
  Rules::Addition::Ordering,
  Rules::Addition::Factoring,
  Rules::Addition::Associativity
);
```

**Benefits:**

- Self-documenting organization
- Easy to test individual rule categories
- Can create different simplification profiles (fast, aggressive, conservative)
- Better code navigation

---

### 2. **Simplification Strategy Configuration** 🎯 HIGH IMPACT

**Current Issue:**

```cpp
template <Symbolic S, SizeT depth>
constexpr auto simplifySymbolWithDepth(S sym) {
  if constexpr (depth >= 20) {
    return S{};
  }
  else if constexpr (requires { evalConstantExpr(sym); }) {
    return evalConstantExpr(sym);
  }
  else if constexpr (requires { powerIdentities(sym); }) {
    return trySimplify<S, depth, powerIdentities<S>>(sym);
  }
  // ... 8 more else-if branches
}
```

Problems:

- Strategy order is hardcoded
- No way to customize which rules apply
- Can't measure which rules are most effective
- Adding new operators requires modifying this function

**Proposed Solution: Strategy Pattern**

```cpp
// Define a simplification strategy as a type list
template <typename... Simplifiers>
struct SimplificationStrategy {
  template <Symbolic S, SizeT depth>
  static constexpr auto apply(S sym) {
    if constexpr (depth >= MaxDepth) {
      return sym;
    } else {
      return applySimplifiers<S, depth, Simplifiers...>(sym);
    }
  }

private:
  static constexpr SizeT MaxDepth = 20;

  template <Symbolic S, SizeT depth, typename First, typename... Rest>
  static constexpr auto applySimplifiers(S sym) {
    if constexpr (requires { First::simplify(sym); }) {
      constexpr auto result = First::simplify(sym);
      if constexpr (!match(result, sym)) {
        return apply<decltype(result), depth + 1>(result);
      }
    }

    if constexpr (sizeof...(Rest) > 0) {
      return applySimplifiers<S, depth, Rest...>(sym);
    } else {
      return sym;
    }
  }
};

// Define simplifiers as types
struct ConstantFolding {
  template <Symbolic S>
  static constexpr auto simplify(S sym)
    requires(requires { evalConstantExpr(sym); })
  {
    return evalConstantExpr(sym);
  }
};

struct PowerSimplifier {
  template <Symbolic S>
  static constexpr auto simplify(S sym)
    requires(requires { powerIdentities(sym); })
  {
    return applyRules<PowerRules>(sym);
  }
};

// ... more simplifiers ...

// Pre-defined strategies
using DefaultStrategy = SimplificationStrategy<
  ConstantFolding,
  PowerSimplifier,
  AdditionSimplifier,
  MultiplicationSimplifier,
  // ...
>;

using FastStrategy = SimplificationStrategy<
  ConstantFolding,
  PowerSimplifier
  // Fewer rules for faster compilation
>;

using AggressiveStrategy = SimplificationStrategy<
  ConstantFolding,
  PowerSimplifier,
  AdditionSimplifier,
  MultiplicationSimplifier,
  TrigonometricSimplifier,
  LogarithmSimplifier,
  // All rules enabled
>;

// Usage
template <Symbolic S>
constexpr auto simplify(S sym) {
  return DefaultStrategy::apply<S, 0>(sym);
}
```

**Benefits:**

- Easy to add new operators without modifying core logic
- Can define custom strategies for different use cases
- Testable in isolation
- Could add compile-time metrics (which rules applied most)

---

### 3. **Rule Metadata & Introspection** 🎯 MEDIUM IMPACT

**Current Issue:**
Rules lack metadata making debugging and optimization difficult.

**Proposed Solution:**

```cpp
// Add metadata to rules
template <typename Pattern, typename Replacement, typename Predicate,
          auto Name, auto Category, auto Priority = 100>
struct AnnotatedRewrite : Rewrite<Pattern, Replacement, Predicate> {
  static constexpr const char* name = Name;
  static constexpr const char* category = Category;
  static constexpr int priority = Priority;
};

// Usage
constexpr auto AdditionRules = RewriteSystem{
  AnnotatedRewrite<
    decltype(0_c + x_), decltype(x_), NoPredicate,
    "zero-addition-left", "identity", 1000
  >{},
  AnnotatedRewrite<
    decltype(x_ + 0_c), decltype(x_), NoPredicate,
    "zero-addition-right", "identity", 1000
  >{},
  // ...
};

// Can query rules at compile-time
static_assert(AdditionRules.ruleCount() == 9);
static_assert(AdditionRules.hasCategory("identity"));
```

**Benefits:**

- Better debugging (know which rule applied)
- Could generate documentation automatically
- Enable rule prioritization
- Helpful for testing and validation

---

### 4. **Performance: Rule Caching** 🎯 MEDIUM IMPACT

**Current Issue:**
Every simplification pass checks all rules every time, even if we know certain rules will never match.

**Proposed Solution:**

```cpp
// Cache which rules apply to which operator types
template <typename Op>
struct RuleCache {
  // Computed at compile-time: which rule sets apply to this Op
  static constexpr auto applicable_rules = computeApplicableRules<Op>();

  template <Symbolic S>
  static constexpr auto simplify(S expr) {
    // Only try rules known to apply to Op
    return applyRuleSet<applicable_rules>(expr);
  }
};

// Type-dispatch optimization
template <Symbolic S>
constexpr auto simplify(S sym) {
  if constexpr (isExpression<S>) {
    using Op = typename S::Op;
    return RuleCache<Op>::simplify(sym);
  } else {
    return sym;
  }
}
```

**Benefits:**

- Reduces compile-time by avoiding impossible rule matches
- Could reduce template instantiations significantly
- Especially helpful for large rule sets

---

### 5. **Better Testing Infrastructure** 🎯 MEDIUM IMPACT

**Current Issue:**
Tests are in one large file with manual test cases.

**Proposed Solution:**

```cpp
// Property-based testing helpers
template <typename Rule>
struct RuleTester {
  // Test that a rule is idempotent
  template <Symbolic S>
  static constexpr bool testIdempotent(S expr) {
    constexpr auto once = Rule::apply(expr);
    constexpr auto twice = Rule::apply(once);
    return match(once, twice);
  }

  // Test that a rule doesn't change semantics
  template <Symbolic S, typename... Bindings>
  static constexpr bool testPreservesSemantics(S expr, Bindings... bindings) {
    constexpr auto simplified = Rule::apply(expr);
    constexpr auto original_val = evaluate(expr, BinderPack{bindings...});
    constexpr auto simplified_val = evaluate(simplified, BinderPack{bindings...});
    return original_val == simplified_val;
  }
};

// Test file organization
namespace Tests {
  namespace Addition {
    void testIdentityRules();
    void testCombiningRules();
    void testOrderingRules();
    void testFactoringRules();
  }

  namespace Multiplication {
    void testIdentityRules();
    void testDistributionRules();
    void testPowerCombining();
  }
}
```

**Benefits:**

- Easier to maintain and extend tests
- Property-based testing catches more bugs
- Better test organization matches rule organization

---

### 6. **Documentation Generation** 🎯 LOW IMPACT

**Proposed Solution:**

```cpp
// Could generate markdown documentation from rules
template <typename RuleSet>
constexpr auto generateRuleDoc() {
  // Iterate over rules and generate documentation
  // "## Addition Rules\n"
  // "### Identity Rules\n"
  // "- `0 + x → x` (zero-addition-left)\n"
  // ...
}
```

---

## Implementation Priority

### Phase 1: High Impact, Low Risk (Immediate)

1. ✅ **Refactor rule organization into categories**

   - Split AdditionRules, MultiplicationRules into sub-categories
   - Create `Rules::Addition::Identity`, etc.
   - Compose with helper function

2. ✅ **Add rule metadata**
   - Names, categories, priorities
   - Helps with debugging

### Phase 2: High Impact, Medium Risk (Next)

3. **Implement SimplificationStrategy pattern**
   - Create simplifier types
   - Implement strategy composition
   - Preserve backward compatibility with current API

### Phase 3: Medium Impact (Future)

4. **Improve testing infrastructure**

   - Create property-based test helpers
   - Organize tests by category

5. **Implement rule caching**
   - Type-based dispatch optimization
   - Benchmark compile-time improvements

### Phase 4: Low Impact (Optional)

6. **Documentation generation**
   - Auto-generate rule documentation
   - Create visualization tools

---

## Code Quality Metrics

### Current State

- **Lines of Code:** ~320 lines
- **Rule Count:** 47 rules across 9 operators
- **Max Depth:** 20 (hardcoded)
- **Abstraction Level:** Good (after refactoring)

### Targets (Post-improvements)

- **Lines of Code:** ~400-450 (more structure, same functionality)
- **Testability:** Excellent (category-based testing)
- **Extensibility:** Excellent (add operators without modifying core)
- **Performance:** 10-20% faster compile times (with caching)

---

## Potential Data Structures

### 1. **RuleCategory** (for organization)

```cpp
template <const char* Name, typename... Rules>
struct RuleCategory {
  static constexpr const char* name = Name;
  static constexpr auto rules = RewriteSystem{Rules{}...};
  static constexpr size_t count = sizeof...(Rules);
};
```

### 2. **SimplificationProfile** (for configuration)

```cpp
struct SimplificationProfile {
  bool enable_constant_folding = true;
  bool enable_ordering = true;
  bool enable_factoring = true;
  bool enable_distribution = true;
  bool enable_trig_identities = true;
  int max_depth = 20;

  // Could be used at compile-time to configure which rules apply
};
```

### 3. **RuleApplicationTrace** (for debugging)

```cpp
template <typename... AppliedRules>
struct RuleTrace {
  // Compile-time trace of which rules were applied
  static constexpr size_t depth = sizeof...(AppliedRules);

  template <typename NewRule>
  using append = RuleTrace<AppliedRules..., NewRule>;
};
```

---

## Specific Recommendations

### Recommendation 1: Split Rules into Files

Current: All rules in `simplify.h`
Proposed:

```
src/symbolic2/simplification/
  ├── rules/
  │   ├── addition_rules.h
  │   ├── multiplication_rules.h
  │   ├── power_rules.h
  │   ├── trig_rules.h
  │   └── log_rules.h
  ├── strategies.h
  ├── core.h
  └── simplify.h (public API)
```

### Recommendation 2: Add Simplification Options

```cpp
template <typename Strategy = DefaultStrategy>
struct Simplifier {
  template <Symbolic S>
  static constexpr auto simplify(S expr) {
    return Strategy::template apply<S, 0>(expr);
  }
};

// Usage
using FastSimplifier = Simplifier<FastStrategy>;
auto result = FastSimplifier::simplify(expr);
```

### Recommendation 3: Improve Error Messages

Add static_assert with helpful messages when rules fail:

```cpp
template <Symbolic S>
constexpr auto powerIdentities(S expr) {
  static_assert(match(S{}, pow(AnyArg{}, AnyArg{})),
    "powerIdentities requires a power expression (x^y)");
  return applyRules<PowerRules>(expr);
}
```

---

## Conclusion

The current implementation is **solid and maintainable**. The suggested improvements focus on:

1. **Better organization** (rule categories)
2. **More flexibility** (strategies)
3. **Easier extension** (add operators without core changes)
4. **Better testing** (property-based, isolated tests)

These changes would make the codebase more professional, easier to contribute to, and better prepared for adding new mathematical operators and simplification techniques.

**Recommended First Step:** Implement rule categorization (Phase 1.1) as it provides immediate benefits with minimal risk.
