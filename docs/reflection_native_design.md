# Reflection-Native Design for symbolic4

What would this library look like if built from scratch with C++26 reflection,
instead of retrofitting reflection onto pre-reflection idioms?

This document identifies patterns that are vestigial — carried over from an era
where types couldn't be inspected as values — and proposes replacements that
treat reflection as a foundational primitive rather than an optimization.

## The Core Diagnosis

The current codebase uses reflection **tactically**: replacing recursive template
specializations with `consteval` loops while preserving the same type-level data
structures (`TypeList`, `IdSet`, `CompressedTuple`, index_sequence gymnastics).

A reflection-native design uses reflection **structurally**: types carry only what
the type system enforces, and everything else becomes consteval computation over
`std::meta::info` values.

The principle: **if the compiler can derive it from the type, don't store it.**

## 1. TypeList Is Vestigial — Use Packs and Info Directly

### Problem

`TypeList<Ts...>` exists because pre-reflection C++ couldn't manipulate parameter
packs as values. You had to wrap `...` in a type to pass it around, index into it,
or filter it. Every TypeList operation was a recursive template specialization chain.

Reflection makes packs directly inspectable. `template_arguments_of(^^SomeType<Ts...>)`
returns the pack as a `vector<info>`. You can index, filter, search — with normal
loops.

### Current: TypeList as carrier

```cpp
template <typename Dist, typename Id, typename DimsList>   // DimsList = TypeList<Countries, Years>
struct IndexedRandomVar { ... };

// Nesting appends via Concat
using NewDims = Concat_t<OldDims, TypeList<DimTag>>;
return IndexedRandomVar<Dist, Id, NewDims>{rv.dist()};
```

### Reflection-native: variadic packs directly

```cpp
template <typename Dist, typename Id, typename... Dims>
struct IndexedRandomVar { ... };

// Nesting: pattern-match the pack, expand with new dim
template <typename DimTag, typename Dist, typename Id, typename... OldDims>
constexpr auto plate(const IndexedRandomVar<Dist, Id, OldDims...>& rv) {
    return IndexedRandomVar<Dist, Id, OldDims..., DimTag>{rv.dist()};
}
```

No `Concat_t`, no `TypeList`, no intermediate types. The compiler already knows
how to expand packs.

### What replaces TypeList operations?

| Old (TypeList)                          | New (reflection)                                     |
|-----------------------------------------|------------------------------------------------------|
| `Contains_v<T, TypeList<Ts...>>`        | `((^^T == ^^Ts) \|\| ...)` fold, or consteval loop   |
| `Get_t<I, TypeList<Ts...>>`             | `[:template_arguments_of(^^Outer)[I + offset]:]`     |
| `Head_t<TypeList<Ts...>>`               | First element of the pack directly                   |
| `Filter_t<Pred, TypeList<Ts...>>`       | Consteval function returning `info`, splice at use    |
| `Concat_t<TypeList<As...>, TypeList<Bs...>>` | Pack expansion: `As..., Bs...`                  |
| `Unique_t<TypeList<Ts...>>`             | Consteval dedup on `vector<info>`                     |
| `Size_v<TypeList<Ts...>>`               | `sizeof...(Ts)` directly                             |

**Delete**: `meta/type_list.h`, `meta/type_list_ops.h`. Move any surviving
algorithms into free consteval functions that operate on `vector<info>`.

### Where packs don't work: computed type lists

Some operations produce type lists that aren't derived from an existing pack —
`Filter`, `Transform`, `FlatMap` compute *new* sets of types from old ones.

These become consteval functions that return `std::meta::info`:

```cpp
// Returns info representing the filtered type pack
template <template <typename> class Pred, typename... Ts>
consteval auto filterTypes() -> std::meta::info {
    std::vector<std::meta::info> result;
    for (auto arg : {^^Ts...}) {
        if (Pred<[:arg:]>::value)
            result.push_back(arg);
    }
    return std::meta::substitute(^^TypePack, result);  // or any carrier
}
```

The question is what to splice *into*. You need some template to `substitute` the
result back through. Options:

- **Splice directly at the use site** where the target template is known:
  `std::meta::substitute(^^IndexedSymbol, concat(^^Id, filtered_dims))`
- **Use a minimal carrier** if you need to pass the list through generic code.
  This carrier is thinner than TypeList — it has no operations, just holds types.

The goal isn't zero carriers; it's zero *operations* on carriers. All computation
happens on `vector<info>`.

## 2. IdSet Is Vestigial — Use Consteval Sets

### Problem

`IdSet<Ids...>` in `let.h` tracks visited identities during expression traversal.
It's implemented as recursive template specializations: `IdSetContains` peels one
type at a time, `IdSetInsert` conditionally prepends.

This is exactly the kind of type-level data structure that reflection eliminates.

### Current: type-level set via recursive specialization

```cpp
template <typename Id, typename First, typename... Rest>
struct IdSetContains<Id, IdSet<First, Rest...>>
    : std::conditional_t<std::is_same_v<Id, First>, std::true_type,
                         IdSetContains<Id, IdSet<Rest...>>> {};
```

### Reflection-native: consteval function

```cpp
template <typename Id, typename... Ids>
consteval bool idSetContains() {
    return ((^^Id == ^^Ids) || ...);
}

// Or for the non-pack case, reflect the set type:
template <typename Id, typename Set>
consteval bool idSetContains() {
    for (auto arg : std::meta::template_arguments_of(^^Set))
        if (arg == ^^Id) return true;
    return false;
}
```

No recursive instantiation, no `conditional_t` chains.

## 3. CompressedTuple via `define_aggregate`

### Problem

`CompressedTuple` uses multiple inheritance from indexed `TupleLeaf<I, T>` types
with `[[no_unique_address]]` to achieve zero-overhead storage. This is clever but
complex — the inheritance hierarchy, `static_cast` for element access, and the
index sequence machinery exist because pre-reflection C++ can't programmatically
define struct members.

P2996 introduces `define_aggregate` — programmatic struct definition at compile
time.

### Current: inheritance-based compressed tuple

```cpp
template <SizeT I, typename T>
struct TupleLeaf {
    [[no_unique_address]] T value;
};

template <SizeT... Is, typename... Ts>
struct TupleImpl<IndexSequence<Is...>, Ts...> : TupleLeaf<Is, Ts>... { ... };

// Access via static_cast to the correct base
template <SizeT I, typename... Ts>
constexpr auto get(const CompressedTuple<Ts...>& tuple) {
    return static_cast<const TupleLeaf<I, Get_t<I, Ts...>>&>(tuple).value;
}
```

### Reflection-native: generated aggregate

```cpp
template <typename... Ts>
consteval auto makeCompressedLayout() -> std::meta::info {
    std::vector<std::meta::info> members;
    template for (constexpr auto t : {^^Ts...}) {
        // Each member gets [[no_unique_address]] and a generated name
        members.push_back(
            std::meta::data_member_spec(t, {.no_unique_address = true}));
    }
    return define_aggregate(^^struct{}, members);
}

template <typename... Ts>
using CompressedStorage = [:makeCompressedLayout<Ts...>():];
```

Element access uses `nonstatic_data_members_of` + splice:

```cpp
template <SizeT I, typename T>
constexpr auto get(const T& obj) {
    constexpr auto members = std::meta::nonstatic_data_members_of(^^T);
    return obj.[:members[I]:];
}
```

No inheritance hierarchy, no `static_cast`, no `IndexSequence`. The compiler
generates a flat struct with the right layout.

**Caveat**: `define_aggregate` support in Bloomberg's clang-p2996 is experimental.
This is a medium-term target, not an immediate refactor.

## 4. BinderPack via Member Reflection

### Problem

`BinderPack` uses multiple inheritance + `using Binders::operator[]...` to
create a heterogeneous lookup table. Each `TypeValueBinder<SymType, ValType>`
provides an `operator[]` overload, and the BinderPack inherits all of them.

This works, but the inheritance machinery is a workaround for not being able to
programmatically generate a struct with typed fields and a dispatching `operator[]`.

### Reflection-native approach

With `define_aggregate` (or even just `template for`), you can generate a flat
struct and a single `operator[]` that dispatches via reflection:

```cpp
template <typename... Binders>
struct BinderPack {
    CompressedStorage<Binders...> storage_;

    template <typename Sym>
    constexpr auto operator[](Sym) const -> decltype(auto) {
        constexpr auto members = std::meta::nonstatic_data_members_of(^^decltype(storage_));
        template for (constexpr auto m : members) {
            using B = [:std::meta::type_of(m):];
            if constexpr (std::is_same_v<typename B::symbol_type, Sym>)
                return storage_.[:m:].value();
        }
    }
};
```

The `template for` loop resolves at compile time — only the matching branch
survives. No inheritance, no `using...` trick.

**However**: the current `BinderPack` pattern is clean and works well. This is
an improvement in simplicity, not a fix for a bug. Lower priority.

## 5. Expression Introspection Without Accessor Traits

### Problem

The current design defines 15+ type traits to destructure expressions:
`is_atom_v`, `is_expression_v`, `get_op_t`, `get_args_t`, `get_id_t`,
`get_effect_t`, `same_atom_id_v`, etc.

Each trait is a named alias or variable template. Callers import the trait, apply
it to a type, and branch on the result. This creates a vocabulary layer between
"I have a type" and "I know what it contains."

### Reflection-native: query the type directly

Instead of pre-defined traits, query structural properties inline:

```cpp
// Current: named trait
if constexpr (is_expression_v<E>) {
    using Op = get_op_t<E>;
    // ...
}

// Reflection-native: structural query
if constexpr (std::meta::template_of(^^E) == ^^Expression) {
    using Op = [:std::meta::template_arguments_of(^^E)[0]:];
    // ...
}
```

The traits become unnecessary. But — and this matters — **named traits are still
good documentation.** `is_atom_v<T>` communicates intent better than
`template_of(^^T) == ^^Atom`. The traits should exist as thin consteval wrappers
(which they already are), not as complex template specializations (which they
already aren't).

**Verdict**: the current trait layer is already reflection-native in implementation.
The *names* add value. Keep them.

## 6. buildNestedSumOver Without TypeList Recursion

### Problem

`BuildNestedSumOver<TypeList<First, Rest...>, Expr>` recursively destructures a
TypeList via partial specialization to build nested `SumOver` nodes.

### Reflection-native: recursive function on a pack

```cpp
template <typename Expr>
constexpr auto buildNestedSumOver(Expr e) { return e; }

template <typename First, typename... Rest, typename Expr>
constexpr auto buildNestedSumOver(Expr e) {
    if constexpr (sizeof...(Rest) == 0)
        return sumOver<First>(e);
    else
        return sumOver<First>(buildNestedSumOver<Rest...>(e));
}

// Called from IndexedRandomVar<Dist, Id, Dims...>:
return buildNestedSumOver<Dims...>(instanceLogProb());
```

No struct template, no TypeList, no partial specialization. Plain function
recursion on a pack.

## 7. SymbolicState Without std::tuple

### Problem

`SymbolicState` wraps `std::tuple<Symbols...>` and `std::tuple<Specs...>`,
then uses `tuple_element_t` and `std::get<I>` for access. The `SymbolIndex`
helper already uses reflection to find a symbol's position — but the surrounding
code still relies on tuple infrastructure.

### Reflection-native: consteval struct generation

The ideal `SymbolicState` would generate its slot layout via reflection:

```cpp
template <typename... Slots>   // Slot = SymbolicSlot<Sym, Spec>
class SymbolicState {
    // Generate a struct with one member per slot
    using Layout = [:defineSlotLayout<Slots...>():];

    Layout layout_;
    std::vector<double> data_;

    template <typename Sym>
    auto operator[](Sym) -> decltype(auto) {
        // Find the slot for this symbol via consteval search
        constexpr auto members = std::meta::nonstatic_data_members_of(^^Layout);
        template for (constexpr auto m : members) {
            using S = [:std::meta::type_of(m):];
            if constexpr (std::is_same_v<typename S::symbol_type, Sym>) {
                auto& slot = layout_.[:m:];
                if constexpr (slot.isScalar())
                    return data_[slot.offset_];
                else
                    return std::span{data_.data() + slot.offset_, slot.size()};
            }
        }
    }
};
```

No `tuple_element_t`, no `std::get<I>`, no `index_sequence` fold for offset
computation. The `template for` loop over members replaces all of it.

**Near-term simplification** (doesn't need `define_aggregate`): replace
`std::tuple` with a flat `CompressedTuple` and reflection-based lookup.

## 8. Expression Node Layout

### The deeper question

Currently, `Expression<Op, Args...>` stores children in a `CompressedTuple<Args...>`,
and `Atom<Id, Effect>` stores its effect as a `[[no_unique_address]]` member. Access
is via `arg<I>()` which calls `get<I>(args_)`.

With reflection, you can ask: **do we need stored children at all?**

Most expression nodes are **empty types**. `Expression<AddOp, Symbol<X>, Symbol<Y>>`
contains no runtime data — the entire tree is in the type. The `CompressedTuple`
optimizes this (empty members take no space), but reflection reveals a simpler
truth: if the children are stateless, don't store them. Reconstruct them on demand:

```cpp
template <SizeT I>
constexpr auto arg() const {
    using ArgType = [:std::meta::template_arguments_of(^^Expression<Op, Args...>)[I + 1]:];
    if constexpr (std::is_empty_v<ArgType>)
        return ArgType{};       // reconstruct — no storage needed
    else
        return get<I>(args_);   // stored — has runtime state
}
```

This is a micro-optimization, but it illustrates the principle: reflection lets
you decide *at the point of use* whether to store or reconstruct, instead of
committing to a storage strategy in the type definition.

A more radical version: only store non-empty children, and use reflection to map
logical indices to physical storage positions. The `CompressedTuple` already
achieves this layout effect via `[[no_unique_address]]`, so the win here is
conceptual clarity rather than performance.

## 9. Pattern Matching via Structural Reflection

### Problem

The pattern matching system (`strategy/pattern.h`) defines `PatternVar`, wildcard
types (`AnyExpr`, `AnyConstant`, `AnySymbol`), and `match()` overloads. Each
wildcard needs its own `match` function, and adding a new pattern category requires
a new type + overload.

### Reflection-native: match by structural query

Instead of wildcard types, match by querying the target's structure:

```cpp
// Current: dedicated wildcard type
template <Symbolic S>
constexpr bool match(AnyConstant, S) {
    return is_constant_v<S> || is_fraction_v<S>;
}

// Reflection-native: structural predicate (hypothetical)
consteval bool isConstantLike(std::meta::info t) {
    if (!std::meta::has_template_arguments(t)) return false;
    auto tmpl = std::meta::template_of(t);
    return tmpl == ^^Constant || tmpl == ^^Fraction;
}
```

You could define match predicates as `consteval bool(std::meta::info)` functions
and compose them. But the current wildcard types are already simple, readable, and
work with the expression algebra (wildcards are `SymbolicTag` so they participate
in pattern expressions). This is a case where the pre-reflection idiom is actually
fine.

**Verdict**: keep wildcards. They're cleaner than predicate functions for this
use case.

## Summary: What Changes, What Stays

### Delete or replace

| What | Why | Replacement |
|------|-----|-------------|
| `TypeList<Ts...>` | Pack carrier no longer needed | Variadic packs + consteval `info` functions |
| `type_list.h` operations (`Get`, `Head`, `Tail`) | Reflection does this in O(1) | `template_arguments_of()[I]` at point of use |
| `type_list_ops.h` (`Filter`, `Transform`, etc.) | Already `vector<info>` internally, TypeList is just I/O format | Consteval functions returning `info` |
| `IdSet` + recursive contains/insert | Trivial with fold expressions or consteval loops | `((^^Id == ^^Ids) \|\| ...)` |
| `MakeIndexedSymbol<Id, TypeList<Dims...>>` | Exists only to unwrap TypeList into pack | Direct `IndexedSymbol<Id, Dims...>` |
| `BuildNestedSumOver<TypeList<...>>` | Struct-based recursion on TypeList | Function recursion on pack |

### Simplify (medium-term, needs `define_aggregate`)

| What | Why | Replacement |
|------|-----|-------------|
| `CompressedTuple` | Inheritance-based EBO workaround | `define_aggregate` with `no_unique_address` members |
| `BinderPack` inheritance | `using...` trick for dispatch | `template for` over generated members |
| `SymbolicState` tuple machinery | `tuple_element_t` + `std::get` boilerplate | Reflection-based slot layout |

### Keep as-is

| What | Why |
|------|-----|
| `Atom<Id, Effect>`, `Expression<Op, Args...>` | The type-level expression tree IS the design. Reflection doesn't change this. |
| Named type traits (`is_atom_v`, `is_expression_v`, etc.) | Already reflection-native internally. Names add documentation value. |
| `BinderPack` (short-term) | Works well, inheritance trick is clean. Replace when `define_aggregate` is stable. |
| Pattern wildcards (`AnyExpr`, `AnyConstant`, etc.) | Simple, readable, integrate with expression algebra. |
| `Grad<Expr>` nesting | Type-level rank encoding is elegant and correct. |
| `CompressedTuple` (short-term) | Works correctly. Replace when `define_aggregate` is available. |

## The Guiding Principle

**Reflection turns type-level computation into value-level computation.**

Pre-reflection C++ forced you to encode algorithms as recursive template
specializations — types as the "instruction set" of a bizarre computation model.
Every TypeList operation, every trait chain, every index_sequence fold was a
program written in a language that happens to be the C++ type system.

Reflection lets you write those algorithms as normal C++ — loops, vectors,
conditionals — that happen to operate on type information. The expression tree
itself (`Atom`, `Expression`, `Grad`) should remain in the type system, because
that's where zero-cost abstraction lives. But the *algorithms that manipulate
those types* should be ordinary consteval code.

The boundary: **types encode structure, consteval code computes over structure.**
TypeList, IdSet, and CompressedTuple sit on the wrong side of that boundary.
They encode computation as structure. Move the computation into consteval
functions, and the structures simplify or disappear.
