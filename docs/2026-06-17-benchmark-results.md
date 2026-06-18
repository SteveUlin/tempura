# Benchmark harness + matrix4 efficiency measurements — 2026-06-17

Motivated by one concrete question: `lu_decomposition.h` claimed eager physical row
swaps beat "lazy permuted-view indirection — that is cache-hostile," and that claim was
asserted in a comment, never measured. This rebuilds `benchmark.h` to the point where
the question can be answered honestly, then answers it — and a few neighbours.

## The harness, and why each fix matters

`src/benchmark.h` was rewritten (see commit). The changes, in order of impact on a
trustworthy number:

- **Optimizer barriers** `doNotOptimize` / `clobberMemory`. Without a read-sink the
  compiler hoists a loop-invariant input out of the timing loop and deletes an
  un-observed output — a kernel measures ~0. GCC needs the memory alternative first
  (`"+m,r"`); `"+r,m"` errors with "impossible constraint" on non-register types.
- **`steady_clock`**, not `high_resolution_clock` (often a `system_clock` alias — an NTP
  step yields negative durations that poison the median).
- **Auto-batching + a templated callable.** The harness grows an inner repeat count
  until one batch ≥ `min_epoch` (default 1 ms), so the two clock reads amortise to
  ~0/op; the callable inlines (no `std::function` indirect call in the measured region).
  This is what lets a sub-µs kernel be timed at all.
- **Per-op latency in `double` nanoseconds.** Integer-ns `batch_duration / batch`
  truncates sub-ns work to 0 — which made `∞ GFLOP/s` possible. The oracle (below)
  caught this.
- **median + MAD + min**, not the old (overflowing, never-divided) mean±σ. Microbench
  noise is one-sided, so a 50%-breakdown estimator is the honest descriptor.
- **GFLOP/s & GB/s** driven by the median; **instability + governor/turbo warnings** to
  stderr so the harness refuses to silently lie when the machine can't be trusted.

### The measurement-correctness oracle (`benchmark_test.cpp`, runs under ctest)

Four properties the harness must satisfy, asserted as a *test* so they gate the build:
linear work scales linearly; `doNotOptimize` actually prevents elision (the critical one
— if it doesn't, every throughput figure is fiction); auto-batching keeps the epoch ≫
clock granularity; identical workloads measure within noise. The double-ns truncation
bug surfaced as oracle failures (`epoch = 0`, ratio 1.30 not ~2), not as a wrong LU
number downstream.

## The LU verdict — the comment's mechanism was wrong

`luDecomposeLazy` (a foil, built only in the bench) runs the identical algorithm but
defers each swap: a pivot swaps two *indices*, and every access gathers through `row[]`.
Both arms run bit-identical arithmetic (same pivot sequence), so the only difference is
physical-swap vs gather-on-access. Fixed-seed random matrices, no diagonal bump (so
pivoting fires heavily and `row[]` scrambles), median GFLOP/s (work = ⅔n³), one core:

```
   n      eager     lazy   eager/lazy
   4       0.91     0.63      1.45x
   8       1.96     1.38      1.42x
  16       3.01     2.90      1.04x
  32       4.33     4.06      1.07x
  64       5.23     5.07      1.03x
 128       5.55     5.32      1.04x
 256       5.80     5.62      1.03x
 512       5.90     5.80      1.02x
```

Eager wins everywhere, but the gap is ~40% at tiny n and **asymptotes to a flat ~2–3%**
by n=64 — the *opposite* of what "cache-hostile" predicts (a gap growing as rows spill
from cache). The reason is structural: in **row-major** storage, permuting *which* row
you process keeps each row's inner k-sweep contiguous (`f[row[j], k]` walks one physical
row sequentially). The gather adds one index load per row, not a scattered access
pattern. So eager is the right default — never slower, and it leaves the factors
contiguous so `solve`/extract stream without indirection — but for *those* reasons, not
cache-hostility. The source comment was corrected to match.

## Neighbours measured

**matmul** `multiply(A,B,dst)`, GFLOP/s (work = 2n³): 9.3 (n=16) → 1.3 (n=512),
monotonically falling. The naive triple loop is cache-bound with no blocking — fine for
small matrices, degrades as the working set leaves cache. An honest baseline, not a
tuned GEMM.

**transpose reduction**, GB/s — the genuine cache-hostility case, and the instructive
contrast with LU:

```
   n    contiguous   strided view   materialize+reduce
  64       13.75        13.76            7.69
 256       13.53         8.66            5.45
1024       13.44         2.72            2.43
```

Reducing over `transposed(m)` walks *columns* in row-major (stride n), touching a new
cache line per element — so the strided view collapses 13.8 → 2.7 GB/s as n grows.
*That* scatters; LU's row-permute does not — same library, opposite verdict, the access
pattern decides. Materializing first loses for a single pass (it pays a strided-read +
contiguous-write copy on top), and only wins once enough reuse amortises the copy.

**norm2** scaled (overflow-safe) vs naive √Σxᵢ², ns/element: ~1.0 vs ~0.58 in cache,
jumping to ~4.2 vs ~0.60 once the vector leaves L2 (262144 doubles = 2 MB). The scaled
form is two passes (max, then Σ(x/scale)²) with a per-element divide — out of cache the
second pass doubles memory traffic and the divide dominates. Overflow safety costs
~1.7× in cache, ~7× out of cache.

**dot(float)** double-accumulation (the `Accumulator` default) vs float: ~3% apart at
all sizes. Widening to double halves SIMD lanes but `dot` is memory/latency-bound on a
serial accumulation, so the wider add hides under memory traffic — the precision upgrade
is effectively free here. (It would bite a compute-bound, cache-resident reduction.)

## Running

```bash
cmake --preset gcc-trunk -DTEMPURA_BUILD_BENCHMARKS=ON
cmake --build build-gcc-trunk --target benchmarks
bench/run.sh build-gcc-trunk/src/matrix4/lu_decomposition_bench   # pins core, warns on env
```

Benchmarks are opt-in (`TEMPURA_BUILD_BENCHMARKS`, default OFF) and not ctest tests; the
oracle is. Numbers above are from one laptop core — the *shapes* (asymptotes, crossovers,
cache cliffs) are the portable findings, not the absolute GFLOP/s.
