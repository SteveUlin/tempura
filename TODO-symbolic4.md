# symbolic4 Implementation TODO

## Phase 1: Core Infrastructure

- [ ] Create `src/symbolic4/` directory structure
- [ ] Implement `Atom<Id, Strategy>` base template
- [ ] Implement binding strategies:
  - [ ] `Lookup` (for Symbol)
  - [ ] `Intrinsic<T>` (for Literal/Constant)
  - [ ] `Sample<D>` (for RandomVar)
  - [ ] `Compute<E>` (for DeterministicVar)
- [ ] Implement `Pair` with `[[no_unique_address]]` (can't reuse meta/tuple.h - lacks compression)
- [ ] Implement `CompressedTuple` with `[[no_unique_address]]`
- [ ] `IdSet` - use `TypeList` from `meta/type_list.h` with ops from `meta/type_list_ops.h`
  - Reuse: `Contains_v`, `Concat_t`, `Unique_t`
- [ ] `ExtractIds<E>` - use `Filter_t`, `FlatMap_t` from `meta/type_list_ops.h`

## Phase 2: Recursion Schemes

- [ ] Implement `fold<Interpreter>(expr, ctx)` — catamorphism
- [ ] Implement `para<Interpreter>(expr, ctx)` — paramorphism
- [ ] Implement `foldUnique<Interpreter>(expr, visited, ctx)` — deduplicating fold
- [ ] Define `Interpreter` concept

## Phase 3: Core Interpreters

- [ ] `Eval` interpreter (fold) — evaluate expression with bindings
- [ ] `Diff<Var>` interpreter (para) — symbolic differentiation
- [ ] `ToString` interpreter (fold) — pretty printing
- [ ] `ExtractSymbols` interpreter (fold) — collect all Symbol ids

## Phase 4: Probabilistic Atoms

- [ ] Implement `let(expr)` function to give identity to expressions
- [ ] Adapt `RandomVar` as `Atom<Id, Sample<Dist>>`
- [ ] Adapt `DeterministicVar` as `Atom<Id, Compute<Expr>>`
- [ ] Remove explicit `Parents...` from RandomVar/DeterministicVar
- [ ] Implement `Sampler` interpreter
- [ ] Implement `LogProbAccumulator` interpreter

## Phase 5: bayes3 Migration

- [ ] Refactor `bayes3/core.h` to use symbolic4 atoms
- [ ] Replace hand-written DAG traversals with `foldUnique`
- [ ] Update `sampleTrace` to use `Sampler` interpreter
- [ ] Update `jointUnnormalizedLogProb` to use `LogProbAccumulator`
- [ ] Ensure all bayes3 tests pass

## Phase 6: Future Atom Types (Priority Order)

- [ ] `Observed<Id, D>` — conditioned on data
- [ ] `Indexed<Id, I, E>` — plate notation
- [ ] `Transformed<Id, E, J>` — reparameterization with Jacobian
- [ ] `Param<Id>` — optimizable parameters

## Phase 7: Interpreter Composition

- [ ] `Compose<I1, I2>` — sequential composition
- [ ] `Parallel<I1, I2>` — run both, return pair (zygomorphism)
- [ ] `ForwardAD<Var, Binders>` — eval + diff in single pass

## Phase 8: Grammars (Optional)

- [ ] Define concept-based grammars for DSL validation
- [ ] `CalcExpr` — simple arithmetic grammar
- [ ] `BayesExpr` — probabilistic model grammar

## Testing Milestones

- [ ] `fold<Eval>` matches current `evaluate()` behavior
- [ ] `para<Diff>` matches current `diff()` behavior
- [ ] `foldUnique<LogProbAccumulator>` matches current `jointUnnormalizedLogProb()`
- [ ] No performance regression in HMC benchmarks
- [ ] All existing symbolic3 tests pass with symbolic4

## Documentation

- [ ] Update DESIGN-symbolic4.md as implementation progresses
- [ ] Add inline documentation for core concepts
- [ ] Create examples showing atom extensibility

---

## Speculative: Other Domains (from Brainstorming)

These are future possibilities if the core abstraction proves useful:

### Heterogeneous Compute
- [ ] `HostVar<Id, T>` / `DeviceVar<Id, T>` — CPU/GPU placement
- [ ] `PlacementInterpreter` — automatic device placement
- [ ] `TransferInterpreter` — insert memory copies

### Task Graphs
- [ ] `Task<Id, F>` / `Future<Id, T>` — async computation
- [ ] `ScheduleInterpreter` — find parallelism, topological sort
- [ ] `ExecuteInterpreter` — thread pool execution

### Dataflow / Streaming
- [ ] `Source<Id, T>` / `Sink<Id, T>` — stream endpoints
- [ ] `Window<Id, W>` — batching/windowing
- [ ] Stream fusion interpreter

### Query Plans
- [ ] `Table<Id, Schema>` / `Filter<Id, P>` / `Join<Id, L, R, K>`
- [ ] `CostEstimator` interpreter
- [ ] `Optimizer` interpreter — join reordering, predicate pushdown
