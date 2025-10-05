# Symbolic2 Naming Conventions

## Overview

This document establishes naming conventions for the symbolic2 library based on the code review conducted on October 4, 2025.

## Function Naming Patterns

### Apply Pattern

Use `apply*` prefix for functions that apply transformation rules to expressions.

**Pattern:** `apply<Category>Rules`

**Examples:**

- `applyPowerRules` - applies power simplification rules
- `applyAdditionRules` - applies addition simplification rules
- `applyMultiplicationRules` - applies multiplication simplification rules
- `applyExpRules` - applies exponential simplification rules
- `applyLogRules` - applies logarithm simplification rules
- `applySinRules` - applies sine simplification rules
- `applyCosRules` - applies cosine simplification rules

**Rationale:** "Apply" is an active verb that clearly indicates the function performs an action. "Rules" is general and encompasses all types of transformations (identities, simplifications, normalizations).

### Normalize Pattern

Use `normalize*` prefix for functions that convert expressions to canonical form.

**Pattern:** `normalize<Operation>`

**Examples:**

- `normalizeSubtraction` - converts `a - b` to `a + (-1·b)`
- `normalizeDivision` - converts `a / b` to `a·b⁻¹`

**Rationale:** "Normalize" clearly indicates transformation to a standard/canonical representation. This is semantically different from general "simplification" - it's specifically about ensuring consistent form.

### Fold Pattern

Use `fold*` prefix for compile-time constant evaluation.

**Pattern:** `fold<Category>`

**Examples:**

- `foldConstants` - evaluates constant-only expressions at compile time

**Rationale:** "Fold" is the standard functional programming term for reducing a structure to a single value. Industry-standard terminology.

## Type Naming Patterns

### Core Types

Use PascalCase for primary symbolic types.

**Pattern:** `Noun` or `NounPhrase`

**Examples:**

- `Symbol` - symbolic variable
- `Constant` - compile-time numeric constant
- `Expression` - compound symbolic expression
- `PatternVar` - pattern matching variable
- `BinderPack` - heterogeneous symbol-to-value bindings

### Wildcard Patterns

Use lowercase Unicode for readability and mathematical convention.

**Pattern:** `𝐬𝐩𝐞𝐜𝐢𝐚𝐥_𝐧𝐚𝐦𝐞` (Unicode bold lowercase)

**Examples:**

- `𝐚𝐧𝐲` - universal wildcard (matches anything)
- `𝐞𝐱𝐩𝐫` - expression wildcard (compound expressions only)
- `𝐜` - constant wildcard (numeric constants only)
- `𝐬` - symbol wildcard (symbols only)

**Rationale:** Unicode makes patterns visually distinct from regular code. Mathematical typography convention.

### Pattern Variables

Use lowercase with underscore suffix.

**Pattern:** `letter_` (single character followed by underscore)

**Examples:**

- `x_`, `y_`, `z_` - general pattern variables
- `a_`, `b_`, `c_` - coefficient pattern variables
- `f_`, `g_` - function pattern variables
- `n_`, `m_` - exponent/index pattern variables

**Rationale:** Underscore suffix clearly distinguishes pattern variables from actual symbols. Consistent with common mathematical convention.

### Rewrite Systems

Use PascalCase with descriptive category name.

**Pattern:** `<Category>Rules`

**Examples:**

- `PowerRules` - power expression transformations
- `AdditionRules` - addition transformations
- `MultiplicationRules` - multiplication transformations
- `LogRules` - logarithm transformations
- `DiffRules` - differentiation rules

**Rationale:** "Rules" suffix clearly indicates a collection of rewrite rules. Category prefix enables easy grouping.

### Rule Categories (Nested)

Use PascalCase for category namespaces, PascalCase for subcategories.

**Pattern:** `<Operation>RuleCategories::<Subcategory>`

**Examples:**

- `AdditionRuleCategories::Identity` - zero addition rules
- `AdditionRuleCategories::Ordering` - canonical ordering rules
- `MultiplicationRuleCategories::Distribution` - distributive property
- `LogRuleCategories::Expansion` - logarithm expansion rules

**Rationale:** Namespaces organize related categories. Clear hierarchy.

## Operator Naming

### Mathematical Operators

Use Unicode symbols where standard in mathematical notation.

**Examples:**

- `π` - pi constant (not `PI` or `pi`)
- `e` - Euler's number (not `E`)

**Rationale:** Mathematical convention. Immediately recognizable.

### Function Operators

Use lowercase function names matching mathematical notation.

**Examples:**

- `sin`, `cos`, `tan` - trigonometric functions
- `log`, `exp` - logarithm and exponential
- `pow` - power function
- `sqrt` - square root

**Rationale:** Standard mathematical function names. Consistent with `<cmath>`.

## Comment Style

### Design Rationale Comments

Focus on **why** decisions were made, not **what** the code does.

**Good:**

```cpp
// Category ordering matters: Distribution before Associativity prevents
// un-factoring distributed terms
```

**Avoid:**

```cpp
// This function applies distribution rules to multiplication expressions
// and then applies associativity rules for normalization
```

### Inline Comments

Use sparingly. Only for non-obvious constraints or mathematical identities.

**Good:**

```cpp
Rewrite{sin(x_ * Constant<-1>{}), Constant<-1>{} * sin(x_)}  // Odd function
```

**Avoid:**

```cpp
Rewrite{0_c + x_, x_}  // Zero is additive identity (obvious from math)
```

### Section Headers

Keep minimal. Code structure should be self-documenting.

**Good:**

```cpp
// Trigonometric rules
constexpr auto DiffSin = ...
constexpr auto DiffCos = ...
```

**Avoid:**

```cpp
// =============================================================================
// TRIGONOMETRIC DIFFERENTIATION RULES - SECTION 4.2
// =============================================================================
//
// This section contains all differentiation rules for trigonometric functions
// including sine, cosine, tangent, and their inverse functions. Each rule
// implements the standard calculus derivative formula with chain rule support.
```

## File Naming

Use lowercase with underscores for headers.

**Pattern:** `category_purpose.h`

**Examples:**

- `core.h` - core types and concepts
- `pattern_matching.h` - pattern matching infrastructure
- `recursive_rewrite.h` - recursive rewrite support
- `symbolic_diff_notation.h` - differentiation notation

**Rationale:** Consistent with C++ standard library conventions. Underscores separate words.

## Variable Naming

### Local Variables

Use descriptive lowercase names with underscores.

**Examples:**

- `bindings_ctx` - binding context
- `recursive_fn` - recursive function parameter
- `result` - transformation result

### Template Parameters

Use PascalCase for type parameters, SCREAMING_CASE for value parameters.

**Type Parameters:**

- `Symbolic` - concept constraint
- `Op` - operator type
- `Args` - argument types
- `Pattern` - pattern type
- `Replacement` - replacement type

**Value Parameters:**

- `Index` - current index
- `depth` - recursion depth

## Consistency Guidelines

1. **Verb-noun ordering:** Functions use verb-first (e.g., `applyRules`, not `rulesApply`)
2. **Abbreviations:** Avoid unless standard (e.g., `diff` for differentiation is acceptable)
3. **Plurals:** Use when referring to collections (e.g., `Rules`, `Categories`)
4. **Prefixes:** Use consistently within a category (e.g., all application functions use `apply*`)

## Anti-Patterns to Avoid

### ❌ Vague Naming

```cpp
processExpr()        // What kind of processing?
handleData()         // What data? What handling?
doStuff()           // Too generic
```

### ❌ Hungarian Notation

```cpp
strName             // Type is in the type system
iCount              // Redundant prefix
ptrExpr             // Not needed in modern C++
```

### ❌ Inconsistent Prefixes

```cpp
applyAdditionRules()
performMultiplicationRules()  // Use 'apply' consistently
executeExpRules()            // Use 'apply' consistently
```

### ❌ Redundant Type Suffixes

```cpp
SymbolType          // Just 'Symbol'
ExpressionClass     // Just 'Expression'
RuleStruct          // Just 'Rule' or specific rule type
```

## Summary

**Key Principles:**

1. **Clarity over brevity** - descriptive names preferred
2. **Consistency** - same patterns for same concepts
3. **Mathematical convention** - use Unicode where standard
4. **Action verbs** - functions start with verbs
5. **Minimal comments** - focus on "why", not "what"

These conventions result in self-documenting code that is:

- Easy to understand
- Easy to extend
- Consistent with mathematical notation
- Maintainable long-term
