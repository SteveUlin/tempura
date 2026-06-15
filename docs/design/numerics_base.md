# Numerics Base — Design

**Status:** vision / pre-implementation · **Date:** 2026-06-15

A greenfield design for tempura's numerical-computing base: a C++26 library built to
*learn* numerical methods and to back a Bayesian (NUTS) sampler. This is not a
refactor of the existing `matrix3` code — it is the target that code evolves into.
(`matrix3` stays as the module codename; the number is flavor, not API.)

In one line: **ergonomic, eager, value-semantic types on top of the C++ stdlib's
`mdspan`/`mdarray` design, plus the numerical algorithms the standard omits — so you never
leave C++ for BLAS/LAPACK.** The stdlib gives the shape vocabulary and the view; we give the
ergonomics and the math.

---

## 1. Thesis

> **Definitively good, in 2026, means Julia's composability without Julia's
> silent-wrong-answer tax.**

Write each algorithm *once* over a concept; swap the element type
(`double` → `Dual` for gradients → `interval` → `quantity` for units) and the
unchanged algorithm just works — **but only along axes the compiler can verify.**

Julia's "it just works" and "it silently doesn't" are the same architecture —
composition with no enforced contract — seen from two sides. C++26 keeps the upside
and inverts the downside:

- **concepts** make the customization contract *enforced* (a missing operation is a
  compile error pointing at the exact requirement, not a quietly-wrong posterior) and
  *discoverable* (reflection can enumerate what a type must provide);
- **monomorphization** pays the specialization cost at *build* time instead of as
  Julia's JIT / invalidation tax at *run* time;
- **value semantics + purity** keep the door open to autodiff / vmap / GPU as later
  transforms without committing to a tracer now.

Everything below serves that one inversion.

---

## 2. Principles (each on its argument)

### P1 — Algorithms name a *concept*, never a concrete type

*Argument:* this is the composability mechanism, stripped of Julia's runtime. A kernel
that depends only on the operations `Scalar` guarantees admits any conforming element
the instant it conforms — nobody edits anybody.

```cpp
// GOOD — generic over the CONCEPT; free functions throughout (cols/rows, not members).
constexpr auto cholesky(const Matrix2D auto& a) {
  using T = scalar_t<decltype(a)>;          // element type comes from the argument
  auto l = zerosLike(a);                     // same shape & storage kind as a
  for (std::size_t j = 0; j < cols(a); ++j) {
    T d = a[j, j];
    for (std::size_t k = 0; k < j; ++k) d -= l[j, k] * l[j, k];
    l[j, j] = sqrt(d);                       // unqualified: ADL finds Dual's sqrt
    for (std::size_t i = j + 1; i < rows(a); ++i) {
      T s = a[i, j];
      for (std::size_t k = 0; k < j; ++k) s -= l[i, k] * l[j, k];
      l[i, j] = s / l[j, j];
    }
  }
  return l;
}
// cholesky(Matrix<double>)      → numeric factor
// cholesky(Matrix<Dual<double>>) → DIFFERENTIATED factor, same source
```

```cpp
// REJECTED — gating on std::is_arithmetic_v silently excludes Dual (it's a class),
// so the whole autodiff path quietly fails to participate. This is a LIVE bug in the
// current scalar×matrix operator.
template <class T> requires std::is_arithmetic_v<T>
auto operator*(T, const SomeMatrix&);        // ✗ excludes Dual / quantity / interval
```

### P2 — Kernels are pure functions over views

*Argument:* purity is the shared precondition of *every* future capability — Halide
retiling, JAX grad/vmap, a thread/GPU backend. It costs nothing today (pure kernels are
just better C++) and violating it once forecloses all of it permanently. The asymmetry
decides it.

```cpp
// The kernel: reads inputs, writes the caller's output, no hidden state, no aliasing
// assumptions. This is the layer a future backend overloads.
template <Scalar T>                          // Ext2 = std::dextents<std::size_t, 2>
constexpr void mulInto(std::mdspan<T, Ext2> out, std::mdspan<const T, Ext2> a,
                       std::mdspan<const T, Ext2> b);   // out/a/b are 2D std::mdspan views

// The operator is sugar: pick the result type, allocate, delegate. Owns nothing clever.
template <Scalar T, std::size_t M, std::size_t K, std::size_t N>
constexpr auto operator*(const Matrix<T, M, K>& a, const Matrix<T, K, N>& b)
    -> Matrix<T, M, N> {
  Matrix<T, M, N> out{};
  mulInto(out.view(), a.view(), b.view());
  return out;
}
```

Rejected: stateful solver objects that mutate captured state, ambient/global RNG,
in-place mutation as the *default* surface.

### P3 — Shape, rank, layout, element live in the type

*Argument:* this makes the compiler the abstract interpreter JAX needs a tracer for.
Shape mismatch is a compile error, free, with no runtime pass.

```cpp
Matrix<double, 3, 4> a;
Matrix<double, 4, 2> b;
auto c = a * b;          // c : Matrix<double, 3, 2>  — deduced at compile time

Matrix<double, 3, 3> bad;
auto e = a * bad;        // COMPILE ERROR: inner extents 4 ≠ 3 (no runtime check needed)
```

Storage is chosen by which *type* you name (see §3), not inferred from the extents:

```cpp
InlineMatrix<double, 3, 3> small;     // stack, compile-time shape, constexpr
Matrix<double>             big(n, n); // heap, runtime shape
```

### P4 — Eager, value-semantic, loud errors — bank them

*Argument:* JAX's worst ergonomics (trace-only debugging, silent OOB clamping,
shape-recompile surprises) exist *only* to serve compile-to-accelerator. An eager CPU
library gets the opposite for free.

```cpp
auto c = a + b;   // c owns its storage → `auto` is ALWAYS safe (no dangling proxy)
c[10, 0];         // out of bounds → assert fires and crashes (never a silent clamp)
```

Rejected: lazy expression templates as the default (they make `auto x = a + b` a
dangling trap and show expression-tree types instead of values); silent saturation.

### P5 — One local derivative rule per primitive; higher-order is composition

*Argument:* JAX's and functorch's economics — `O(primitives + transforms)` local rules
instead of `O(primitives × transforms)` hand-written kernels. Reverse-mode is the
transpose of the same rules.

```cpp
// The autodiff element type: a value paired with its derivative.
template <Scalar T>
struct Dual {
  T value{};
  T deriv{};
};

// Each primitive defines its local rule ONCE (the chain rule, written down once)…
template <Scalar T>
constexpr auto sin(Dual<T> x) -> Dual<T> { return {sin(x.value), cos(x.value) * x.deriv}; }
template <Scalar T>
constexpr auto exp(Dual<T> x) -> Dual<T> { auto e = exp(x.value); return {e, e * x.deriv}; }

// …and every generic algorithm (P1) differentiates for free.
```

### P6 — Keep the trusted core tiny; optimization is composable user-space

*Argument:* Exo's thesis and Halide's autoscheduler failure both show the durable shape
is a small set of correctness-preserving primitives, not a monolith that bakes in
scheduling. Tiling / fusion / parallelism are layered hints, never core.

---

## 3. The type model

**Ergonomics on top of the stdlib design.** We reimplement the *full* `std::mdarray`
interface ourselves — `mdarray<T, Extents, LayoutPolicy, Container>`, N-dimensional and
policy-generic — faithfully, so it is a true drop-in the day libstdc++ ships `std::mdarray`
(adopted into C++26 but absent from GCC 16 trunk as of 2026-06; `std::mdspan` *is* present, so
the view side is real today). `mdarray` is the owning analog of `mdspan`: storage + the same
`extents`/`layout`/`accessor`, with `to_mdspan()`. We own only the container; the shape/view
vocabulary is `std`.

```cpp
// The owning core — a faithful reimplementation of std::mdarray's full interface
// (N-d, container/layout-policy generic). We default column-major (std defaults row-major).
template <Scalar T, class Extents, class LayoutPolicy = std::layout_left,
          class Container = std::vector<T>>
class mdarray {
 public:
  constexpr auto operator[](auto... idx) -> T&;        // C++23 multidim
  constexpr auto extents() const -> const Extents&;
  constexpr auto to_mdspan();                           // the borrowed std::mdspan view
  // value semantics (deep copy); std-faithful ctors.
};

// Matrix-flavored 2D ALIASES — the ergonomic happy path; storage = which alias you name:
template <Scalar T>
using Matrix = mdarray<T, std::dextents<std::size_t, 2>, std::layout_left, std::vector<T>>;
template <Scalar T, std::size_t R, std::size_t C>
using InlineMatrix = mdarray<T, std::extents<std::size_t, R, C>, std::layout_left,
                             std::array<T, R * C>>;     // stack, static, constexpr
template <Scalar T, std::size_t N> using Vec = InlineMatrix<T, N, 1>;  // a vector IS N×1
```

The 2D ergonomics live as **free functions** over these aliases — `rows(m)`, `cols(m)`,
`transpose(m)`, the operators — ADL-found, consistent with "math is free functions," so they
apply to any `mdarray`-shaped value, owning or view. `Matrix`/`InlineMatrix` are not new
classes; they're 2D instantiations of the one owning core, differing only in container
(`vector` vs `array`) and extents (dynamic vs static).

Both model the **same `Matrix2D` concept**, and algorithms are generic over that concept —
so the owning matrix is passed **directly**, with no `.view()` at the call site (it already
has `[i,j]` and extents; there is nothing to convert):

```cpp
cholesky(small);          // owning stack matrix — passes directly
cholesky(big);            // owning heap matrix — identical code
cholesky(transpose(a));   // a view-adapter also models Matrix2D — also direct
```

Everything is a **free function** (`cholesky`, `transpose`, `rows`, `cols`, operators —
ADL-found); there is no caller-facing member API. Internally an algorithm normalizes its
argument to one `std::mdspan` via a free `view(m)` so the heavy body instantiates per
view-type, not per owning-type — a bloat-control detail, never the call-site API. (We prefer
this concept-generic boundary over an implicit `Matrix → std::mdspan` conversion, which would
reintroduce the dangling-temporary footgun. `to_mdspan()` exists only as std::mdarray
interop, not the idiomatic path.)

This loses nothing on constexpr: a constexpr matrix is inherently fixed-size — exactly
`InlineMatrix` — so the heap default for *runtime* matrices forfeits no compile-time
evaluation. Static shape-checking lives on `InlineMatrix` (sizes known → mismatch is a
compile error); the heap `Matrix` is runtime-shaped (runtime asserts).

```cpp
// The non-owning view IS std::mdspan — there is no wrapper. Algorithms are generic over
// the Matrix2D concept (which std::mdspan AND our owning mdarray both satisfy), so they
// accept owning matrices, mdspan views, and adapters (transpose/submatrix) alike. The only
// tempura abstraction at the view layer is the CONCEPT, never a wrapper type.
auto v = a.to_mdspan();   // a borrowed, zero-copy std::mdspan over a's storage
```

The element type is the **composition axis**: `<double>` computes, `<Dual<double>>`
differentiates — on either storage type, through the unchanged algorithm.

Precision rule (decided): **no scalar promotion.** `T op T → T`; matrix⊕matrix requires
identical element type; a scalar coefficient is taken in the matrix's type
(`2.0 * Matrix<double>` is fine); change precision with an explicit `cast<U>(a)`. The
argument: promotion trades a *visible* cast for an *invisible* one, and a silent
precision shift mid-pipeline is a correctness hazard for a sampler.

### Structured matrices (triangular, symmetric, banded)

These are **not new owning types** — they're the three `mdarray`/`mdspan` customization
points over the same dense storage, which is exactly what reimplementing the full interface
buys:

- **Layout** (`(i,j) → offset`) = *storage* structure — a packed layout stores only the
  triangle/band. Deferred until memory demands it.
- **Accessor** (`offset → element`) = *interpretation* — an upper-triangular accessor returns
  `0` when `i>j`; a symmetric accessor reads `(j,i)` when `i>j` (store half, read full).
- **Type tag** (a marker on a view) = compile-time *dispatch* — `Lower{L.view()}` selects
  forward-substitution by overload.

```cpp
auto u = upperTriangular(a.view());     // accessor: reads 0 below the diagonal
auto s = symmetric(a.view());           // accessor: reads (j,i) when i>j
auto x = solve(Lower{l.view()}, b);     // tag: → forward-substitution
```

Discipline: add a *specific* structure when an algorithm needs it — the first is the
triangular solve out of Cholesky/LU — never a speculative lattice of formats.

### Sparse arrays & the LinearOperator seam (deferred)

Sparse is a different *concept*, not a different storage behind `Matrix2D`. The access
contracts are incompatible: dense `m[i,j]` is an O(1) `T&` and every cell exists; a sparse
missing cell is a *structural zero* (no reference to hand back), a write *inserts* (changes
the sparsity structure), and iteration visits nonzeros, not all `(i,j)`. Force one concept
and you either lie (a shared-zero reference — a foot-gun) or break it. A sparse Cholesky is
a different algorithm (fill-reducing ordering, symbolic factorization), not dense Cholesky
with new storage. So dense and sparse do **not** share the element-access concept.

They unify one level **up**, at the `LinearOperator` concept — `apply(x) → A·x` (and
`applyTranspose`). Iterative/Krylov solvers (CG, GMRES, Lanczos), eigensolvers, and
Hessian-vector products are written against *that*, so they run on a dense matrix, a sparse
matrix, or a **matrix-free** operator (a function computing `A·x` without ever forming `A`)
unchanged. This is the real composition seam — and the one a sampler wants
(large/structured covariances used only via products; HVPs in Riemannian-HMC are inherently
matrix-free).

**Deferred (YAGNI):** the near-term hot path is small dense. To keep the door open without
building it — keep the dense access concept separate from the `LinearOperator` concept, so
`sparse/` slots in later as a sibling (its own CSR/CSC formats, two-phase build → freeze
construction) — and delete the half-baked COO currently bolted into the dense module.

---

## 4. Transforms

*Design for them as a precondition discipline now; do not build a tracer.* The discipline
(pure functions, value semantics, one rule per primitive) is the whole investment; the
transform itself is thin plumbing once the invariants hold.

```cpp
// Forward-mode gradient: seed the dual, run the unchanged f, read the derivative.
template <class F>
constexpr auto grad(F f) {
  return [f](auto x) { return f(Dual{x, decltype(x){1}}).deriv; };
}

constexpr auto f = [](auto x) { return sin(x) * x; };
auto df = grad(f);
df(2.0);   // cos(2)·2 + sin(2)

// vmap STARTS as a naive eager loop; the SIGNATURE is what composes. The implementation
// upgrades to a real batching interpreter later without changing call sites.
template <class F>
constexpr auto vmap(F f) {
  return [f](auto batch) { /* apply f per row, collect */ };
}
```

The gradient a NUTS sampler needs falls out by substitution: pass `Dual` through the
unchanged log-density. (See §8 for the reverse-mode caveat.)

---

## 5. Naming & namespaces — decided by ADL, not convention

```cpp
tempura::Matrix<double, 3, 3> a;
auto l = cholesky(a);            // flat tempura:: — unqualified, ADL-resolved
auto x = solve(a, b);

tempura::linalg::SVD svd{a};     // a NON-ADL noun → grouped only for discovery
```

*Argument:* generic kernels call `sqrt(x)`, `a*b`, `log(d)` **unqualified** — that *is*
the composability payoff (same source differentiates when `x` is `Dual`). ADL resolves
those through the argument's namespace. Split the composition vocabulary into a
sub-namespace and every generic kernel needs `using tempura::linalg::sqrt;` — ceremony
that scales with the API and, forgotten once, **silently binds to `std::` and returns a
wrong / non-differentiated answer.** So the namespace boundary can *break unqualified
generic dispatch*; that cost dominates for the vocabulary.

- **Flat `tempura`** for the composition vocabulary: `Scalar` arithmetic, the array/view
  types, the operators, the math primitives that carry derivative rules.
- **Sub-namespace only for non-ADL nouns**: factorization *class* names
  (`Cholesky`, `QR`, `SVD`) nobody calls unqualified in generic code — pure upside there
  (discovery), shortens nothing load-bearing.

---

## 6. Library structure

Layers may depend only *downward*. Types live in the single `tempura` namespace;
directories are organization, not namespaces.

```
src/
  scalar.h            [✓] Scalar concept (Field/Ring reserved, unbuilt)
  dual.h                  Dual<T> forward-mode autodiff scalar; one JVP rule per primitive
  rng.h                   counter-based, splittable, value-semantic RNG (reproducible chains)
  transforms.h            grad / vmap as higher-order functions over pure callables

  matrix3/                array + linear-algebra module (codename; types are tempura::)
    matrix_concept.h      the ONE contract: Matrix2D (mdspan-shaped, m[i,j], Scalar element).
                          The view IS std::mdspan — no wrapper; this concept is the only
                          tempura abstraction at the view layer.
    mdarray.h             full std::mdarray reimplementation (N-d); Matrix/InlineMatrix = 2D
                          aliases (vector/dynamic, array/static); .to_mdspan() → std::mdspan
    views.h               transpose / submatrix / row / col + materialize() (non-owning)
    kernels.h             pure free-function kernels over views (mulInto/addInto/dot/axpy)
    arithmetic.h          operator + - * , scalar *, hadamard — thin sugar over kernels
    executor.h            thin eager-CPU executor seam (backend hook for later)
    decompositions/       cholesky.h lu.h qr.h svd.h eigen.h solve.h  (the learning payload)

  bayes/                  consumer: log-density + NUTS skeleton (the integration test)
```

Dependency rule: `decompositions → kernels + matrix_concept`; `arithmetic → kernels`;
`kernels → views + matrix`; everything → `scalar`. Decompositions depend on the
*concept*, never on `Matrix` internals — so the same QR runs on a static matrix, a
dynamic one, a submatrix, or a future backend matrix unchanged.

What carries over from existing `matrix3`: the verified-correct **decomposition
algorithms** (the math), retargeted onto mdspan views. What we **drop**: matrix3's
hand-rolled `extents`/`layout`/`accessor` — replaced by `std::extents` /
`std::layout_*` / `std::mdspan`. Reuse std for plumbing; shape-bookkeeping isn't a
numerical method, so the learning budget goes to the algorithms, not to re-deriving the
view machinery. What's new: the two explicit owning types `Matrix` (heap) /
`InlineMatrix` (stack) — std has no owning matrix, this is the gap we fill — the `Matrix2D`
concept (one contract over mdspan-shaped views), `dual`, `transforms`, `rng`, the executor
seam, and the pure-kernel layer.

### Relationship to std::linalg / mdspan

We don't compete with `std::linalg` (P1673); we sit at a different layer and borrow its
model where it fits. `std::linalg` is BLAS over `std::mdspan`: free functions
(`matrix_product`, `triangular_solve`, …), **caller-allocates out-parameters**, **no owning
matrix type**, execution-policy parameters, mixed precision — exactly right for a *standard*,
which must not dictate your container. Our divergences, each deliberate:

- **It's a primitive layer; we're an application library.** We put a caller-allocates kernel
  layer *resembling* `std::linalg` underneath, then add owning value types
  (`Matrix`/`InlineMatrix`) and **eager value-returning operators** (`auto C = A*B`) on top —
  ergonomics a standard can't commit to.
- **It excludes factorizations; they're our core.** P1673 is BLAS, not LAPACK — no
  LU/QR/SVD/Cholesky/eigen. Those are the learning payload, our center of gravity.
- **Backend via overload, not an execution-policy parameter.** It threads an optional
  `ExecutionPolicy` through every signature; we don't (the backend is unimplemented — pure
  free-function kernels *are* the seam, a backend adds overloads). Same goal, lighter mechanism.
- **`Scalar`-only + no promotion, vs broadly-arithmetic + mixed precision.** We trade a
  standard's generality for autodiff composition (`Dual` flows) and a sampler's precision
  stability.
- **Lazy accessors, bounded.** Its `scaled()/transposed()/conjugated()` are lazy mdspan
  accessors; we adopt that *only inside a kernel call* (`mulInto(C, transposed(A), B)` to
  fuse without materializing), never as a value-level lazy node that leaks into `auto`.

**Question #3 — partly resolved:** the view IS `std::mdspan` (no wrapper type; the `Matrix2D`
concept is the only view-layer abstraction — decided). What remains open is the *kernel*
layer: call `std::linalg` (free, battle-tested, parallel) vs write our own kernels over
mdspan. Lean: our own kernels now — libstdc++'s `std::linalg` is still maturing in 2026, and
writing the BLAS-1/2 kernels ourselves is part of the learning mission; `std::linalg` interop
later.

---

## 7. Compute models — the cheap half of Halide

Adopt purity + view-concepts now (the correctness-neutral invariant that makes a backend
swap *possible*); refuse the schedule DSL and autoscheduler (they go stale on refactor
and are brittle). The seam is a thin executor placed **only where divergence will
happen** — gemm / reduce / elementwise — not a general framework. A parallel-CPU executor
is a drop-in (partition the output; kernels don't change). A lazy/GPU backend later is an
interpreter over the same view-based primitives, chosen explicitly, never the default.

**Purity holds the door open; you don't build the door now.**

---

## 8. Honest tradeoffs

- **Closed-world, not Julia's open-world.** A stranger can't make an already-compiled
  algorithm work with their type without recompiling against the headers. We trade
  runtime open-extensibility (and its JIT/invalidation/silent-error costs) for
  compile-time-checked composition. Right trade for a sampler; a real loss of magic.
- **Concepts catch interface violations, not numerical ones.** A type that satisfies
  `Scalar` but has a wrong derivative rule, or a "covariance" that isn't SPD, still
  gives a wrong-but-type-correct answer. Contracts shrink the silent-error surface; tests
  and asserts still own numerical correctness.
- **Monomorphization means build-time cost and code bloat** — the inverse of Julia's
  first-use latency, paid at compile instead of run. Reflection + heavy constexpr can
  make it worse before tooling catches up.
- **Eager forgoes automatic cross-op fusion** Eigen gets free. We bet the hot path is
  small dense matrices where it barely matters, and recover fusion later via the executor
  seam — a measurable cost we're choosing to defer for large elementwise chains.
- **Views reintroduce a sliver of dangling risk** (a view can outlive its owner). Value
  semantics at the boundary contains it, but the kernel currency is borrowed.

---

## 9. Open questions

1. **Reverse-mode autodiff without a mutable tape** — *the* unresolved question, and it
   gates real Bayesian models (forward-mode `Dual` is O(inputs); high-dim posteriors need
   reverse). Can a value-semantic, pure VJP (closure / compile-time tape) ride the same
   per-primitive rules, or does reverse mode inherently need a side-effecting tape that
   breaks P2? Not yet designed.
2. **Where exactly to draw the executor seam** — thin enough not to pollute kernels,
   real enough that a backend slots in without a rewrite. The right cut isn't knowable
   until 2–3 real backends would exist.
3. **`std::mdspan` directly, or a tempura view type?** Interop + battle-tested layout vs
   carrying the `Scalar`/differentiability contract and better diagnostics.
4. **How much should reflection auto-*generate* vs merely auto-*check*?** Emitting concept
   diagnostics is upside; auto-generating contracts risks over-broad requirements nobody
   reads.
5. **Batch-axis representation for `vmap`** — the naive loop ships now, but the eventual
   batching interpreter needs a batch axis in the view model; deferring has foreclosure
   risk.

---

## 10. Implementation plan

Each phase is build-verified and lands as a small reviewed commit (or a few). The
through-line is the **integration test in Phase 6**, which proves the whole stack
composes; earlier phases exist to make it possible.

### Phase 0 — Vocabulary  *(foundation)*
- [✓] `scalar.h` — `Scalar` concept (done; ordered, opt-in membership).
- [ ] `matrix3/matrix_concept.h` — the single `Matrix2D` contract. Replaces the two rival
  trait systems. *This is the seam; nothing else proceeds cleanly without it.*

### Phase 1 — The container
- [ ] `mdarray.h` — the full `std::mdarray` reimplementation; `Matrix<T>` (heap, runtime —
  happy path) and `InlineMatrix<T,R,C>` (stack, static, constexpr — NUTS hot path) as 2D
  aliases. The view is `std::mdspan` (`to_mdspan()`), no wrapper; the `Matrix2D` concept is
  the algorithm currency. Deep-copy value semantics, multidim `[]`, bounds asserts.
- [ ] Tests: construction, access, copy/move independence, static-shape compile checks.

### Phase 2 — Eager math
- [ ] `kernels.h` — pure free-function kernels over views: `addInto`, `mulInto`, `dot`,
  `axpy` (one loop each, no duplication across storage kinds).
- [ ] `arithmetic.h` — operators as thin sugar; **constrain on `Scalar`, not
  `is_arithmetic_v`** (fixes the live composability bug); force-same precision;
  lift static shape-compat (`K==K2`) to a compile-time constraint in `operator*`.

### Phase 3 — Autodiff substrate
- [ ] `dual.h` — `Dual<T>` as a `Scalar`; one JVP rule per math primitive
  (`sin/cos/exp/log/sqrt/lgamma/…`).
- [ ] Test the thesis end-to-end-in-miniature: a generic kernel differentiates when fed
  `Dual` (compare against finite differences).

### Phase 4 — The load-bearing decomposition
- [ ] `decompositions/cholesky.h` + triangular `solve` — generic over a view, deriving
  `ValueType` from the element. **Prove it differentiates when fed `Dual`** (the MVN
  log-density / mass-matrix factor a sampler needs).

### Phase 5 — Transforms + RNG
- [ ] `transforms.h` — `grad(f)` (forward-mode), `vmap(f)` (naive eager loop). Signatures
  that compose; implementations upgrade later.
- [ ] `rng.h` — counter-based, splittable, value-semantic RNG (reproducible & parallel
  chains; a precondition for future `vmap`-over-chains).

### Phase 6 — Integration test  *(the proof)*
- [ ] `bayes/` — a minimal log-density + NUTS skeleton consuming gradients via `Dual`
  through the *unchanged* density. Proves gradient correctness, value semantics, and
  shape-in-type all compose. **If this is clean, the base is sound.**

### Phase 7 — Deferred / research
- [ ] Reverse-mode autodiff without a mutable tape (open question #1) — *before* scaling to
  real high-dim posteriors.
- [ ] Flesh out the executor seam when a second backend (parallel CPU) actually motivates
  it.
- [ ] Migrate/retire the surviving good parts of old `matrix3` onto the new layers in
  place; drop the dead siblings (`matrix`, `matrix2`) once nothing references them.
- [ ] `LinearOperator` concept + a `sparse/` sub-library (CSR/CSC, build → freeze) when a
  sparse or matrix-free need actually appears — unify dense/sparse at the operator, never
  the storage (§3 sparse seam). Delete the bolted-on COO before then.

---

*Decision gates before building:* (1) does the thesis — enforced composability over
`Scalar`, eager, pure — match the intended base? (2) ship forward-mode `Dual` first
(fast path to a working NUTS, reverse-mode deferred), or design reverse-mode-without-a-tape
up front since real models need it?
