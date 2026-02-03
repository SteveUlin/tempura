# symbolic4 Implementation TODO

## Milestone 1: The Unified Core (Refactoring)

**Goal**: Establish `Atom` and `Strategy` as the core primitives, replacing ad-hoc Interpreters.

- [x] **Core Data Structures**
  - [x] Ensure `Atom<Id, Effect>` is the universal leaf node in `src/symbolic4/core.h`
  - [x] Implement `CompressedTuple` and `Pair` with `[[no_unique_address]]` (in `src/symbolic4/compressed.h`)
  - [x] Verify `Expression<Op, Args...>` structure

- [x] **Strategy Engine Port** (from `symbolic3`)
  - [x] Port `PatternVar`, `BindingContext` to `src/symbolic4/strategy/pattern.h`
  - [x] Port `Rule`, `RuleSet` to `src/symbolic4/strategy/rule.h`
  - [x] Port Combinators (`Seq`, `Choice`, `Try`) to `src/symbolic4/strategy/combinator.h`
  - [x] Port Traversals (`BottomUp`, `TopDown`, `Innermost`) to `src/symbolic4/strategy/combinator.h`

- [ ] **Legacy Compatibility Adapter** (optional - may not need)
  - [ ] Implement `InterpreterAdapter<I>` to wrap legacy `terminal`/`combine` classes into a `BottomUp` strategy

## Milestone 2: Declarative Simplification

**Goal**: Replace monolithic simplification with declarative rules.

- [x] **Algebraic Rules** (in `interpreter/simplify.h` using fold scheme)
  - [x] Constant folding: `2+3` -> `5`
  - [x] Identity elimination: `x+0`->`x`, `x*1`->`x`, `x*0`->`0`
  - [x] Self operations: `x-x`->`0`, `x/x`->`1`
  - [x] Inverse cancellation: `exp(log(x))`->`x`
  - [x] Canonical ordering: `y+x` -> `x+y` (in `simplify/canonicalize.h`)

## Milestone 3: Bayesian Improvements

**Goal**: Switch Bayesian inference components to the Strategy system.

- [ ] **LogProb Strategy**
  - [ ] Re-implement `jointLogProb` as a strategy (currently uses `foldUnique`)
  - [ ] Handle DAG traversal (visit each node once)

- [x] **Differentiation Strategy** (in `strategy/diff.h`)
  - [x] Define `DiffRules` as declarative `Rewrite` rules
  - [x] Terminal rules via pattern matching (`AnySymbol`, `AnyConstant`, `AnyLiteral`)
  - [x] `FailUnhandledDiff` for compile-time errors on missing rules
  - [x] Uses `innermost` traversal with `simplify`

- [x] **Plate Notation** (in `indexed/`)
  - [x] Implement `Indexed<Id>` atom
  - [x] Implement plate transforms

## Milestone 4: Advanced Features (Future)

- [ ] **Code Generation**: Strategy to emit C++ source code
- [ ] **GPU Placement**: Strategy to tag nodes for GPU execution
- [ ] **Parallel Composition**: `Zygomorphism` (run two strategies in parallel)

## Testing Milestones

- [ ] `EvalStrategy` produces same results as legacy `evaluate()`
- [ ] `SimplifyStrategy` correctly simplifies `x + 0` and `x * 0`
- [ ] `DiffStrategy` produces correct gradients for HMC
- [ ] HMC sampler compiles and runs with new engine