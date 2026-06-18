# Matrix3 → Matrix4 Migration Plan (2026-06-16)

Extends `docs/2026-06-15-matrix-design.md` (the cross-library survey + the
matrix4 design it justified). That doc settled *what* the design is; this one
settles *how* matrix3's mature algorithm suite moves onto it, and what the C++26
standard situation forces.

## 0. Standard status — the constraint that fixes the design (2026-06-16)

C++26 was finalized at Croydon (March 2026). What this pins down:

- **In C++26:** `std::mdspan`, `std::submdspan`, and `std::linalg` (P1673 — the
  BLAS interface: free functions over mdspan, destination-passing, and the lazy
  view adaptors `scaled()` / `transposed()` / `conjugated()`).
- **Deferred to C++29:** `std::mdarray` (P1684, the owning analog). So
  `Dense<T, Extents, Container>` is a **permanent** part of tempura, not a
  hold-until-mdarray stopgap. Its shape mirrors mdarray's settled factoring
  (`<T, Extents, Layout, Container>`, with Layout fixed to `layout_right`).

Three consequences for the port:

1. **Slices use `std::submdspan`** — do not hand-roll `submatrix.h` (matrix3 did).
2. **`transposed()` is nearly free** over `layout_right` (swap extents, reuse the
   single stride), so column-major access in matrix3's LayoutLeft algorithms maps
   to a transposed *view*, never a storage choice.
3. **constexpr lives in owning storage**, not mdspan: `InlineMatrix` (array) is
   fully constexpr; `vector`-backed is constexpr under C++20 rules only. The
   compile-time test oracle (§4) therefore prefers `InlineMatrix` fixtures.

*(Sourcing caveat: the 2026-06-16 web research had WebFetch blocked. Confirm
P1673 §10.3 aliasing wording and exact `matrix_product` signatures in P1673R13
before quoting them.)*

## 1. Where things stand

**matrix4 (canonical substrate).** `Dense` owner + view-based free-function
kernels. Implemented: `add` / `multiply` / `multiplyAdd` (+ value/operator/compound
wrappers), `multiply`(matvec) / `dot` / `norm2`, `Permutation` + `permutedRows`
(custom mdspan layout). Just landed (2026-06-16): aliasing assert on
multiply/multiplyAdd (`sharesStorage`), overflow-safe scaled `norm2`,
`static_assert` that fixed-size storage requires all-static extents.

**matrix3 (mature algorithms, wrong substrate).** Carries the payload —
`lu_decomposition`, `cholesky`, `qr`, `svd`, `symmetric_eigen`, `banded`, `block`,
`kronecker_product`, `gauss_jordan`, `transpose`, `submatrix`, `complex`,
`to_string`, plus its own `addition` / `multiplication`. Architecture: owners
*inherit* a hand-rolled `GenericMatrix<Extents, Layout, Accessor>` (a `std::mdspan`
clone), column-major (`LayoutLeft`), intrusive `MatrixTraits`.

The migration is mechanical in shape, careful in numerics: re-express each matrix3
algorithm as a free function over an mdspan view.

## 2. Translation rules (matrix3 idiom → matrix4 idiom)

| matrix3 | matrix4 |
|---|---|
| `class Foo : GenericMatrix<...>` owner | `Dense` alias, or a plain mdspan view — never a new class |
| intrusive `MatrixTraits<T>::kRows` | `extents_type::static_extent(0)` |
| `LayoutLeft` (column-major storage) | `layout_right` owner + `transposed()` view where column traversal is wanted |
| accessor return-category leak (ref vs value) | uniform mdspan reference accessor |
| member algorithm `m.lu()` | free `lu(view)` returning a factor object, destination-passing |
| structure type (`Identity`, `Banded`) | custom **layout** if it only re-indexes (banded offset); custom **accessor** if it mirrors/computes (symmetric, identity) |
| lazy wrapper class (`RowPermuted`) | custom mdspan **layout** (already done: `layout_row_permuted`) |
| `submatrix.h` | `std::submdspan` |

The structure-layout-vs-accessor split is the one subtle rule (from the research):
a `layout_*` mapping can only re-index existing storage, so a symmetric matrix that
stores half and mirrors on read needs a custom *accessor* (or a mapping that folds
`(i,j)↔(j,i)`), not a layout.

## 3. Build order (dependency-ordered)

**Layer A — view vocabulary** (everything downstream uses it):
1. `transposed()` view — unblocks QR / SVD / symmetric eigen.
2. Slicing via `std::submdspan`; delete `matrix3/submatrix.h` rather than port it.
3. `diagonal()` / `row()` / `column()` views.
4. `scaled()` accessor adaptor (std::linalg parity) — keeps α out of kernel signatures.

**Layer B — structure as layouts/accessors:**
5. `Identity` (accessor), `Banded` (layout offset), symmetric/triangular (accessor fold).
6. `block` / `kronecker_product` as view compositions.

**Layer C — decompositions (the payload).** Each is free functions returning a
small factor object that holds the factors (owners and/or views) plus free
`solve()` / `determinant()` / `reconstruct()`:
7. **LU** with partial pivoting (reuse the existing `Permutation` for pivots) → `solve`, `det`, `inverse`. Highest value: unblocks linear solves.
8. **Cholesky** (SPD) → `solve`.
9. **QR** (Householder) → least-squares `solve`.
10. **Symmetric eigen** (Jacobi, or tridiagonalize + QL).
11. **SVD**.

**Layer D — utilities:** `to_string`, complex element support, `gauss_jordan`
(likely *demote* to a teaching example — LU subsumes it for solving).

## 4. Correctness oracle (non-negotiable)

Per the interpolate lesson (a dormant module shipped a *silently wrong*
cubic-spline solve and still compiled green): **a green compile is not
correctness, and a round-trip is not an oracle.** Every ported decomposition is
verified against an independent check before it counts as done:

- **Reconstruction:** ‖A − LU‖, ‖A − QR‖, ‖A − UΣVᵀ‖ within a derived tolerance.
- **Known-answer:** hand-computed small cases, in the style of matrix4's existing
  hand-computed-product tests. Where matrix3 already has trusted tests, port the
  expected **numbers**, not just the test structure.
- **Property:** orthogonality ‖QᵀQ − I‖; eigen residual ‖Av − λv‖; pivot growth.
- **constexpr where possible:** `static_assert` a decomposition of a fixed
  `InlineMatrix` against its known factors — the constexpr-by-default payoff, and
  the strongest possible regression guard.

## 5. Per-file disposition

| matrix3 file | action | matrix4 target |
|---|---|---|
| `matrix.h` (GenericMatrix) | **drop** — replaced by `Dense` | `matrix.h` (done) |
| `extents.h` / `layouts.h` / `accessors.h` | **drop** — use `std::extents`/`std::layout_right`/`std::default_accessor` | — |
| `matrix_traits.h` | **drop** — use `extents_type` | — |
| `addition.h` / `multiplication.h` | **done** | `matrix.h` |
| `permutation.h` / `permuted.h` | **done** (ported) | `permutation.h` / `permuted.h` |
| `transpose.h` | port as **view** | `transposed.h` (Layer A) |
| `submatrix.h` | **drop** — `std::submdspan` | — |
| `lu_decomposition.h` | **port** | `lu.h` |
| `cholesky.h` | **port** | `cholesky.h` |
| `qr.h` | **port** | `qr.h` |
| `svd.h` | **port** | `svd.h` |
| `symmetric_eigen.h` | **port** | `symmetric_eigen.h` |
| `banded.h` | port as **layout** | `banded.h` |
| `block.h` | port as **view composition** | `block.h` |
| `kronecker_product.h` | **port** | `kronecker_product.h` |
| `gauss_jordan.h` | **demote** to example (LU subsumes) | `examples/` |
| `complex.h` | **port** (element-type support) | folds into kernels |
| `to_string.h` | **port** | `to_string.h` |
| `inline_coordinate_list.h` | **evaluate** — sparse, may defer | tbd |

## 6. The de-version endgame

Per the living-monorepo convention (no parallel vN lineages), the *point* of this
migration is to end the five-directory confusion. Once matrix4 reaches parity:
rename `matrix4` → `matrix`, delete `matrix` / `matrix2` / `matrix3`, and migrate
`examples/matrix3_demo.cpp`. The forks exist only until their algorithms have a
verified home on the new substrate.
