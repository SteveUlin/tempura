# Matrix / Linear-Algebra Library Design — Lessons from Prior Art (2026-06-15)

## 1. Purpose & scope

This document distills how a dozen matrix/array libraries — Eigen, Blaze, Armadillo, Boost.uBLAS, ETL, `std::mdspan`/`mdarray`/`linalg`, NumPy, PyTorch, JAX, GLM, Julia, Fortran, MATLAB, R, nalgebra, ndarray — chose to answer the same recurring design questions, and catalogs the regrets that bit their users. It exists to inform tempura's concrete decisions about value-returning `A+B`/`A*B`, the destination-form primitive, and aliasing policy. It surveys neutrally through Section 5 and takes a firm position in Section 6.

## 2. The design axes

Every matrix library is a set of choices along (mostly) orthogonal axes. A reader who holds these apart can reason about any library — and spot where two libraries made the same choice for opposite reasons.

- **Storage location: stack vs heap.** Where the elements live. Stack (`std::array`, Eigen `Matrix4f`, nalgebra `ArrayStorage`) avoids `malloc`, enables register residency and unrolling; heap (`std::vector`, `MatrixXd`, nalgebra `VecStorage`) is unbounded but pays allocation.
- **Dimension binding: static vs dynamic.** Whether each extent is a compile-time constant (in the type) or a runtime value. The cleanest designs (mdspan, nalgebra) make this **per-axis** and **independent of storage**: `Matrix<float, 3, Dynamic>` fixes rows, leaves columns runtime.
- **Operator semantics: what `*` means.** Matrix product (Eigen-Matrix, GLM, Julia, MATLAB, nalgebra) or element-wise/Hadamard (NumPy, ETL, valarray, Fortran, R, ndarray). The single most consequential and most-litigated decision in the field.
- **Return / output type.** What `A op B` produces: a concrete owning value (eager libraries), a lazy proxy holding references (expression-template libraries), or nothing — the result is written into a caller-supplied destination (BLAS, `std::linalg`).
- **Evaluation strategy: eager vs lazy.** Eager allocates and computes immediately (NumPy, nalgebra, ndarray, GLM, Julia surface syntax). Lazy/expression-template defers to assignment to fuse loops and elide temporaries (Eigen, Blaze, Armadillo, ETL, xtensor).
- **Aliasing policy.** What happens when the destination overlaps an input. Forbid-by-contract (BLAS, `std::linalg`, Julia `mul!`), safe-by-default-with-auto-temp (Blaze, uBLAS, NumPy ufuncs), unsafe-by-default-with-opt-in (Eigen non-matmul), or structurally-impossible (JAX immutability, Rust borrow checker).
- **Memory layout.** Row-major (NumPy, PyTorch, ndarray, mdspan default) or column-major (Eigen, Armadillo, nalgebra, Julia, Fortran, MATLAB, R, GLM). Chosen for cache traversal or BLAS/GL interop; a type parameter in the best designs (`layout_right`/`layout_left`).
- **Type promotion.** Whether mixed element types convert implicitly. C++ libraries uniformly refuse (Eigen `static_assert`, explicit `.cast<>`; nalgebra/ndarray single generic `T`); NumPy's value-based casting was a documented regret fixed by NEP 50.
- **Error handling: compile-time vs runtime conformance.** Static dims permit compile-time shape checks (Eigen fixed, nalgebra `Const`); dynamic dims fall back to runtime assert/panic (or, in old Fortran, silent UB).

These axes are the lens for the rest of the document.

## 3. What has bitten people

This is the heart. Each footgun is real, documented, and traceable to one of the axes above.

### 3.1 The `auto` / expression-template trap (the sharpest, most universal regret)

Every expression-template library makes `A*B` return a *proxy holding references*, not a result. Capturing it with `auto` produces two distinct bugs.

**Dangling references.** When an operand is a temporary, the proxy outlives it. Works in Debug, corrupts in Release:

```cpp
auto C = f() * v;   // f() returns Matrix by value → proxy dangles
                    // Release (-O3): reuses freed memory → prints -2.60696e-40
```

**Silent recomputation.** The proxy re-evaluates on every use:

```cpp
auto C = A * B;
for (...) w = C * v;   // recomputes A*B every iteration
```

Eigen documents this in capitals: *"do not use the `auto` keyword with Eigen's expressions… unless you are 100% sure."* The fix is to name the concrete type or call `.eval()`. Blaze, Armadillo, and ETL inherit the same trap from the same mechanism. xtensor's variant is subtler and therefore worse: element-wise `auto` is usually fine, but **reducers** (`auto s = xt::sum(x, {1})`) dangle and *"may produce incorrect results or crash, especially in optimized builds."*

The committee cited this explicitly as a reason to omit expression templates from `std::linalg`: *"C++ auto type deduction makes it easy for users to capture expressions before the expression templates system has the chance to evaluate them."* Eager libraries (NumPy, nalgebra, ndarray, GLM, Julia surface syntax) have **no** auto trap by construction — `auto C = A*B` is always a concrete value.

### 3.2 Aliasing: silent wrong results on `M = M.op()`

Aliasing — the same matrix on both sides of `=` — is where libraries diverge most sharply, and where the unsafe defaults bite hardest.

```cpp
a = a.transpose();   // Eigen: WRONG → [[1,2],[2,4]] instead of [[1,3],[2,4]]
                     // lazy eval overwrites cells it still needs
mat.bottomRightCorner(2,2) = mat.topLeftCorner(2,2);  // overlapping read/write corruption
```

Eigen's rule is a cognitive minefield: coefficient-wise ops assume *no* aliasing (safe, since `(i,j)` depends only on `(i,j)`); **matrix product is the one op that assumes aliasing by default** and silently inserts a temporary, so `A = A*A` is correct but costs a hidden temp; `noalias()` opts *out* of that temp and is *wrong* if aliasing actually exists. Fixes are scattered across `transposeInPlace`, `adjointInPlace`, `.eval()`.

Julia's `mul!(C, A, B)` historically had the same bug: `A_mul_B!(B, A, B)` silently *zeroed* `B` for dims > 3 — and worse, a doc example implied it was safe. NumPy solves overlap at runtime with a heuristic copy (since 1.13 ufuncs behave as if no overlap), but detection is approximate — it can both over-copy and historically miss cases.

The opposite-default camp: Blaze and uBLAS are **safe-by-default** (auto-temp via runtime `canAlias`/`isAliased`), with `noalias()` as the speed opt-out. uBLAS *invented* the `noalias` idiom; Eigen borrowed the name but flipped the default. `std::linalg` and BLAS **forbid** aliasing by contract — UB, no silent patch. JAX (immutable) and Rust (borrow checker) make the entire bug class structurally impossible.

### 3.3 `*` meaning matmul vs Hadamard, and the `np.matrix` disaster

One symbol, two operations both wanting infix syntax. PEP 465 is the canonical analysis: *"there are two important operations which compete for use of Python's `*` operator."* The worst answer was **minting a second type** that redefines `*`:

- **`np.matrix`** made `*` mean matmul, forced everything 2-D, and is now pending-deprecation. It is an ill-behaved `ndarray` subclass that breaks duck typing (`A[i]` returns a matrix not a row) and fragmented the ecosystem: `matrix`, `ndarray`, and `scipy.sparse` each defined `*` differently, so *"code that expects an `ndarray` and gets a `matrix`… may crash or return incorrect results,"* and generic code *"founders into a swamp of `isinstance`."*
- **Eigen's two-type scheme** (`Matrix` `*`=matmul vs `Array` `*`=Hadamard) is the same mistake in C++: declaring `Matrix` and writing `a * b` silently does matmul, never Hadamard. Fix: `a.cwiseProduct(b)` or `(a.array()*b.array()).matrix()`.

The clean answers all use **one type, two spellings**: Blaze/Armadillo `%` for Hadamard; NumPy/PyTorch `@` for matmul (left-associative, `@@` deliberately rejected as restraint); nalgebra `component_mul()`; GLM `matrixCompMult()`; Fortran/R/ETL name matmul (`matmul`, `%*%`, `mmul`) and keep `*` element-wise. The lesson, stated by both PEP 465 and the Rust crate authors: **pick one meaning for `*`, name the other, and never let a second type redefine the operator.**

### 3.4 valarray: the standard's cautionary tale

`std::valarray` (C++98) was *specified assuming* implementers would use expression templates but **never mandated it**. Early implementations were naive and slow, users lost trust, the ecosystem left. Its slice proxies (`slice_array`, `gslice_array`) are a proto-auto-trap: `auto x = v[slice]` captures a proxy, not data. It is 1-D only, no shape conformance, and picked element-wise `*` — the exact ambiguity P1673 later cited to ban operators. The lesson: **an optional optimization that the spec relies on but does not require will not be delivered uniformly, and the abstraction dies.**

### 3.5 Slice copy-vs-view surprises

Whether a slice aliases the parent or copies silently changes results, and no two libraries agree:

- **NumPy:** basic slicing → view; advanced (fancy/boolean) indexing → copy; `reshape` → view-if-possible-else-copy. `y = x[1:3]; x[1:3] = [10,11]` mutates `y`. The docs call the distinction *"crucial for debugging"* (`arr.base is None` ⇒ copy).
- **Julia:** the asymmetry that bites — `x[1:10]` on the **RHS copies**, but `x[1:10] .= v` on the **LHS mutates the parent**. Opt into views with `@view`.
- **Fortran:** an assumed-shape slice is a strided view via descriptor, but passing it where contiguous data is needed triggers silent **copy-in/copy-out temporaries** — hidden O(n) cost and *"a data race that has not existed in the original program."*

### 3.6 Stack overflow from large fixed sizes

Coupling "static dimensions" to "stack storage" means a large fixed-size matrix silently blows the stack:

- **Eigen:** *"a very large matrix using fixed sizes inside a function could result in a stack overflow."* Guarded by `EIGEN_STACK_ALLOCATION_LIMIT` (default 128 KB; compile-time assert for declared types, silent heap fallback for internal temporaries).
- **nalgebra:** `SMatrix<f32,1000,1000>` is 4 MB on the stack with **no automatic spill** — the user must switch to `DMatrix`.
- **Julia StaticArrays:** unrolls per size; *"consider a normal `Array` for arrays larger than 100 elements"* — past ~11×11–14×14 compile time blows up and runtime degrades below `Array`.
- **The decisive case for tempura:** the *result* shape is not bounded by operand storage. An outer product of two small stack vectors can be an 8 MB matrix. So result storage cannot be inferred from operand storage without risking overflow.

### 3.7 Type-promotion and integer-overflow surprises

- **NumPy's old value-based casting** (a documented regret): the *value* of a scalar picked the result dtype, and 0-D arrays behaved unlike N-D. `np.uint8(1)+2 → int64`. NEP 50 fixed it with weak scalar typing (Python scalars adopt the array dtype; values never inspected).
- **Silent integer wraparound** is a live footgun in NumPy (arrays wrap with no warning: `np.uint8(0)-np.uint8(1) → 255`) and a *deliberate* speed choice in Julia (`2^64 == 0`) that the community repeatedly relitigates. C++ libraries with a single generic `T` (Eigen, nalgebra, ndarray) sidestep promotion entirely but inherit C++ integer wrap semantics.

### 3.8 Broadcasting silent rank promotion

NumPy broadcasts `(3,)` against `(3,1)` to `(3,3)` instead of erroring — an accidentally-transposed vector yields a plausible-but-wrong matrix. ndarray's footgun is asymmetric: **only the right-hand operand broadcasts**, so `&small + &big` does not behave as NumPy users expect. Eigen and nalgebra avoid the class by requiring explicit `colwise()`/`rowwise()` broadcasting.

### 3.9 In-place `+=` that does not do what it says

Julia's `B += x` **rebinds** the name (allocates), it does not mutate; genuine in-place needs `B .+= x` (lowers to `broadcast!`). JAX `+=` rebinds too (no `__iadd__`). MATLAB's in-place optimization (`A = f(A)`) is **silently not applied** in scripts, `eval`, `try/catch`, or with indexed assignment — performance collapses to full copies with no diagnostic.

### 3.10 Alignment and pass-by-value (Eigen-specific)

Fixed-size vectorizable members need `EIGEN_MAKE_ALIGNED_OPERATOR_NEW`; passing Eigen objects by value breaks alignment (`std::bind`/functors storing by value bite here). Misalignment is a controlled-crash assertion. Largely mitigated by C++17 aligned `new`, but a long-standing source of confusion.

## 4. What is considered good design

Each principle below is paired with the evidence that earns it.

- **Eager evaluation unless fusion is a measured need.** Both Rust crates deliberately reject expression templates; NumPy, Julia (surface), GLM, nalgebra are eager. The auto trap (§3.1) and aliasing minefield (§3.2) are *consequences* of laziness. Eager costs temporaries — recovered by an explicit destination API, not by reintroducing proxies.

- **One type, two operator spellings — never two types.** PEP 465 and the `np.matrix` deprecation prove that redefining `*` on a second type fragments the ecosystem. Blaze (`*`/`%`), NumPy (`*`/`@`), nalgebra (`*`/`component_mul`) all keep one type.

- **Decouple dimension-staticness from storage, and make dimensions per-axis.** nalgebra's `Matrix<T,R,C,S>` (dimension as `Const`/`Dyn`, storage as an orthogonal trait) and mdspan's per-axis `extents` are the model. This yields all four static/dynamic combinations from one type.

- **Result storage belongs to the caller, not to operand inference.** The outer-product-overflow case (§3.6) shows operand storage cannot determine result storage safely. `std::mdarray` ties storage to *result extents* (all-static → `array`/stack; any dynamic → `vector`/heap), and a destination API lets the caller own the buffer outright.

- **Destination / gemm API as the explicit primitive.** `std::linalg`, BLAS, NumPy `out=`, nalgebra `mul_to`/`gemm`, Julia `mul!` all write into a caller-supplied output. P1673 splits `C := αAB + βC` into a **write-only ("overwriting", β=0) and a read-and-write ("updating", β≠0) overload** rather than a runtime flag — intent explicit, different optimizations unlocked. Triangular ops additionally ship in-place and not-in-place overloads.

- **Aliasing: forbid by contract and assert, do not silently patch.** P1673/BLAS forbid input/output aliasing outright; Julia's modern `mul!` *detects and throws*. This matches fail-loudly far better than Eigen's silent-temp-for-matmul-only rule or NumPy's heuristic copy that can miss cases.

- **No implicit numeric promotion.** Every C++ and Rust library refuses it (Eigen `static_assert` + `.cast<>`, single generic `T`). NEP 50 is the standard *removing* value-based promotion after years of pain.

- **Compile-time shape conformance where dimensions are static — but guard the bounds against rot.** nalgebra's static `Const` dims fail nonconformant multiply at compile time; the cautionary tale is its pre-const-generics `typenum` era that *"infected the public API."* C++26 concepts/reflection keep the constraint intent readable.

- **Trap loudly on bad input.** NumPy's silent integer array wraparound and MATLAB's silent auto-grow are the anti-patterns; assert/throw is the principle.

- **Layout is a type parameter; pick the default for your dominant consumer.** mdspan's `layout_right`/`layout_left`/`layout_stride` express interop in the type, not by copying. Column-major if BLAS is the consumer; row-major for C/cache traversal.

- **`scaled`/`transposed`/`conjugated` as explicit inline views, not stored proxies.** P1673 borrows *one* aspect of expression templates — modifying the accessor — but only as named arguments passed inline (`matrix_vector_product(scaled(2.0,A), x, y)`), so they cannot be stored and dangled.

## 5. Comparison table

| Library | `*` means | Eval | auto trap | Aliasing policy | Storage × dims | Layout | Promotion | Conformance |
|---|---|---|---|---|---|---|---|---|
| Eigen | matmul (Matrix) / Hadamard (Array) | lazy ET | **severe** | unsafe non-matmul; matmul auto-temp; `noalias` opt-out | fixed→stack / Dynamic→heap, per-axis | col-major | explicit `.cast<>` | compile (fixed) / runtime |
| Blaze | matmul; `%` Hadamard | lazy ET | yes | **safe** auto-detect; `noalias` opt-out | Static/Dynamic/**Hybrid** | configurable | explicit | compile/runtime |
| Armadillo | matmul; `%` Hadamard | lazy ET | yes | delayed-eval, mostly safe | heap / `::fixed` | col-major | explicit `conv_to` | runtime |
| uBLAS | `prod()`; `element_prod` | lazy ET | yes | `=` temps (safe); `noalias` opt-out | dynamic heap | configurable | — | runtime |
| ETL | **Hadamard**; `mmul()` | lazy ET | yes | not prominent | `fast_`/`dyn_` | — | — | compile/runtime |
| mdspan/mdarray | none | n/a | none | n/a | view / array∨vector by extents, per-axis | right/left/stride | — | compile/runtime |
| std::linalg | none (free fns) | eager | none | **forbidden by contract**; overwrite/update overloads | views (mdspan) | both | — | runtime |
| valarray | **Hadamard** | unspecified→naive | proxy footgun | unspecified | heap 1-D | n/a | — | none |
| NumPy | **Hadamard**; `@` matmul | eager | none | runtime heuristic copy | heap, dynamic | row-major | NEP 50 weak | runtime |
| PyTorch | **Hadamard**; `@` | eager | none | `_`-suffix mutation; version-counter guard | heap, dynamic | row-major | NumPy-style | runtime |
| JAX | **Hadamard**; `@` | eager (XLA) | none | **immutable** (no aliasing) | heap, dynamic | row-major | weak (f32 default) | runtime |
| GLM | matmul; `matrixCompMult` | eager | none | safe (eager copy) | small fixed, stack only | col-major | per-component | compile |
| Julia | matmul; `.*` Hadamard | eager (surface) | none | `mul!` throws; `A=A*B` safe | heap / StaticArrays stack | col-major | silent int wrap | runtime |
| nalgebra | matmul; `component_mul` | eager | none | `*` fresh; `mul_to`/`gemm` caller-distinct | `Const`/`Dyn` × Array/Vec/View | col-major | single `T` | **compile** (Const) / runtime |
| ndarray | **Hadamard**; `.dot()` | eager | none | borrow checker forbids | static rank, runtime shape | row-major | single `T` | runtime |

## 6. Recommendations for tempura

tempura's stated values — constexpr-by-default, fail-loudly (assert, no silent fallbacks), no magic constants, terse, correctness over micro-optimization — point at one coherent design. The prior art makes the choices nearly forced.

### 6.1 Output type of value-form `A+B` and `A*B`

**Element type:** single generic `T`, no implicit promotion. Mixed-type operands are a `static_assert`, matching every serious C++/Rust library (§4) and avoiding NumPy's value-based-casting regret. If tempura ever wants `f32×f64`, require an explicit cast view — never silent.

**Extents:** derived from the operation, per-axis, preserving staticness where the math determines it. `A(M,K) * B(K,N) → (M,N)`: if `M` and `N` are static in the operands, the result extents are static; if either is `Dynamic`, the result axis is `Dynamic`. This is exactly mdspan/nalgebra per-axis composition and gives compile-time conformance for static cases (inner-dim mismatch is a `static_assert`), runtime `assert` otherwise. This is fully constexpr-evaluable.

**Storage — the load-bearing decision: heap-by-default for value forms, with NO stack-size threshold.** The §3.6 outer-product case proves storage *cannot* be inferred from operand storage: two small `InlineMatrix` operands can produce an 8 MB result. So value-form results return `Matrix<T, Rows, Cols>` (heap-backed `std::vector`), *regardless of operand storage*.

I reject the Eigen-style stack-size threshold explicitly, because it violates two tempura values:

- **No magic constants.** A `128 KB` / `EIGEN_STACK_ALLOCATION_LIMIT`-style cutoff is precisely the magic constant the codebase forbids. Worse, Eigen's threshold has a *silent* fallback to heap for temporaries — a silent fallback, which fail-loudly prohibits.
- **Correctness over micro-optimization.** Stack-returning value forms exist only to save an allocation. The auto-temp-vs-stack win is a micro-optimization; the downside is stack overflow, a correctness failure. The trade is upside-down for a learning library.

The stack path is not lost — it is *opt-in and caller-owned* via the destination form (§6.2). If the caller knows the result is small and static, they declare an `InlineMatrix` destination and call `multiply(A, B, dst)`. **Storage thus belongs to the caller**, exactly as the research concluded: value forms default to the safe heap; the caller reaches for the stack deliberately, sized by a type they wrote. This mirrors `std::mdarray` (storage follows *result* extents/container choice) and nalgebra (`SMatrix` is a deliberate caller choice with a documented overflow risk, never an inference).

**Evaluation: eager, no expression templates.** This is the single highest-leverage choice. The auto trap (§3.1) and the aliasing minefield (§3.2) are *both* consequences of laziness, and both are silent-wrong-result bugs — anathema to fail-loudly. Eager value forms make `auto C = A*B` a concrete `Matrix`, always correct. The cost (intermediate temporaries in `A+B+C`) is recovered by the destination API, not by proxies. terse + correctness + constexpr all favor eager: an eager `constexpr` loop is trivially compile-time-evaluable; expression-template trees fight constexpr and reflection.

### 6.2 Destination form as the primitive; `+`/`+=`/`*`/`*=` as thin wrappers

**Yes — make the destination form the primitive.** This is the `std::linalg`/BLAS/nalgebra architecture, and it fits tempura's values perfectly:

```cpp
// PRIMITIVES — write into caller-owned destination, fully constexpr
add(A, B, dst);            // dst ← A + B   (overwriting)
multiply(A, B, dst);       // dst ← A · B   (overwriting)
multiplyAdd(A, B, dst);    // dst ← A · B + dst  (updating)
```

Following P1673's strongest idea, **split overwrite from update into separate named functions rather than a runtime `β` flag** (`multiply` vs `multiplyAdd`). No magic flag, intent explicit in the name, each path optimizable independently. This *is* the gemm pattern (`C ← αAB + βC`) decomposed into honest pieces, and it is where the stack-allocated fast path lives: the caller supplies an `InlineMatrix` `dst` when they know it is safe.

Layer the ergonomic surface on top as one-liners:

```cpp
auto C = A + B;            // value form: allocate heap result, add(A,B,C), return
A += B;                    // compound: add(A, B, A)  — see aliasing rule below
auto C = A * B;            // value form: allocate heap result, multiply(A,B,C), return
A *= scalar;               // scalar scale, element-wise, always alias-safe
```

This inverts the usual "operators are primitive, gemm is the optimization" framing. Here the **destination form is the truth**; operators are sugar that allocate-then-delegate. One implementation of the math, three façades. Terse, DRY for the *stable* abstraction (the kernel), and the kernel is the thing tested and proven `constexpr`.

**Operator semantics:** `*` = matrix product (the nalgebra/Julia/GLM/Eigen-Matrix choice). Hadamard gets a distinct name — `hadamard(A, B, dst)` / `componentMul` — never an operator, never a second type. §3.3 is unambiguous: the `np.matrix` and Eigen-two-type regrets both stem from letting `*` mean different things on different types. One `Matrix` type, `*` = matmul, element-wise named.

### 6.3 Aliasing policy for the destination multiply

**Forbid aliasing by contract and `assert` on it — do not auto-temp.** This is BLAS/`std::linalg`/Julia-`mul!`, and it is the only policy consistent with fail-loudly:

```cpp
// multiply(A, B, dst): PRECONDITION — dst aliases neither A nor B.
// Checked, not silently worked around.
assert(!overlaps(dst, A) && !overlaps(dst, B));
```

The reasoning, grounded in the prior art:

- **Auto-temp is a silent fallback.** Blaze/uBLAS insert a hidden temporary when they detect overlap. That is convenient but it is *exactly* the silent behavior tempura forbids — the user never learns their call pattern allocates. fail-loudly says: tell them.
- **`noalias()`-style opt-out is a magic incantation that is UB if wrong.** Eigen's `noalias()` is a promise the compiler cannot check; getting it wrong is silent corruption. No magic constants extends to no magic promises.
- **The assert is correct *and* cheap to express, and it teaches.** `overlaps()` is a constexpr pointer/extent comparison. When it fires, the user learns aliasing is illegal here and writes `auto tmp = A*B; ...` themselves — making the temporary *visible and intentional*, the whole point of the value form existing alongside the destination form.

For **compound assignment** the aliasing is *intrinsic*, so handle it honestly rather than forbidding it:

- `A += B`, `A -= B`, `A *= scalar` are **element-wise**: `(i,j)` depends only on `(i,j)`, so `dst == A` is safe. No temp, no assert needed (document the safety).
- `A *= B` (matrix product, `A ← A·B`) **necessarily aliases** — `A` is both input and output. Here the value form is the honest primitive: implement `A *= B` as `A = A * B` (allocate a fresh heap result, then move-assign). This makes the unavoidable temporary explicit in the lowering, never hidden. The destination `multiply` keeps its forbid-and-assert contract; the operator that *must* alias routes through the allocating value path.

This gives a clean, teachable rule: **element-wise compound ops alias safely; the destination matrix-multiply forbids aliasing and asserts; the matrix-multiply compound operator pays for an explicit temporary because it has no choice.** No silent temps, no UB opt-outs, no magic thresholds — every allocation is either caller-chosen or a visible consequence of an operation that mathematically requires it.

## 7. Sources

- PEP 465 (`@` / `*` rationale, `np.matrix` regret): https://peps.python.org/pep-0465/
- NEP 50 (scalar promotion): https://numpy.org/neps/nep-0050-scalar-promotion.html
- P0009R9 (mdspan): https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2019/p0009r9.html
- P1684R5 (mdarray): https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2023/p1684r5.html
- P3308R0 (mdarray Q&A): https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2024/p3308r0.html
- P1673R8 (linalg rationale): https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2022/p1673r8.html
- P1673R13 (latest linalg): https://isocpp.org/files/papers/P1673R13.html
- cplusplus/papers #557 (linalg): https://github.com/cplusplus/papers/issues/557
- Eigen aliasing: https://libeigen.gitlab.io/eigen/docs-nightly/group__TopicAliasing.html
- Eigen lazy evaluation and aliasing: https://libeigen.gitlab.io/eigen/docs-nightly/TopicLazyEvaluation.html
- Eigen common pitfalls (auto, alignment): https://libeigen.gitlab.io/eigen/docs-nightly/TopicPitfalls.html
- Eigen auto-trap blog (Pavel): https://www.pavelp.cz/posts/eng-cpp-eigen-trap/
- Eigen Matrix class / stack limit: https://libeigen.gitlab.io/eigen/docs-nightly/group__TutorialMatrixClass.html
- Eigen preprocessor directives: https://eigen.tuxfamily.org/dox/TopicPreprocessorDirectives.html
- Eigen Array vs Matrix: https://libeigen.gitlab.io/eigen/docs-nightly/group__TutorialArrayClass.html
- Eigen storage orders: https://libeigen.gitlab.io/eigen/docs-nightly/group__TopicStorageOrders.html
- Eigen broadcasting: https://libeigen.gitlab.io/eigen/docs-nightly/group__TutorialReductionsVisitorsBroadcasting.html
- Eigen mixed types (#279): https://gitlab.com/libeigen/eigen/-/issues/279
- Eigen NoAlias: https://eigen.tuxfamily.org/dox/classEigen_1_1NoAlias.html
- Blaze README: https://github.com/parsa/blaze/blob/master/README.md
- Blaze Tutorial: https://github.com/BioDataAnalysis/blaze/blob/master/blaze/Tutorial.h
- Blaze design paper (Iglberger et al., SISC 2012): https://blogs.fau.de/hager/files/2012/05/ET-SISC-Iglberger2012.pdf
- Armadillo docs: https://arma.sourceforge.net/docs.html
- Armadillo JOSS 2016: https://arma.sourceforge.net/armadillo_joss_2016.pdf
- Armadillo (arXiv 2502.03000): https://arxiv.org/html/2502.03000
- uBLAS operations overview: https://www.boost.org/doc/libs/1_53_0/libs/numeric/ublas/doc/operations_overview.htm
- Eigen vs uBLAS (LWN): https://lwn.net/Articles/319838/
- Iglberger et al. (arXiv:1104.1729): https://arxiv.org/pdf/1104.1729
- ETL repo: https://github.com/wichtounet/etl
- ETL 1.0 post: https://baptiste-wicht.com/posts/2016/09/expression-templates-library-etl-10.html
- ETL 1.2 GPU: https://baptiste-wicht.com/posts/2017/10/expression-templates-library-etl-1-2-complete-gpu-support.html
- Cross-lib benchmark (Poya): https://romanpoya.medium.com/a-look-at-the-performance-of-expression-templates-in-c-eigen-vs-blaze-vs-fastor-vs-armadillo-vs-2474ed38d982
- Expression templates / valarray history (Wikipedia): https://en.wikipedia.org/wiki/Expression_templates
- cppreference `<valarray>`: https://en.cppreference.com/cpp/header/valarray
- Veldhuizen, Expression Templates (1995): https://www.cs.rpi.edu/~musser/design/blitz/exprtmpl.html
- mdspan gentle introduction (Kokkos wiki): https://github.com/kokkos/mdspan/wiki/A-Gentle-Introduction-to-mdspan
- kokkos/mdspan reference impl: https://github.com/kokkos/mdspan
- xtensor pitfalls: https://xtensor.readthedocs.io/en/latest/pitfall.html
- xtensor containers: https://xtensor.readthedocs.io/en/latest/container.html
- xtensor expressions/lazy eval: https://xtensor.readthedocs.io/en/latest/expression.html
- xtensor-blas reference: https://xtensor-blas.readthedocs.io/en/stable/reference.html
- GLM manual: https://github.com/g-truc/glm/blob/master/manual.md
- GLM repo: https://github.com/g-truc/glm
- GLM matrixCompMult API: https://glm.g-truc.net/0.9.4/api/a00133.html
- NumPy copies and views: https://numpy.org/doc/stable/user/basics.copies.html
- SciPy cookbook views vs copies: https://scipy-cookbook.readthedocs.io/items/ViewsVsCopies.html
- NumPy ufuncs / overlap: https://numpy.org/doc/stable/reference/ufuncs.html
- NumPy overlap PR #8043: https://github.com/numpy/numpy/pull/8043
- NumPy internals (memory order): https://numpy.org/doc/1.21/reference/internals.html
- NumPy matmul (`out=`): https://numpy.org/doc/stable/reference/generated/numpy.matmul.html
- NumPy matmul out issue #18669: https://github.com/numpy/numpy/issues/18669
- np.matrix deprecation (scikit-learn #12327): https://github.com/scikit-learn/scikit-learn/issues/12327
- NumPy deprecations: https://numpy.org/devdocs/release/notes-towncrier.html
- Integer overflow #10782: https://github.com/numpy/numpy/issues/10782
- Integer overflow #8987: https://github.com/numpy/numpy/issues/8987
- "The Silence of the Integers": https://tttthomasssss.github.io/silent-integer-overflow.html
- NumPy broadcasting silent wrong answers: https://simplico.net/2026/04/18/numpy-broadcasting-rules-why-3-and-31-behave-differently-and-when-it-silently-gives-wrong-answers/
- JAX rank-promotion warning: https://github.com/jakevdp/jax/blob/concrete/docs/rank_promotion_warning.rst
- Modular row- vs column-major: https://www.modular.com/blog/row-major-vs-column-major-matrices-a-performance-analysis-in-mojo-and-numpy
- PyTorch tensor docs (`_` suffix): https://docs.pytorch.org/docs/2.12/tensors.html
- PyTorch autograd mechanics: https://docs.pytorch.org/docs/0.3.0/notes/autograd.html
- PyTorch avoid inplace: https://lernapparat.de/pytorch-inplace
- PyTorch named tensors: https://docs.pytorch.org/docs/stable/named_tensor.html
- PyTorch named tensors status: https://discuss.pytorch.org/t/status-of-named-tensors/172278
- Reviving named tensors: https://dev-discuss.pytorch.org/t/reviving-named-tensors-strategy-for-addressing-the-experimental-backlog/3289
- PyTorch shape errors explained: https://heytensor.com/blog/pytorch-shape-errors-explained.html
- Debugging shape mismatches: https://apxml.com/courses/getting-started-with-pytorch/chapter-8-monitoring-debugging-models/debugging-shape-mismatches
- Tensor Considered Harmful (Rush): http://nlp.seas.harvard.edu/NamedTensor
- JAX sharp bits: https://docs.jax.dev/en/latest/notebooks/Common_Gotchas_in_JAX.html
- JAX immutability #2219: https://github.com/jax-ml/jax/issues/2219
- TensorFlow variables vs constants: https://www.w3reference.com/blog/tensorflow-variables-and-constants/
- jaxtyping: https://docs.kidger.site/jaxtyping/
- Julia arrays manual: https://docs.julialang.org/en/v1/manual/arrays/
- Julia noteworthy differences: https://docs.julialang.org/en/v1/manual/noteworthy-differences/
- Julia More Dots (fusion): https://julialang.org/blog/2017/01/moredots/
- Julia extensible broadcast fusion: https://julialang.org/blog/2018/05/extensible-broadcast-fusion/
- Julia broadcast internals: https://github.com/JuliaLang/julia/blob/master/base/broadcast.jl
- Julia broadcasting (Barton): https://cityinthesky.co.uk/posts/2019/working-with-broadcasting-in-julia/
- Julia `mul!` aliasing bug (#13488): https://github.com/JuliaLang/julia/issues/13488
- Julia LinearAlgebra: https://docs.julialang.org/en/v1/stdlib/LinearAlgebra/
- Julia views/slices (bkamins): https://bkamins.github.io/julialang/2022/06/10/view.html
- Julia slices discourse: https://discourse.julialang.org/t/slices-should-they-default-to-views/83023
- StaticArrays README: https://github.com/JuliaArrays/StaticArrays.jl/blob/master/README.md
- StaticArrays thread: https://www.juliapackages.com/p/staticarrays
- Julia integer overflow discourse: https://discourse.julialang.org/t/discussion-about-integer-overflow/69627
- Julia integers/floats manual: https://docs.julialang.org/en/v1/manual/integers-and-floating-point-numbers/
- Julia integer overflow #50486: https://github.com/JuliaLang/julia/issues/50486
- Julia performance tips: https://docs.julialang.org/en/v1/manual/performance-tips/
- Fortran stack/heap segfault: https://jblevins.org/log/segfault
- Flang array repacking (copy-in/out): https://flang.llvm.org/docs/ArrayRepacking.html
- MATLAB avoid unnecessary copies: https://www.mathworks.com/help/matlab/matlab_prog/avoid-unnecessary-copies-of-data.html
- Undocumented MATLAB memory optimizations: https://undocumentedmatlab.com/articles/internal-matlab-memory-optimizations
- MATLAB array indexing (auto-grow): https://www.mathworks.com/help/matlab/learn_matlab/array-indexing.html
- MATLAB in-place answers: https://www.mathworks.com/matlabcentral/answers/359410
- R names and values (copy-on-modify): https://adv-r.hadley.nz/names-values.html
- R preallocate (O(n²) growth): https://library.virginia.edu/data/articles/why-preallocate-memory-r-loops
- nalgebra const-generics integration (dimforge): https://www.dimforge.com/blog/2021/04/12/integrating-const-generics-to-nalgebra/
- nalgebra const-generics status quo: https://rust-lang.github.io/project-const-generics/vision/status_quo/nalgebra.html
- nalgebra SMatrix: https://docs.rs/nalgebra/latest/nalgebra/base/type.SMatrix.html
- nalgebra Matrix (gemm/mul_to/component_mul): https://docs.rs/nalgebra/latest/nalgebra/base/struct.Matrix.html
- nalgebra user guide: https://www.nalgebra.rs/docs/user_guide/vectors_and_matrices/
- ndarray ArrayBase: https://docs.rs/ndarray/latest/ndarray/struct.ArrayBase.html
- ndarray for NumPy users: https://docs.rs/ndarray/latest/ndarray/doc/ndarray_for_numpy_users/index.html
- ndarray co-broadcasting PR #898: https://github.com/rust-ndarray/ndarray/pull/898
- ndarray layout issue #1172: https://github.com/rust-ndarray/ndarray/issues/1172
- Rust "operator overloading doesn't scale": https://internals.rust-lang.org/t/rusts-operator-overloading-doesnt-scale/408
- Rust `@`-for-matmul proposal (rejected): https://internals.rust-lang.org/t/add-operator-for-matrix-multiplication/16026
- riptutorial: auto and expression templates: https://riptutorial.com/cplusplus/example/14495/auto-and-expression-templates
- Amy Tabb (Eigen column-major TIL): https://amytabb.com/til/2021/11/23/remembering-eigen-column-major/
