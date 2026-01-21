# Task: Implement symbolic4

## References
- Design doc: /home/sulin/tempura/DESIGN-symbolic4.md
- TODO list: /home/sulin/tempura/TODO-symbolic4.md
- Existing code: /home/sulin/tempura/src/symbolic3/
- Code style: /home/sulin/tempura/CLAUDE.md

## Core Abstraction

Everything is an `Atom<Id, Strategy>`:

```cpp
template <typename Id, typename Strategy>
struct Atom : SymbolicTag {
  using id_type = Id;                    // void for anonymous
  using strategy_type = Strategy;
  [[no_unique_address]] Strategy strategy_;
};

// Strategies
struct Lookup {};                        // Symbol: look up in bindings
template <typename T> struct Intrinsic { T value; };  // Literal: embedded value
template <typename D> struct Sample { D dist_; };     // RandomVar: sample
template <typename E> struct Compute { E expr_; };    // DeterministicVar: evaluate

// Aliases
template <typename Id> using Symbol = Atom<Id, Lookup>;
template <typename T> using Literal = Atom<void, Intrinsic<T>>;
template <typename Id, typename D> using RandomVar = Atom<Id, Sample<D>>;
template <typename Id, typename E> using DeterministicVar = Atom<Id, Compute<E>>;
```

## Directory Structure

```
src/symbolic4/
├── core.h              # Atom, strategies, Expression, Symbolic concept
├── compressed.h        # Pair, CompressedTuple, IdSet with [[no_unique_address]]
├── operators.h         # AddOp, MulOp, etc. (can reuse from symbolic3)
│
├── scheme/
│   ├── fold.h          # fold<Interpreter>(expr, ctx)
│   ├── para.h          # para<Interpreter>(expr, ctx)
│   └── fold_unique.h   # foldUnique with IdSet tracking
│
├── interpreter/
│   ├── eval.h          # Evaluation interpreter (fold)
│   ├── diff.h          # Differentiation interpreter (para)
│   └── to_string.h     # Pretty printing (fold)
│
├── let.h               # let(expr) - give identity to expression
├── evaluate.h          # Convenience wrapper
└── derivative.h        # Convenience wrapper
```

## Implementation Order

### Phase 1: Foundation
1. `compressed.h` - Pair, CompressedTuple with [[no_unique_address]]
2. `core.h` - Atom, strategies, type traits
3. Write tests for core types

### Phase 2: Schemes
4. `scheme/fold.h` - catamorphism
5. `scheme/para.h` - paramorphism
6. Write tests showing fold vs para

### Phase 3: Interpreters
7. `interpreter/eval.h` - evaluation
8. `interpreter/diff.h` - differentiation
9. Port existing symbolic3 tests

### Phase 4: Identity & DAG
10. `let.h` - let(expr) function
11. `scheme/fold_unique.h` - deduplicating fold
12. `IdSet` and `ExtractIds` for dependency tracking

## Reusing meta/ Directory

**Can reuse directly:**
- `meta/type_list.h` - TypeList, Get_t, Head_t, Tail_t, Size_v
- `meta/type_list_ops.h` - Concat_t, Filter_t, Transform_t, Contains_v, Unique_t
- `meta/utility.h` - SizeT, IndexSequence, MakeIndexSequence, isSame, Conditional, DerivedFrom, isEmpty
- `meta/tags.h` - Tag<T>, TypeList<Types...>

**Must create new (meta/tuple.h doesn't use [[no_unique_address]]):**
- `compressed.h` - Pair and CompressedTuple with [[no_unique_address]] for zero-overhead storage

**IdSet implementation using existing ops:**
```cpp
// IdSet is just a TypeList used for tracking visited identities
template <typename... Ids>
using IdSet = TypeList<Ids...>;

// Use existing ops:
// - Contains_v<IdSet, Id> to check if visited
// - Concat_t<IdSet, TypeList<Id>> to add new id
// - Unique_t to deduplicate
```

**ExtractIds using Filter_t and Transform_t:**
```cpp
// Extract all atom Ids from expression tree
// Uses Filter_t to keep only atoms with non-void ids
// Uses FlatMap_t to recurse into Expression children
```

## Key Constraints

- NO std:: in metaprogramming (std::tuple doesn't do [[no_unique_address]])
- Use custom Pair, CompressedTuple from compressed.h
- Prefer meta/ utilities over reimplementing
- Everything constexpr
- Zero runtime overhead - types ARE the representation
- Follow CLAUDE.md naming: PascalCase types, camelCase functions

## Testing

Use unit.h framework:
```cpp
"atom has correct strategy"_test = [] {
  auto x = Symbol<struct X>{};
  static_assert(std::same_as<typename decltype(x)::strategy_type, Lookup>);
};
```

Prefer static_assert for compile-time properties.

## Success Criteria

- [ ] `fold<Eval>(expr, bindings)` works like current `evaluate()`
- [ ] `para<Diff>(expr, var)` works like current `diff()`
- [ ] `let(expr)` creates DeterministicVar with fresh identity
- [ ] `foldUnique` deduplicates by identity
- [ ] All existing symbolic3 behavior preserved
- [ ] No performance regression
