# autodiff — automatic differentiation

An exploration of automatic differentiation as a *composable* framework rather than a
grab-bag of modes. The organizing idea, borrowed from JAX:

> **There are only two primitives — `jvp` (forward) and `vjp` (reverse). Everything else
> is seeding and composition.**

A Jacobian is batched jvp (or vjp); a Hessian is reverse-over-forward; a Hessian-vector
product is one forward-over-reverse pass. Name the two atoms well and the rest falls out —
including the parts that mix forward and reverse — with no special-case machinery.

## The two cores

| Mode | Type | What it is |
|------|------|------------|
| **forward / JVP** | `Dual<T, G>` (`dual.h`) | a + bε, ε²=0. One pass gives value + directional derivative. The carry `G` decouples from `T`: `G=T` is one direction, a vector is K directions (jacfwd), `Dual` is second order. |
| **reverse / VJP** | `Tape<T>` owns a `WeightedDag<T>` and hands out `Var<T>` leaves; ops record, `tape.backward` seeds + sweeps (`tape.h`, `weighted_dag.h`) | a flat Wengert tape: each op records its local partials + parent indices; recording order *is* topological order, so the backward sweep is a linear array scan. |

The two design choices that make the whole thing work:

- **The tape is flat and index-based.** Operations are recorded in evaluation order into
  CSR arrays (`WeightedDag<T>`: deps, weights, edge offsets), so the reverse sweep is just
  iterating them backward — O(#ops), cache-friendly, and correct on shared subexpressions
  (where a pointer-graph DAG re-walks shared nodes and goes exponential). Nodes are plain
  `size_t` indices: they survive reallocation, and per-node data (adjoints, tangents) is
  any external `std::vector` aligned by index. Weights are mutable (`edges(n)`) and the
  dag `clear()`s keeping capacity, so one tape re-records or re-linearizes with no
  steady-state allocation. This is why the old `node.h` (virtual + `shared_ptr`-per-node +
  recursive sweep) was deleted.
- **Every core is generic over its scalar `T`.** A `Tape<Dual<U>>` records a forward-dual
  computation and sweeps it backward — i.e. forward-over-reverse — with no second AD
  system. That single fact gives Hessians and HVPs for free.

## The transform layer

Written once as a generic callable `f` (`[](auto... x){ ... }`), differentiated any way:

| Transform | File | Builds on |
|-----------|------|-----------|
| `jvp(f, x, v)` → (f(x), J·v) | `transform.h` | forward / `Dual` |
| `vjp(f, x)` → (f(x), pullback) | `transform.h` | reverse / tape |
| `grad`, `valueAndGrad` | `transform.h` | reverse, unit seed |
| `jacfwd` / `jacrev` → `Dense<T,M,N>` | `jacobian.h` | batched jvp / vjp |
| `hvp`, `gradAndHvp`, `hessian` → `Dense<T,N,N>` | `hessian.h` | forward-over-reverse |
| `Jet<T,N>` (arbitrary-order univariate) | `jet.h` | truncated Taylor recurrences |

Seeds are always caller-supplied tangent/cotangent vectors — never a baked-in `1.`
literal (which would break non-`double` scalars). Jacobians and Hessians land in
`src/matrix` `Dense`/`InlineDense` on the `Scalar` concept.

## Choosing a mode

- **Gradient of a scalar (∇f), many inputs** → reverse (`grad`). One sweep, all partials.
- **Directional derivative / few inputs, many outputs (tall J)** → forward (`jvp`, `jacfwd`).
- **Wide J (few outputs)** → reverse (`jacrev`). `jacfwd` and `jacrev` agree; shape decides cost.
- **Hessian / HVP** → `hessian` / `hvp` (forward-over-reverse; `hvp` never forms H).
- **High-order univariate derivatives** → `jet` (forward Taylor).

## Notes

- **constexpr**: the forward modes (`Dual`, `Jet`) are fully `constexpr` (GCC trunk makes
  `<cmath>` constexpr). The reverse tape allocates, so it is runtime-only — reverse mode is
  not a constexpr path, and that is fine.
- **Exception-free**: domain violations (`sqrt`(<0), `log`(≤0), …) and misuse (mismatched
  tapes) `assert` rather than throw.
- **Parallelism**: each Jacobian column / Hessian column is an independent sweep, so the
  seed loops are the natural seam for a `task/` (`bulk`) parallel backend. Kept serial
  until there is a measured need — the tape itself stays sequential (a scheduler would only
  re-derive the order recording already gives for free).
