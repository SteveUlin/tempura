# Design: Symbolic Graph Compute Execution

## Problem Statement

`bayes3` demonstrates a powerful pattern: symbolic expressions that form DAGs with multiple interpretations:
- **Sampling**: traverse DAG, draw random values
- **Log-probability**: traverse DAG, accumulate probabilities
- **Symbolic log-prob**: traverse DAG, build symbolic expression
- **Differentiation**: transform symbolic expression

This pattern is duplicated across three traversal implementations (`SampleTraverse`, `LogProbTraverse`, `SymbolicTraverse`) with nearly identical structure. The core insight - "expression DAGs with multiple interpreters" - should be upleveled into `symbolic3`.

## Goals

1. **Unify traversal patterns** into a single generic framework
2. **Separate graph structure from interpretation**
3. **Enable new interpreters** without duplicating traversal logic
4. **Maintain compile-time type safety** and zero runtime overhead
5. **Support lazy evaluation** for HMC/NUTS efficiency

## Current State Analysis

### symbolic3 Today

```
symbolic3/
├── core.h          # Symbolic, Expression<Op, Args...>, Symbol, Literal
├── evaluate.h      # evaluate(expr, BinderPack{x=1, y=2})
├── derivative.h    # diff(expr, symbol)
├── traversal.h     # Fold, Unfold, TopDown, BottomUp (for simplification)
├── operators.h     # +, -, *, /, sin, cos, etc.
└── ...
```

Key abstractions:
- `Symbolic` concept: anything that represents a symbolic value
- `Expression<Op, Args...>`: compound expression with operator and children
- `evaluate()`: bind symbols to values, reduce to concrete result
- `diff()`: symbolic differentiation producing new expression
- Traversal strategies: control order of transformation (for simplification)

### bayes3 Today

```
bayes3/
└── core.h          # RandomVar, DeterministicVar, Trace, sampleTrace, etc.
```

Key abstractions:
- `GraphNode` concept: node in DAG with `parents()`, `sym()`, `nodeUnnormalizedLogProb()`
- `RandomVar`: stochastic node with distribution
- `DeterministicVar`: computed node (deterministic transform)
- `Trace`: maps symbols to sampled values
- `IdSet`: compile-time set for deduplication during traversal
- Three traversal implementations with same structure but different "effects"

### The Duplication Problem

All three traversers follow identical structure:
```cpp
template <typename Node, typename Visited, ...>
struct SomeTraverse {
  static constexpr bool skip = Visited::template contains<typename Node::id_type>;
  using NewVisited = typename Visited::template insert<typename Node::id_type>;

  static auto apply(const Node& node, Visited visited, ...) {
    if constexpr (skip) {
      return {identity_value, visited};
    } else {
      auto [parent_result, after_parents] = foldParents<0>(node.parents(), NewVisited{}, ...);
      auto this_result = computeThisNode(node, ...);
      return {combine(parent_result, this_result), after_parents};
    }
  }
};
```

The only differences:
1. **Effect type**: RNG for sampling, Trace for log-prob, nothing for symbolic
2. **Identity value**: `Trace{}`, `0.0`, `Literal{0.0}`
3. **Node computation**: `sampleDist()`, `logProbDist()`, `nodeUnnormalizedLogProb()`
4. **Combination**: trace merging, addition, symbolic addition

---

## Proposed Design

### 1. DAGNode Concept in symbolic3 (Lean, No Probability Semantics)

The graph structure concept lives in `symbolic3`, stripped of Bayesian-specific requirements:

```cpp
namespace tempura::symbolic3 {

// A node in a directed acyclic computation graph
template <typename T>
concept DAGNode = requires(const T& node) {
  typename T::id_type;       // Unique type for deduplication
  typename T::symbol_type;   // Symbol<Id> for binding
  { T::sym() } -> Symbolic;
  { node.parents() };        // tuple of parent DAGNodes
};

}  // namespace tempura::symbolic3
```

The Bayesian-specific concept stays in `bayes3`:

```cpp
namespace tempura::bayes3 {

// A probabilistic node with log-prob contribution
template <typename T>
concept ProbabilisticNode = symbolic3::DAGNode<T> && requires(const T& node) {
  { node.nodeUnnormalizedLogProb() } -> Symbolic;
};

}  // namespace tempura::bayes3
```

This separation allows `symbolic3::DAGNode` to be used for non-probabilistic DAGs (e.g., computation graphs, dependency tracking) without requiring probability semantics.

### 2. Interpreter Concept with Reader/Writer Pattern

The sampling interpreter reveals a critical issue: it needs to **read** from the accumulated trace (to evaluate distribution parameters) before **writing** to it (with the sampled value). A pure monoid pattern doesn't capture this bidirectional flow.

We use a **Reader/Writer pattern** where the interpreter explicitly receives and returns state:

```cpp
namespace tempura::symbolic3 {

// An interpreter defines how to process a computation graph
template <typename I>
concept Interpreter = requires {
  typename I::state_type;     // Threaded state (Trace, accumulator, etc.)
  typename I::context_type;   // Side context (RNG, const Trace for queries, etc.)
};

// Per-node processing requirement
template <typename I, typename Node>
concept InterpretableNode = requires(const Node& node,
                                     typename I::state_type state,
                                     typename I::context_type& ctx) {
  // Process node: receives current state, returns new state
  // The interpreter can READ from state (e.g., evaluate params with trace)
  // and WRITE to state (e.g., add sampled value to trace)
  { I::processNode(node, state, ctx) } -> std::convertible_to<typename I::state_type>;
};

}  // namespace tempura::symbolic3
```

**Key semantics for `processNode`:**
- **Input state**: accumulated state from processing all parent nodes (fully accumulated, not a delta)
- **Output state**: new state after processing this node (also fully accumulated)
- The interpreter decides what "processing" means - it can:
  - Read from state to get parent values (sampling)
  - Compute a contribution and combine with state (log-prob)
  - Build up a larger symbolic expression (symbolic log-prob)

### 3. Generic DAG Traversal

Single traversal implementation parameterized by interpreter:

```cpp
namespace tempura::symbolic3 {

template <typename Interpreter>
struct GraphTraverse {
  template <typename Node, typename Visited, typename Ctx>
  static auto apply(const Node& node, Visited, typename Interpreter::state_type state, Ctx& ctx)
      -> std::pair<typename Interpreter::state_type, /*NewVisited*/>;

private:
  template <std::size_t I, typename Parents, typename V, typename Ctx>
  static auto traverseParents(const Parents& parents, V visited,
                              typename Interpreter::state_type state, Ctx& ctx)
     -> std::pair<typename Interpreter::state_type, /*NewVisited*/>;
};

// Entry point
template <typename Interpreter, DAGNode Node, typename Ctx>
auto traverse(const Node& root, typename Interpreter::state_type initial, Ctx& ctx) {
  return GraphTraverse<Interpreter>::template apply<Node, IdSet<>>(
      root, IdSet<>{}, initial, ctx).first;
}

}  // namespace tempura::symbolic3
```

### 4. Example Interpreters

#### Sampling Interpreter (Reads AND Writes to Trace)

```cpp
namespace tempura::bayes3 {

template <typename Generator>
struct SamplingInterpreter {
  using state_type = /* Trace<...> */;      // Accumulated trace
  using context_type = Generator&;           // RNG

  template <ProbabilisticNode Node, typename TraceT>
  static auto processNode(const Node& node, TraceT parent_trace, Generator& rng) {
    // READ from trace: evaluate distribution parameters using parent values
    auto value = sampleDist(node.dist_, parent_trace, rng);
    // WRITE to trace: add this node's sampled value
    return mergeTraces(parent_trace, Trace{node.sym() = value});
  }
};

}  // namespace tempura::bayes3
```

#### Symbolic Log-Prob Interpreter (Pure Accumulation)

```cpp
namespace tempura::bayes3 {

struct SymbolicLogProbInterpreter {
  using state_type = /* Symbolic expression */;
  struct context_type {};  // No runtime context

  template <ProbabilisticNode Node, Symbolic Acc>
  static constexpr auto processNode(const Node& node, Acc parent_lp, context_type&) {
    // Pure combination: add this node's symbolic log-prob to accumulator
    return parent_lp + node.nodeUnnormalizedLogProb();
  }
};

}  // namespace tempura::bayes3
```

#### Numeric Log-Prob Interpreter (Read-Only Access to Trace)

```cpp
namespace tempura::bayes3 {

template <typename TraceT>
struct NumericLogProbInterpreter {
  using state_type = double;       // Accumulated log-prob
  using context_type = const TraceT&;  // Const reference to full trace

  template <ProbabilisticNode Node>
  static double processNode(const Node& node, double parent_lp, const TraceT& trace) {
    // READ from context (full trace) - no WRITE
    double value = trace[node.sym()];
    return parent_lp + logProbDist(node.dist_, value, trace);
  }
};

}  // namespace tempura::bayes3
```

---

## Lazy Evaluation for HMC/NUTS

For HMC and NUTS, we want to:
1. **Build once**: construct a symbolic log-prob expression from the DAG
2. **Differentiate once**: symbolically differentiate w.r.t. all parameters
3. **Evaluate many times**: evaluate both log-prob and gradients with different parameter values

The current design supports this via separation of concerns:

```cpp
// Phase 1: Build symbolic expression (once per model)
auto lp_expr = jointUnnormalizedLogProb(model_root);
auto grad_expr = diff(lp_expr, param_symbol);

// Phase 2: Create evaluator (once per model, captures expressions)
auto lp_fn = [&lp_expr](const auto& bindings) {
  return evaluate(lp_expr, bindings);
};
auto grad_fn = [&grad_expr](const auto& bindings) {
  return evaluate(grad_expr, bindings);
};

// Phase 3: Evaluate repeatedly (per HMC step)
for (int step = 0; step < num_steps; ++step) {
  auto bindings = currentPositionToBindings(position);
  double lp = lp_fn(bindings);
  double grad = grad_fn(bindings);
  // ... leapfrog integration
}
```

### CompiledGraph for Maximum Efficiency

For performance-critical paths, we can provide a `CompiledGraph` that pre-computes the evaluation structure:

```cpp
namespace tempura::symbolic3 {

template <Symbolic LogProbExpr, Symbolic... GradExprs>
struct CompiledGraph {
  // Pre-flattened expression for efficient evaluation
  // Symbol ordering determined at compile time

  template <typename... Values>
  auto evaluateLogProb(Values... values) const -> double;

  template <typename... Values>
  auto evaluateGradients(Values... values) const -> std::array<double, sizeof...(GradExprs)>;

  template <typename... Values>
  auto evaluateBoth(Values... values) const
      -> std::pair<double, std::array<double, sizeof...(GradExprs)>>;
};

template <DAGNode Root>
auto compile(const Root& root) {
  auto lp = jointUnnormalizedLogProb(root);
  // ... extract symbols, compute gradients, build CompiledGraph
}

}  // namespace tempura::symbolic3
```

---

## Compile-Time Cost Mitigation

### Problem: O(N^2) IdSet Instantiations

With N nodes, the current approach creates O(N^2) `IdSet` type instantiations as each node adds itself during traversal. For large graphs, this causes:
- Slow compile times
- Large object file sizes
- Deep template recursion

### Mitigation Options

**Option A: Runtime Tracking (Recommended for Large Graphs)**

For graphs with more than ~20 nodes, use runtime deduplication:

```cpp
namespace tempura::symbolic3 {

// Runtime visited set using type-erased id
struct RuntimeVisited {
  std::unordered_set<std::type_index> visited_;

  template <typename Id>
  bool contains() const {
    return visited_.count(std::type_index(typeid(Id))) > 0;
  }

  template <typename Id>
  void insert() {
    visited_.insert(std::type_index(typeid(Id)));
  }
};

// Choose strategy based on graph size
template <std::size_t NodeCount>
using VisitedSet = std::conditional_t<(NodeCount <= 20), IdSet<>, RuntimeVisited>;

}  // namespace tempura::symbolic3
```

**Option B: Linear IdSet Representation**

Store IDs as a type list without deduplication checks at insertion; check membership lazily:

```cpp
template <typename... Ids>
struct LinearIdSet {
  template <typename Id>
  static constexpr bool contains = (std::is_same_v<Id, Ids> || ...);

  template <typename Id>
  using insert = LinearIdSet<Id, Ids...>;  // Always prepend, O(1) instantiation
};
```

**Option C: Chunked/Hierarchical Sets**

Group IDs into chunks to reduce type explosion:

```cpp
template <typename Chunk1, typename Chunk2, ...>
struct ChunkedIdSet;
```

**Recommendation**: Start with compile-time IdSet (current approach) for correctness and simplicity. Add runtime tracking as an escape hatch when models exceed ~30 nodes.

---

## Alternative Design Patterns Considered

### Visitor Pattern

The classic OOP visitor pattern:

```cpp
struct NodeVisitor {
  virtual void visit(RandomVar&) = 0;
  virtual void visit(DeterministicVar&) = 0;
};

struct SamplingVisitor : NodeVisitor { ... };
struct LogProbVisitor : NodeVisitor { ... };
```

**Why not adopted:**
- Requires virtual dispatch (runtime overhead)
- Node types must be known upfront (no extension without modifying base)
- Cannot return different types per visitor (accumulator type varies)
- Loses compile-time type information needed for symbolic differentiation

### Tagless Final / Finally Tagless

Encode operations as type class methods:

```cpp
template <typename Repr>
struct BayesDSL {
  template <typename Mu, typename Sigma>
  static auto normal(Repr<Mu> mu, Repr<Sigma> sigma) -> Repr<NormalNode<Mu, Sigma>>;

  template <typename Node>
  static auto sample(Repr<Node>) -> Repr<double>;

  template <typename Node>
  static auto logProb(Repr<Node>) -> Repr<double>;
};

// Concrete representation
struct SamplingRepr { ... };
struct SymbolicRepr { ... };
```

**Why not adopted:**
- Requires significant DSL restructuring (not incremental)
- All operations must thread through the DSL type class
- More complex for simple use cases (user just wants to write `normal(mu, sigma)`)
- Good for new DSLs from scratch, but high migration cost for existing bayes3

### Why the Proposed Approach is Better

The **parameterized traversal with interpreter concept** offers:

1. **Incremental adoption**: Works with existing `RandomVar`/`DeterministicVar` types
2. **Zero runtime overhead**: Template metaprogramming, no virtual dispatch
3. **Type-safe accumulators**: Each interpreter declares its own state/accumulator type
4. **Compile-time symbolic**: Symbolic log-prob and differentiation remain constexpr
5. **Familiar pattern**: Similar to std::accumulate, folds, reduce operations
6. **Extensible**: New interpreters don't require modifying existing code

---

## Implementation Risks and Mitigations

### 1. Template Recursion Depth

**Risk**: Deep DAGs cause template recursion limit (typically 900-1024).

**Mitigation**:
- GCC/Clang support `-ftemplate-depth=N` for increasing limit
- Use iterative unrolling with index sequences for parent traversal
- Document maximum supported graph depth

### 2. Reference Type Deduction

**Risk**: Perfect forwarding and reference collapsing can cause dangling references.

**Mitigation**:
```cpp
// Always use remove_cvref_t when storing parent types
using Parent = std::remove_cvref_t<decltype(parent)>;

// State passing should use value semantics (Trace is cheap to copy)
// or explicit lvalue references in interpreter signature
```

### 3. Constexpr Compatibility

**Risk**: Some operations (RNG, runtime allocation) cannot be constexpr.

**Mitigation**:
- Symbolic log-prob: fully constexpr (no side effects)
- Sampling: not constexpr (uses RNG) - this is inherent, not a bug
- Mark `processNode` as `constexpr` where applicable; the framework doesn't require it

### 4. Error Messages

**Risk**: Template errors in deep nesting are unreadable.

**Mitigation**:
- Use `static_assert` with custom messages for concept failures
- Provide `is_valid_interpreter<I, Node>` type trait for diagnostics
- Consider wrapper types that provide cleaner error messages

---

## File Organization

```
symbolic3/
├── core.h              # (existing) Symbolic, Expression, etc.
├── evaluate.h          # (existing)
├── derivative.h        # (existing)
├── traversal.h         # (existing - for expression simplification)
├── dag_node.h          # NEW: DAGNode concept (lean, no prob semantics)
├── graph_traverse.h    # NEW: Generic DAG traversal with interpreters
├── id_set.h            # NEW: Type-level set (moved from bayes3)
└── ...

bayes3/
├── core.h              # RandomVar, DeterministicVar (use graph_traverse.h)
├── interpreters.h      # NEW: SymbolicLogProb, Sampling, NumericLogProb
├── probabilistic.h     # NEW: ProbabilisticNode concept (extends DAGNode)
└── ...
```

---

## Migration Path

1. **Phase 1**: Extract `IdSet` to `symbolic3/id_set.h`, add `DAGNode` concept to `symbolic3/dag_node.h`
2. **Phase 2**: Implement `GraphTraverse` in `symbolic3/graph_traverse.h`
3. **Phase 3**: Define `ProbabilisticNode` and interpreters in `bayes3/`
4. **Phase 4**: Refactor `bayes3` traversals to use interpreters
5. **Phase 5**: Delete duplicate traversal code, add runtime IdSet option

Each phase maintains backward compatibility - existing `bayes3` API unchanged.

---

## Design Decisions Summary

| Decision | Rationale |
|----------|-----------|
| Reader/Writer pattern over monoid | Sampling needs to read trace before writing |
| Separate DAGNode from ProbabilisticNode | symbolic3 shouldn't require probability semantics |
| State = fully accumulated (not delta) | Consistent semantics; interpreter controls combination |
| Compile-time IdSet with runtime escape hatch | Best of both worlds: type safety for small graphs, scalability for large |
| Template interpreters over visitors | Zero overhead, type-safe accumulators, constexpr support |
| Incremental over tagless final | Lower migration cost, familiar C++ patterns |

---

## Open Questions

1. **CompiledGraph API**: What's the best interface for pre-compiled evaluation? Should it integrate with autodiff types?

2. **Multi-output graphs**: Current design assumes single root. Extend to handle multiple observed variables cleanly?

3. **Partial evaluation**: Can we support fixing some parameters while keeping others symbolic?

4. **GPU/SIMD**: Future direction - batch evaluation over multiple traces?

---

## Success Criteria

- [ ] `bayes3` uses `GraphTraverse` instead of hand-written traversals
- [ ] Adding new interpreter requires ~20 lines, not ~50
- [ ] No performance regression (measure with HMC example)
- [ ] Compile times don't significantly increase for current model sizes
- [ ] Sampling interpreter correctly handles trace read/write dependency
- [ ] Symbolic log-prob remains fully constexpr
