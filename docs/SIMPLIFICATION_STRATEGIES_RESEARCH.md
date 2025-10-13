# Advanced Simplification Strategies for Symbolic3

## Problem Statement

Current simplification approach:

1. Simplify bottom node
2. Move up one level
3. Repeat until top
4. If nothing changed, return; else repeat entire process

This is inefficient and inflexible. Key issues:

- **Redundant work**: `(complex_expr) + (complex_expr)` simplifies both sides independently
- **Missed short-circuits**: `0 * (complex_expr)` should return immediately
- **No control over traversal**: Can't express different rules going down vs. up

## Research: Term Rewriting Strategies

### Core Concepts from Literature

**Evaluation Strategies** (from functional programming):

- **Eager/Strict**: Evaluate all arguments before applying a function
- **Lazy/Non-strict**: Only evaluate what's needed
- **Normal order**: Leftmost-outermost redex first (most lazy)
- **Applicative order**: Leftmost-innermost redex first (most eager)

**Traversal Orders** (from term rewriting):

- **Innermost**: Apply at leaves first, propagate upward (bottom-up)
- **Outermost**: Apply at root first, recurse if changed (top-down with repetition)
- **Bottom-up**: Post-order traversal (children, then parent)
- **Top-down**: Pre-order traversal (parent, then children)

**Key Insight**: Different rules benefit from different evaluation orders.

### Strategies for Specific Scenarios

#### 1. Short-Circuit Evaluation (Annihilators)

**Problem**: `0 * (huge_expr)` shouldn't simplify `huge_expr`

**Solution**: **Eager parent patterns with lazy children**

```cpp
// Match parent pattern BEFORE recursing into children
// Pattern: if parent has annihilator form, short-circuit
constexpr auto eager_annihilator_check = [](auto expr, auto ctx) {
  // Check for 0 * x pattern at current level only
  if (matches_zero_mult_pattern(expr)) {
    return 0_c;  // Short-circuit, never recurse
  }
  return Never{};  // Not an annihilator, proceed with normal traversal
};
```

**Strategy**: Apply parent-level patterns BEFORE recursing (outermost-first)

#### 2. Common Subexpression Elimination

**Problem**: `(complex_expr) + (complex_expr)` simplifies both sides independently

**Solution**: **Hash-consing or memoization within context**

Option A: **Structural Equality Check** (compile-time)

```cpp
// Check if children are structurally identical before recursing
template <typename Op, Symbolic A, Symbolic B>
constexpr auto smart_add(Expression<Add, A, B> expr, Context ctx) {
  if constexpr (std::is_same_v<A, B>) {
    // x + x → 2*x, no need to recurse into both
    return 2_c * simplify(A{}, ctx);  // Simplify once, reuse
  } else {
    // Different children, recurse normally
    return simplify(A{}, ctx) + simplify(B{}, ctx);
  }
}
```

Option B: **Memoization in Context** (runtime)

```cpp
// Extend context with memoization table
struct MemoContext {
  std::unordered_map<ExpressionHash, SimplifiedResult> memo;

  auto simplify_with_memo(auto expr) {
    auto hash = compute_hash(expr);
    if (auto it = memo.find(hash); it != memo.end()) {
      return it->second;  // Reuse previous result
    }
    auto result = simplify_once(expr);
    memo[hash] = result;
    return result;
  }
};
```

**Trade-off**: Compile-time approach is zero runtime cost but limited to structural equality.

#### 3. Conditional Traversal Based on Operation

**Problem**: Different operations need different traversal strategies

**Solution**: **Operation-specific traversal combinators**

```cpp
// Dispatch to appropriate strategy based on operation
template <Symbolic Expr>
constexpr auto smart_simplify(Expr expr, auto ctx) {
  if constexpr (is_multiplication<Expr>) {
    // Multiplication: check for annihilators FIRST (outermost)
    return outermost(mult_rules).apply(expr, ctx);
  } else if constexpr (is_addition<Expr>) {
    // Addition: bottom-up to collect like terms
    return innermost(add_rules).apply(expr, ctx);
  } else if constexpr (is_power<Expr>) {
    // Power: top-down to expand early
    return topdown(power_rules).apply(expr, ctx);
  } else {
    // Default: innermost
    return innermost(all_rules).apply(expr, ctx);
  }
}
```

#### 4. Staged Simplification with Different Modes

**Problem**: Some rules should only apply going down, others going up

**Solution**: **Separate downward and upward rule sets**

```cpp
struct TwoPhaseSimplify {
  Strategy downward_rules;   // Applied during descent (pre-order)
  Strategy upward_rules;     // Applied during ascent (post-order)

  template <Symbolic Expr, typename Context>
  constexpr auto apply(Expr expr, Context ctx) const {
    // Phase 1: Apply downward rules (parent before children)
    auto after_down = downward_rules.apply(expr, ctx);

    // Phase 2: Recurse into children
    auto after_children = [&] {
      if constexpr (has_children_v<decltype(after_down)>) {
        return apply_to_children(TwoPhaseSimplify{downward_rules, upward_rules},
                                 after_down, ctx);
      } else {
        return after_down;
      }
    }();

    // Phase 3: Apply upward rules (children after parent)
    return upward_rules.apply(after_children, ctx);
  }
};

// Usage
constexpr auto downward = distribution_rules | annihilator_rules;  // Expand early
constexpr auto upward = factoring_rules | collection_rules;        // Collect late
constexpr auto two_phase = TwoPhaseSimplify{downward, upward};
```

**Example**:

- Downward: `(a+b) * 0` → check annihilator BEFORE distributing
- Upward: `x*a + x*b` → factor AFTER children simplified to `a` and `b`

### 5. Contextual Evaluation Order

**Problem**: Should `exp(log(complex_expr))` simplify the argument first?

**Solution**: **Context-dependent strategies**

```cpp
// Identity functions: simplify argument ONLY if top-level identity matches
constexpr auto identity_aware = [](auto expr, auto ctx) {
  if constexpr (matches<exp(log(x_))>(expr)) {
    // exp(log(x)) → x, DON'T simplify x first
    return extract_argument(expr);
  } else if constexpr (matches<log(exp(x_))>(expr)) {
    // log(exp(x)) → x, DON'T simplify x first
    return extract_argument(expr);
  } else {
    // Not an identity, recurse normally
    return Never{};
  }
};

// Apply identity checks BEFORE recursing
constexpr auto smart_transcendental =
  outermost(identity_aware) | innermost(transcendental_rules);
```

## Recommended Architecture

### Design Philosophy

> "Elegance in rule definitions, even at cost of complexity elsewhere"

### Proposed Strategy Combinator: `Smarter`

```cpp
// Smart simplification with built-in heuristics
template <Strategy Rules>
struct Smarter {
  Rules rules;

  template <Symbolic Expr, typename Context>
  constexpr auto apply(Expr expr, Context ctx) const {
    // 1. Check for annihilators/identities at parent level (eager)
    if (auto quick = quick_patterns.apply(expr, ctx); !is_never(quick)) {
      return quick;
    }

    // 2. Check for structural sharing (CSE)
    if constexpr (has_identical_children(expr)) {
      return simplify_with_sharing(expr, ctx);
    }

    // 3. Choose traversal strategy based on operation type
    if constexpr (needs_outermost(expr)) {
      return outermost(rules).apply(expr, ctx);
    } else if constexpr (needs_topdown(expr)) {
      return topdown(rules).apply(expr, ctx);
    } else {
      // Default: innermost (most common)
      return innermost(rules).apply(expr, ctx);
    }
  }
};
```

### Pattern-Specific Strategy Selection

```cpp
// Annihilators: outermost (check parent before recursing)
constexpr auto annihilator_rules = Rewrite{0_c * x_, 0_c} |
                                   Rewrite{x_ * 0_c, 0_c};

// Identities: outermost (remove wrapper before recursing)
constexpr auto identity_rules = Rewrite{exp(log(x_)), x_} |
                                Rewrite{log(exp(x_)), x_};

// Collection: innermost (children must be simplified first)
constexpr auto collection_rules = Rewrite{x_ + x_, 2_c * x_} |
                                  Rewrite{x_ * a_ + x_ * b_, x_ * (a_ + b_)};

// Expansion: topdown (expand before recursing)
constexpr auto expansion_rules = Rewrite{(a_ + b_) * c_, a_ * c_ + b_ * c_};

// Combined strategy with automatic dispatch
constexpr auto smart_rules =
  outermost(annihilator_rules | identity_rules) >>
  topdown(expansion_rules) >>
  innermost(collection_rules);
```

## Implementation Recommendations

### Phase 1: Add Short-Circuit Patterns (High Impact, Low Complexity)

```cpp
// New traversal: OutermostOnce (check parent, recurse if no match)
template <Strategy QuickPatterns, Strategy MainRules>
struct ShortCircuit {
  template <Symbolic Expr, typename Context>
  constexpr auto apply(Expr expr, Context ctx) const {
    // Try quick patterns at parent level FIRST
    auto quick_result = QuickPatterns{}.apply(expr, ctx);
    if constexpr (!std::is_same_v<decltype(quick_result), Never>) {
      return quick_result;  // Short-circuit successful
    }

    // No short-circuit, proceed with normal simplification
    return innermost(MainRules{}).apply(expr, ctx);
  }
};

constexpr auto quick_patterns =
  Rewrite{0_c * x_, 0_c} |
  Rewrite{x_ * 0_c, 0_c} |
  Rewrite{1_c * x_, x_} |
  Rewrite{x_ * 1_c, x_};

constexpr auto optimized_simplify =
  ShortCircuit<decltype(quick_patterns), decltype(algebraic_simplify)>{};
```

### Phase 2: Common Subexpression Detection (Medium Impact, Medium Complexity)

```cpp
// Compile-time CSE for structural equality
template <typename Op, Symbolic A, Symbolic B>
constexpr auto smart_binary_op(Expression<Op, A, B> expr, auto ctx) {
  if constexpr (std::is_same_v<A, B>) {
    // Children are identical, simplify once
    auto child_simplified = innermost(rules).apply(A{}, ctx);

    // Then apply operation-specific like-term rules
    if constexpr (std::is_same_v<Op, Add>) {
      return 2_c * child_simplified;  // x + x → 2*x
    } else if constexpr (std::is_same_v<Op, Mult>) {
      return pow(child_simplified, 2_c);  // x * x → x^2
    }
  }

  // Different children or not applicable
  return innermost(rules).apply(expr, ctx);
}
```

### Phase 3: Two-Phase Traversal (High Elegance, High Complexity)

```cpp
// Separate rules for descent (pre-order) vs ascent (post-order)
constexpr auto descent_rules =
  annihilator_rules |    // Short-circuit early
  identity_rules |       // Unwrap early
  expansion_rules;       // Expand before children simplify

constexpr auto ascent_rules =
  collection_rules |     // Collect after children simplified
  factoring_rules |      // Factor after children simplified
  constant_fold;         // Fold after everything simplified

constexpr auto two_phase_simplify =
  TwoPhaseSimplify{descent_rules, ascent_rules};
```

## Benchmark Scenarios

To validate improvements:

1. **Short-circuit**: `0 * (x + y + z + ... + w)` with 100 terms

   - Current: Simplifies all 100 terms, then multiplies by 0
   - Optimized: Returns 0 immediately

2. **CSE**: `(x*x + x*x) + (x*x + x*x)`

   - Current: Simplifies `x*x` four times
   - Optimized: Simplifies `x*x` once, reuses

3. **Identity unwrapping**: `exp(log(x*x + y*y))`

   - Current: Simplifies argument, then applies exp/log, then cancels
   - Optimized: Cancels exp/log immediately, then simplifies

4. **Two-phase**: `(x+x+x) * (y+y) + (x+x+x) * (y+y)`
   - Current: Bottom-up, multiple passes
   - Optimized: Descent to check annihilators, ascent to collect terms

## Conclusion

**Key Insights:**

1. **Outermost-first** for annihilators and identities (short-circuit)
2. **Innermost-first** for collection and folding (need simplified children)
3. **Two-phase** for conflicting rule requirements (expand down, collect up)
4. **CSE** for redundant subexpressions (compile-time type equality check)

**Recommended Approach:**
Start with Phase 1 (short-circuit patterns) - high impact, low complexity.
Then add Phase 2 (CSE) if compile times allow.
Phase 3 (two-phase) is most elegant but requires rethinking rule organization.

**Elegance vs Efficiency:**
The two-phase approach offers the most elegant rule definitions:

- "This rule applies going down" (descent_rules)
- "This rule applies coming up" (ascent_rules)

But it requires maintaining two separate rule sets. The short-circuit approach
offers 80% of the benefit with 20% of the complexity.
