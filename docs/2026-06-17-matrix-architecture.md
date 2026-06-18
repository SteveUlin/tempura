# Matrix Library Architecture (2026-06-17)

The grounded reference for tempura's linear-algebra design. Supersedes the owner
design in `src/matrix4/` (`Dense` → `MdArray`). Extends the cross-library survey
(`2026-06-15-matrix-design.md`) and the migration plan (`2026-06-16-matrix-migration.md`).

Grounded in `std::mdspan` (P0009, C++23/26) and `std::linalg` (P1673, C++26); the
owner mirrors `std::mdarray` (P1684, deferred to C++29 — so we own it ourselves).

## 1. The organizing principle: a matrix is four orthogonal choices

`std::mdspan` exists because "a matrix" decomposes into separable concerns. There
are four axes, they are orthogonal, and every feature lives on exactly one:

| Axis | Question | Choices | Provided by |
|---|---|---|---|
| **Extents** | what shape? | static / dynamic / mixed | `std::extents` |
| **Layout** | logical `(i,j)` → which offset? | row-major / transposed / triangular-packed / banded / permuted | `std::mdspan` layouts + ours |
| **Accessor** | offset → element? | plain / scaled / triangle-zeroing / symmetric-fold | `std::mdspan` accessors + ours |
| **Storage** | where do the bytes live? | `std::array` (inline) / `std::vector` (heap) | **our owner** |

mdspan owns the first three (for non-owning views). The owner supplies the fourth
and hands out an mdspan for the rest.

**The rule that places any feature** (the grounding you can apply without judgment):

- Changes *the bytes* → **storage / owner** axis (inline, heap, packed, sparse).
- Changes only *how the bytes are read* → **layout / accessor view** (transposed,
  permuted, banded, triangular-lens, symmetric).
- Changes *the very notion of "the bytes"* → **sparse**, which leaves the model.

## 2. The owner: `MdArray` = {mapping, storage}

`MdArray` is the owning analog of `mdspan` — `std::mdarray`'s shape, kept focused
(layout pinned to `layout_right`; no allocator/container-policy baggage).

It stores a **mapping** (extents + `layout_right`) and a **container** — *not* a
live `mdspan`. A stored span's `data_handle()` points into the owner's own buffer
and would dangle on copy (the `vector` reallocates) or move (an `array` relocates).
So `toMdspan()` synthesizes the span on demand; indexing forwards to the mapping.

```cpp
template <class T, class Extents, class Container = std::vector<T>>
class MdArray {
  using mapping_type = std::layout_right::mapping<Extents>;
 public:
  // ── mdspan-shaped read API: forwards to mapping + container ──
  constexpr auto operator[](std::integral auto... i) -> T& { return c_[m_(i...)]; }
  constexpr auto extents() const -> const Extents& { return m_.extents(); }
  constexpr auto extent(rank_type r) const { return m_.extents().extent(r); }
  static constexpr auto rank() { return Extents::rank(); }
  constexpr auto data_handle()       -> T*       { return c_.data(); }
  constexpr auto data_handle() const -> const T* { return c_.data(); }
  constexpr auto mapping() const -> const mapping_type& { return m_; }

  // ── the seam: a view on demand (member; the primary, mdarray-aligned API) ──
  constexpr auto toMdspan()       { return std::mdspan{c_.data(), m_}; }
  constexpr auto toMdspan() const { return std::mdspan<const T, Extents>{c_.data(), m_}; }

  // ── ownership API (owner-only concerns) ──
  // ctors (shape, row-literal+CTAD, materialize-from-mdspan), container() ...
 private:
  mapping_type m_;
  Container c_;
};
```

Storage is the only axis distinguishing the owner aliases — distinct, unrelated
types (no implicit stack↔heap conversion), one template authored once:

```cpp
template <class T, size_t R, size_t C>          using InlineMatrix = MdArray<T, extents<size_t,R,C>, array<T,R*C>>; // stack, fully constexpr
template <class T, size_t R=dyn, size_t C=dyn>  using Matrix       = MdArray<T, extents<size_t,R,C>, vector<T>>;     // heap
template <class T, size_t N>                    using InlineVec    = MdArray<T, extents<size_t,N>,   array<T,N>>;
template <class T, size_t N=dyn>                using Vec          = MdArray<T, extents<size_t,N>,   vector<T>>;
```

Invariant (enforced by `static_assert`): fixed-size storage (`array`) ⇒ all-static
extents. `InlineMatrix` is the fully-constexpr path (the constexpr gap is in *owning*
storage, not mdspan).

## 3. Views: where structure lives (layout & accessor axes)

The algorithm layer consumes views, never owners. One thin adaptor bridges them:

```cpp
constexpr auto view(auto&& x) {              // pass mdspans through; call .toMdspan() on owners
  if constexpr (is_mdspan<decltype(x)>) return x; else return x.toMdspan();
}
concept Matrix2D = /* viewable && view rank == 2 */;
concept Vector1D = /* viewable && view rank == 1 */;
```

Structure is a custom **layout** (re-indexes existing bytes) or **accessor**
(computes/mirrors on read) — never a new owner type:

| Structure | Mechanism | Why |
|---|---|---|
| transposed | extents swap over `layout_right` | free; same bytes, swapped strides |
| permuted (rows) | `layout_row_permuted` (gather) | pivot/reorder with no copy |
| banded | layout (offset re-index) | re-indexes the dense buffer |
| triangular (lens) | accessor: return 0 in the zeroed triangle | declare an existing dense matrix triangular |
| symmetric | accessor: fold `(i,j)↔(j,i)` | store/read one triangle, mirror the other |

Owner-layout boundary: a layout belongs on the **owner** only when it determines the
*allocation* (packed-triangular, padded-for-SIMD). Row vs column-major reduce to each
other by transposition, so the owner commits to one canonical (`layout_right`);
everything else is a view.

## 4. Triangular / permutation / sparse — the three stories

**Triangular** — two designs by intent (mirrors BLAS/LAPACK/std::linalg):
- *Lens*: an accessor over a full dense matrix that zeroes a triangle. Storage stays
  n². For the `L`/`U` packed into one matrix after LU. (std::linalg's
  `upper_triangle`/`lower_triangle`/`implicit_unit_diagonal` tags.)
- *Packed*: store n(n+1)/2 elements with a triangular layout. Changes the
  allocation → an **owner** variant (`PackedTriangular<inline|heap>`).

**Permutation** — an index array, never a dense matrix. `Permutation<N>` holds the
map; `permutedRows(m, p)` is a gather *layout* yielding a plain mdspan (zero-copy).
Matches LAPACK `ipiv`. Hot loops materialize (physical swaps); the lazy view's
`order[i]` indirection is cache-hostile.

**Sparse** — the boundary of the dense model. mdspan assumes `(i,j)→offset` is
arithmetic into a contiguous buffer; sparse breaks that, so it does **not** pretend
to be an mdspan. A parallel track:
- storage: CSR / COO (`inline_coordinate_list.h` exists);
- a `SparseMatrix` concept (shape + iteration over nonzeros `(i,j,value)`);
- sparse-specific kernels (`spmv`); seams to dense via `toDense` / `fromDense`.

Matches every serious library (Eigen Sparse vs Dense modules; std::linalg is
dense-only by design).

## 5. Algorithms: free functions over views

LU / QR / Cholesky / SVD / eigen are free functions over `Matrix2D`, returning small
factor objects (holding factors as owners and/or views) plus a free `solve()` /
`determinant()` / `reconstruct()`. Because they see only a view, they are storage-,
shape-, and structure-agnostic: a Cholesky over a triangular *view* of a symmetric
matrix just works. Destination-passing primitives; eager (no expression templates —
the auto-trap and aliasing bugs are consequences of laziness). Aliasing forbidden by
contract and asserted (`sharesStorage`, pointer-equality, constexpr-safe).

## 6. What we deliberately do NOT carry

- **No `Layout` parameter on the owner** — `layout_right` only. Column-major is a
  transposed view; padded layouts are the one future owner-layout (they change the
  allocation). This is the focus the earlier `MdArray`→`Dense` cut was reaching for.
- **No allocator / container-policy machinery** — a plain `Container` parameter
  (P1684 itself dropped its ContainerPolicy for this reason).
- **No expression templates** — eager, constexpr-friendly, fail-loud.
- **No second matrix type that redefines `*`** — one owner, `*` = matmul, Hadamard
  named.

## 7. Open decision

The owner name. `MdArray` is the honest, std-aligned choice (it *is* the owning
mdspan). Alternatives if the std echo is unwanted: `Grid`, `MdBuffer`. The interface
shape is the load-bearing part; the name is a rename away.
