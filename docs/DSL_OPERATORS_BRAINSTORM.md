# Smart Dispatch DSL - Design Brainstorming

## Goal

Create elegant, readable syntax for expressing traversal strategies and rule organization.

---

## Core Operators

### 1. Phase Composition: `~>` (flows into)

```cpp
// Read as: "descent flows into ascent"
auto simplify = descent_rules ~> ascent_rules;

// Expands to: TwoPhase{descent_rules, ascent_rules}
```

**Rationale:**

- Visual: shows data flow from top to bottom
- Mnemonic: `~` suggests wave/flow, `>` suggests direction
- Not confusing with existing operators

### 2. Quick Check: `?>` (try first)

```cpp
// Read as: "try quick patterns first, then full pipeline"
auto optimized = quick_patterns ?> full_pipeline;

// Expands to: ShortCircuit{quick_patterns, full_pipeline}
```

**Rationale:**

- `?` suggests conditional/quick check
- `>` suggests fallback/continuation
- Reads naturally: "try this? then this"

### 3. Strategy Choice: `|` (or, existing)

```cpp
// Already have this, keep it
auto rules = rule1 | rule2 | rule3;
```

### 4. Strategy Sequence: `>>` (then, existing)

```cpp
// Already have this, keep it
auto pipeline = stage1 >> stage2 >> stage3;
```

### 5. Conditional Application: `when` (existing)

```cpp
auto conditional = when([](ctx) { return should_apply(ctx); }, rules);
```

---

## New Operators for Smart Dispatch

### 6. Traverse Mode: `@` (at)

```cpp
// Read as: "apply rules at innermost level"
auto strategy = rules @ innermost;
auto strategy = rules @ outermost;
auto strategy = rules @ bottomup;
auto strategy = rules @ topdown;

// Expands to: innermost(rules), outermost(rules), etc.
```

**Rationale:**

- `@` suggests location/position
- Postfix feels natural for traversal mode
- Chainable: `(rules1 | rules2) @ innermost`

**Alternative:** `<@>` for symmetry

```cpp
auto strategy = innermost <@> rules;  // Pre-order feel
```

### 7. Fixpoint Iteration: `*` (star, like regex)

```cpp
// Read as: "apply rules repeatedly until fixpoint"
auto strategy = rules*;

// With explicit limit
auto strategy = rules * 10;  // Max 10 iterations

// Expands to: FixPoint<10>{rules}
```

**Rationale:**

- `*` universally means "zero or more" (regex, grammars)
- Postfix like Kleene star
- Optional numeric argument for limit

### 8. Depth Control: `%` (modulo/limit)

```cpp
// Read as: "apply rules up to depth 5"
auto strategy = rules % 5;

// Expands to: WithDepthLimit<5>{rules}
```

**Rationale:**

- `%` suggests modulo/bounds/limits
- Not commonly used in this context
- Postfix for consistency

### 9. Memoization: `$` (cache, like bash variables)

```cpp
// Read as: "cache results of these rules"
auto strategy = $rules;

// Expands to: Memoized{rules}
```

**Rationale:**

- `$` suggests value/stored thing
- Single character prefix
- Recognizable from shell scripting

---

## Composition Examples

### Example 1: Basic Two-Phase

```cpp
auto simplify =
  (annihilators | identities) ~>
  (collection | factoring);

// Reads: "annihilators or identities, flows into collection or factoring"
```

### Example 2: With Short-Circuit

```cpp
auto optimized =
  quick_patterns ?>
  (expansion_rules ~> collection_rules);

// Reads: "try quick patterns first,
//         then expansion flows into collection"
```

### Example 3: Full Smart Pipeline

```cpp
auto smart =
  quick_patterns ?>
  (expansion @ topdown ~> collection @ bottomup)*;

// Reads: "try quick patterns first,
//         then expand top-down flows into collect bottom-up,
//         repeat until fixpoint"
```

### Example 4: With All Features

```cpp
auto advanced =
  (annihilators | identities) ?>
  $((distribution | unwrapping) @ topdown ~>
    (factoring | constant_fold) @ bottomup)* % 5;

// Reads: "try annihilators or identities first,
//         then cache results of:
//           distribute or unwrap top-down, flows into
//           factor or fold bottom-up,
//         repeat until fixpoint (max 5 levels deep)"
```

---

## Alternative Syntax Styles

### Style A: Named Functions (Current)

```cpp
auto simplify = short_circuit(
  quick_patterns,
  two_phase(
    topdown(expansion_rules),
    bottomup(collection_rules)
  )
);
```

**Pros:** Explicit, familiar
**Cons:** Verbose, nested parens

### Style B: Operators (Proposed)

```cpp
auto simplify =
  quick_patterns ?>
  (expansion_rules @ topdown ~> collection_rules @ bottomup);
```

**Pros:** Concise, visual flow
**Cons:** Learning curve, operator overloading

### Style C: Fluent/Builder (Alternative)

```cpp
auto simplify =
  strategy()
    .quick(quick_patterns)
    .descend(expansion_rules, topdown)
    .ascend(collection_rules, bottomup)
    .fixpoint()
    .build();
```

**Pros:** Discoverable, chainable
**Cons:** Verbose, requires builder class

### Style D: Hybrid (Mix)

```cpp
auto simplify =
  quick_patterns ?>
  two_phase(
    expansion_rules @ topdown,
    collection_rules @ bottomup
  )*;
```

**Pros:** Balance of clarity and concision
**Cons:** Mixed styles

---

## Recommended Operators

### Core Set (Implement First)

1. **`~>` (flow)** - Two-phase composition
2. **`?>` (try)** - Short-circuit
3. **`@` (at)** - Traversal mode
4. **`*` (star)** - Fixpoint with optional limit

### Extended Set (Later)

5. **`%` (limit)** - Depth control
6. **`$` (cache)** - Memoization

### Keep Existing

- **`|` (or)** - Choice combinator
- **`>>` (then)** - Sequential composition
- **`when`** - Conditional application

---

## Implementation Priority

### Phase 1: Essential Operators

```cpp
operator~>  // Two-phase
operator?>  // Short-circuit
operator@   // Traversal mode
```

### Phase 2: Convenience Operators

```cpp
operator*   // Fixpoint
operator%   // Depth limit
```

### Phase 3: Advanced Features

```cpp
operator$   // Memoization (if needed)
```

---

## Syntax Decision Matrix

| Feature       | Named Function         | Operator        | Fluent                    | Recommended |
| ------------- | ---------------------- | --------------- | ------------------------- | ----------- |
| Two-Phase     | `two_phase(d, a)`      | `d ~> a`        | `.descend(d).ascend(a)`   | **`~>`**    |
| Short-Circuit | `short_circuit(q, f)`  | `q ?> f`        | `.quick(q).fallback(f)`   | **`?>`**    |
| Traversal     | `innermost(r)`         | `r @ innermost` | `.traverse(r, innermost)` | **`@`**     |
| Fixpoint      | `FixPoint{r}`          | `r*`            | `.fixpoint(r)`            | **`*`**     |
| Depth Limit   | `WithDepthLimit<5>{r}` | `r % 5`         | `.maxDepth(r, 5)`         | **`%`**     |

---

## Real-World Examples

### Example: Physics Simplification

```cpp
// User-defined physics rules
constexpr auto physics =
  Rewrite{E_, mass * c * c} |         // E = mc²
  Rewrite{momentum, mass * velocity}; // p = mv

// Smart pipeline with physics rules
constexpr auto physics_simplify =
  (annihilators | physics) ?>         // Quick checks with physics
  (expansion @ topdown ~>
   (collection | physics) @ bottomup)*; // Expand, collect with physics
```

### Example: Symbolic Math

```cpp
constexpr auto symbolic_mode =
  identities ?>                        // Cancel exp(log(x))
  (expansion @ topdown ~>              // Expand products
   collection @ bottomup) % 10;        // Collect terms (max depth 10)
```

### Example: Numeric Evaluation

```cpp
constexpr auto numeric_mode =
  constant_fold ?>                     // Fold constants immediately
  (identities ~> constant_fold)*;      // Simplify and fold repeatedly
```

---

## Parser-Friendly Syntax

For potential future DSL parsing:

```cpp
// Could be parsed from string literals
constexpr auto simplify = R"(
  (annihilators | identities) ?>
  (expansion @ topdown ~> collection @ bottomup)*
)"_strategy;
```

---

## Final Recommendation

**Implement these 4 operators first:**

1. **`~>`** - Two-phase (most important)
2. **`?>`** - Short-circuit (high impact)
3. **`@`** - Traversal mode (good ergonomics)
4. **`*`** - Fixpoint (cleaner than function call)

**Syntax:**

```cpp
auto simplify =
  quick ?>
  (descent @ topdown ~> ascent @ bottomup)*;
```

This gives us:

- ✅ Visual data flow
- ✅ Concise but readable
- ✅ Composable
- ✅ No ambiguity with existing operators
- ✅ Scales to complex pipelines

Let's implement this!
