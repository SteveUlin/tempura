# Learning path: foundations → Bayesian inference

2026-07-12. The build order for tempura's numerics spine, written as a
dependency graph of ideas and artifacts: understand X, build Y, Y unlocks Z.
sulin writes the code; per-brick division of labor decided at each brick.
Existing bricks are context reads, not exercises. Supersedes the spine section
of the 2026-06-14 foundation plan; complements 2026-06-15-matrix-design.md.

## Where the library stands

The dense core is real: `matrix/` has LU (scale-invariant pivoting), Householder
QR, Cholesky, Golub-Kahan SVD, symmetric eigen — all wired, all behind one
kernel architecture (free functions over mdspan views). `autodiff/` implements
the two-primitive design (Dual = jvp, tape = vjp; jacobians, jets, and
forward-over-reverse Hessians composed from them). `math/` has the toolkit
(float_bits, poly, ulp_sweep) but only two worked functions: exp (≤2 ULP) and
sqrt. Everything Bayesian is orphaned legacy (three generations). No sparse
code exists. That distribution of strength dictates the path: the linear
algebra story needs *finishing*, the scalar-function story needs *building*,
and the Bayesian story needs *rebuilding on top of both*.

## Positions

Opinionated stances, each with its reason. Deviate when an arc teaches
otherwise.

1. **Faithful rounding (≤1 ULP) is the standard target.** All the interesting
   design pressure — reduction, minimax, error-free transformations — lives
   between "naive" and 1 ULP. Correct rounding (≤0.5 ULP) is a capstone, and
   only on float32, where all 2³² inputs can be swept and the claim *proved*.
2. **Log-space discipline everywhere in stats.** Γ overflows at z≈171;
   densities multiply into underflow. Implement lgamma not gamma, logProb not
   prob. Normalizers are lgamma/logBeta expressions.
3. **Matrix-free before matrices.** CG needs only a matvec; HVP (already in
   `autodiff/hessian.h`) needs no Hessian; a preconditioner is an operator.
   The pattern recurs at every level — Newton-CG, mass-matrix adaptation,
   sparse solves — so learn operators first, storage formats second.
4. **Sparse means precision matrices, nothing more.** A Bayesian lib needs
   sparse exactly where conditional independence makes precision matrices
   sparse (GMRF/CAR/hierarchical Laplace). The ceiling is CSparse-level
   simplicial Cholesky with log-det behind a symbolic/numeric split — not
   general sparse arithmetic, not supernodal BLAS3. Rung zero (CSR SpMV + CG)
   is nearly free. GPs do NOT motivate sparse: kernel matrices are dense.
5. **Every brick lands working, with its oracle.** ULP sweep for scalar
   functions, reference cross-check for samplers, measured numbers for
   kernels. No unbacked scaffolding.
6. **Kernels stay free functions over views.** New storage (CSR, banded,
   permuted) means new types + a few new kernels, never a kernel-set fork.
   This seam is already in place; sparse must not break it.

## The spine

```
A scalar fns ──────────► D distributions ◄─verify─ E quadrature (sidetrack)
   (lgamma, erf)               │
B dense linalg ──► C optimization ──► Laplace bridge
   │    (autodiff, built, feeds C and F)      │
   └──► CG ──► G sparse                       ▼
                  │          F MH → HMC → NUTS ──► diagnostics
                  └─────────────► capstone: GMRF model via sparse Cholesky
```

A and B are independent — interleave freely. C needs B's CG only at its end.
D needs A's lgamma. F needs C's gradient plumbing and D's transforms. G needs
B's CG and earns its keep only inside F's capstone.

---

## Arc A — scalar function approximation

The libm arc. One skeleton governs every function: **reduce → approximate →
reconstruct**. Shrink an unbounded argument onto a tiny interval, evaluate a
low-degree polynomial there, undo the reduction. The error budget is dominated
by the *reduction*, not the polynomial — that inversion of intuition is the
arc's central lesson.

Context reads: `math/float_bits.h`, `math/poly.h`, `math/ulp_sweep.h`,
`math/exp.h` (the worked example of the whole skeleton).

1. **expm1, log1p** — understand: catastrophic cancellation — subtraction of
   near-equal values exposes prior rounding error; why e^x−1 and ln(1+x) need
   first-class entry points (the naive forms lose every significant bit near
   0). build: both, small-|x| polynomial + large-|x| fallback, under the ULP
   sweep. The fastest possible introduction to the arc's core enemy.
2. **log** — understand: multiplicative reduction — x = m·2^e with m ∈
   [√½, √2), so log x = e·ln2 + log m; why ln2 is stored split hi+lo (exp.h
   already reserves the constants). build: log; it completes the exp/log pair
   and unlocks pow later.
3. **sin, cos** — understand: additive (Cody–Waite) reduction, k·π/2 with π/2
   split into 2–3 words; parity structure (sin = odd powers only, cos = even);
   why zeros at kπ make reduction the whole problem. build: sin/cos for
   moderate |x|; delete the legacy `math/sin.h`/`cos.h` orphans they replace.
4. **Minimax coefficients** — understand: Taylor is the worst basis for a
   fixed degree (error piles at interval edges); Chebyshev is near-minimax;
   Remez equioscillation is optimal; and real coefficients must be
   re-optimized over the *representable* lattice (Sollya's fpminimax).
   build: Sollya into the dev shell; regenerate exp's degree-13 Taylor as
   minimax and measure the degree drop. Constants stay derived-with-provenance.
5. **Error-free transformations** — understand: TwoSum, FastTwoSum,
   TwoProd-via-FMA — the rounding error of + and × is itself exactly
   representable; double-double as unevaluated hi+lo pairs (~106 bits).
   build: `eft.h`, extend poly.h's compensated Horner onto it.
6. **Huge-argument reduction** — sidetrack: Payne–Hanek — multiply by
   hundreds of stored bits of 2/π, windowed by the exponent. build: the slow
   path for sin/cos at large |x|; musl's `__rem_pio2.c` is the living example.
7. **Oracle upgrade** — understand: faithful (≤1 ULP) vs correctly rounded
   (≤0.5 ULP); the Table Maker's Dilemma. build: MPFR behind the ULP sweep as
   the true-value oracle + an exhaustive float32 sweep (all 2³² inputs,
   minutes of runtime).
8. **Capstone: correctly-rounded expf** — build: retarget float32 exp to
   correct rounding and *prove* it with the exhaustive sweep. The
   hobbyist-achievable version of what CORE-MATH/RLIBM do.
9. **erf, erfc** — understand: domain splitting — compute whichever of
   erf/erfc is small directly, derive the other (1−erf at large x has no
   bits); rational approximation, scaled erfcx. build: erf brick; supersedes
   `special/erf.h`.
10. **lgamma** — understand: reflection maps z<½ to z≥½; Spouge (closed-form
    coefficients, rigorous bound — easiest to get right) vs Lanczos (best
    accuracy per term) vs Stirling (large z, fewest constants). build: lgamma
    via Spouge first, Lanczos as a refinement pass; add logBeta. Supersedes
    `special/gamma.h` and unlocks Arc D — every discrete/shape-family
    normalizer is an lgamma expression.

## Arc B — dense linear algebra, finishing the story

The through-line is **conditioning and stability**: every algorithm choice in
this arc is a response to "how much can the answer move per unit of input
error, and does my algorithm add more?" That's why the pedagogy runs QR before
LU — orthogonal transforms don't amplify error, and the least-squares trap
(normal equations square the condition number: κ(AᵀA) = κ(A)²) motivates
orthogonality *before* pivoting ever comes up.

Context reads: `matrix/matrix.h` (the view/kernel seam), `matrix/qr.h`,
`matrix/cholesky.h`, `matrix/lu_decomposition.h`.

1. **Rectangular QR → lstsq** — understand: projectors, least squares as
   projection, why Householder beats Gram-Schmidt (stability), the normal-
   equations trap. build: lift the square-only asserts, thin/economy QR, an
   `lstsq` front-end. Least squares is the gateway drug to statistics.
2. **Linear regression demo** — build: fit + residual plot via plot.h. First
   contact between matrix/ and data.
3. **Conditioning made concrete** — understand: κ as worst-case error
   amplification; backward vs forward error. build: a condition-number
   estimate (rcond from a factorization) so `solve` can report trust.
4. **CG** — understand: the Krylov idea — treat A as an operator, build the
   answer from the subspace {b, Ab, A²b, …}; CG as optimization over that
   subspace (SPD case). build: matrix-free `cg(matvec, b)` taking any
   callable. This one brick bridges three arcs: dense iterative methods,
   Newton-CG in Arc C, and sparse solves in Arc G — because it never asks
   what A *is*, only what A *does*.
5. **Newton-CG** — build: CG + `hvp` from autodiff = truncated Newton without
   ever materializing a Hessian. The matrix-free position paying off.
6. **Sidetrack: nonsymmetric eigen** — understand: Hessenberg reduction +
   shifted QR iteration. build only if the itch strikes; nothing downstream
   needs it.

## Arc C — optimization

First real consumer of the autodiff module. Backprop *is* reverse-mode AD —
`tape.h` already implements it (flat Wengert tape, linear backward sweep);
this arc is where it gets used in anger. The conceptual spine: a gradient
costs ~3–4× one function evaluation regardless of dimension (the cheap
gradient principle), so gradient-based methods dominate; everything after
that is about turning gradients into good *directions* and *step sizes*.

Context reads: `autodiff/README`, `dual.h`, `tape.h`, `transform.h`,
`hessian.h` — understand jvp/vjp as the only primitives, everything else
composition.

1. **Line search** — understand: sufficient decrease + curvature (Wolfe
   conditions) — why "just take a small step" fails both ways. build:
   backtracking + Wolfe line search.
2. **Descent baselines** — build: gradient descent ± momentum on Rosenbrock
   (`special/rosnbrock_function.h` exists — note the filename typo when
   touching it). Plot trajectories with plot.h; the banana valley teaches
   conditioning viscerally.
3. **L-BFGS** — understand: the secant condition; approximating curvature
   from gradient history; why storing m vector pairs beats storing a matrix.
   build: two-loop recursion L-BFGS.
4. **MLE payoff** — build: logistic regression fit by tape-grad + L-BFGS.
   First statistical estimation done end-to-end with own machinery.
5. **Laplace bridge** — understand: posterior ≈ Gaussian at the MAP with
   covariance H⁻¹ — the cheapest approximate-Bayes there is, and the
   conceptual bridge from optimization to inference. build: `laplace` from
   grad + hessian + cholesky (log-det comes free from the factor).

## Arc D — randomness and distributions

Requires A10 (lgamma). The legacy `prob/` tree is reference reading — clean
shapes, wrong regime — rebuild fresh under current conventions.

1. **PRNG** — understand: state, period, streams; why counter-based (Philox)
   vs xorshift-family (xoshiro256++) — reproducibility and parallel streams
   vs raw speed. build: `random.h`, one generator, justified.
2. **Sampling transforms** — understand: inverse-CDF, rejection,
   ratio-of-uniforms, ziggurat — the toolbox for turning uniform bits into
   any shape. build: normal, exponential, gamma samplers.
3. **Distributions** — understand: log-density discipline (position 2);
   every normalizer as an lgamma/logBeta expression. build: normal,
   exponential, gamma, beta, binomial, Poisson, student-t — each with
   logProb + sample.
4. **Unconstraining transforms** — understand: change of variables needs the
   log-Jacobian; HMC lives in unconstrained space, so σ>0 samples via log σ,
   p∈(0,1) via logit p. build: `transforms` brick. Non-negotiable
   prerequisite for Arc F on real models.
5. **Verification** — build: sampled moments vs closed forms and vs Arc E
   quadrature; empirical-CDF plots as the visual oracle.

## Arc E — quadrature (sidetrack)

Optional, but it repays two arcs and repairs known breakage. understand:
orthogonal polynomials and the three-term recurrence; Golub–Welsch — Gaussian
nodes/weights are the eigendecomposition of the Jacobi matrix, so
`symmetric_eigen.h` already contains the hard part and the magic constants in
tables become *derived*. build: Gauss-Legendre/Hermite via Golub–Welsch,
replacing the broken `quadature/gaussian.h` (Hermite initial-guess sign bug;
directory name is also a typo). Payoff: an independent oracle for Arc D
normalizers/moments and for Laplace-approximation quality.

## Arc F — MCMC → HMC → NUTS

The capstone arc. Each tick is a *working sampler*, strictly extending the
last.

Context reads (archaeology, not templates): `bayes/estimation/hmc.h`,
`dual_averaging.h` from the orphaned first generation.

1. **Why sample** — understand: expectation is the product of density × 
   volume, and in high dimension that mass concentrates in a thin shell (the
   typical set) far from the mode. build: sample a d=100 Gaussian, histogram
   the radius with plot.h — watch the shell appear.
2. **Metropolis-Hastings** — understand: Markov chains, detailed balance as
   the correctness argument. build: random-walk MH on 2D targets (banana,
   donut); braille-plot chains. Watch it diffuse.
3. **HMC** — understand: augment with momentum; Hamiltonian flow conserves
   H, so a perfect integrator would accept always; leapfrog is symplectic —
   its energy error oscillates instead of drifting, which is the entire
   trick. build: leapfrog + fixed-step HMC; verify H conservation on a
   Gaussian; compare mixing against MH side by side.
4. **Step-size adaptation** — understand: acceptance rate vs ε; dual
   averaging as stochastic optimization of ε toward a target acceptance
   (δ≈0.8). build: warmup ε adaptation.
5. **Mass matrix** — understand: the metric is a preconditioner — Arc B's
   conditioning through-line resurfacing in sample space; diagonal metric
   from warmup variances. build: windowed diagonal-metric adaptation.
6. **NUTS** — understand: the U-turn criterion; recursive tree doubling with
   random direction (reversibility); multinomial trajectory sampling (the
   modern replacement for the paper's slice variable). build: NUTS with max
   treedepth.
7. **Divergences** — understand: a diverging trajectory (ΔH exploding) flags
   high-curvature geometry the integrator can't follow — a *diagnostic*, not
   a nuisance. build: detection + reporting.
8. **Diagnostics** — understand: split R-hat (rank-normalized), bulk/tail
   ESS — the difference between "it ran" and "it worked." build:
   `diagnostics` brick.
9. **Capstone: eight schools** — build: the hierarchical model; watch the
   funnel produce divergences in centered form, fix by non-centered
   reparameterization, cross-check posterior numbers against a reference
   implementation. This is the "sampler is correct" certificate.

## Arc G — sparse (after dense)

Enters only when a model shape demands it. understand first, build second:

1. **Why precision matrices** — understand: sparse precision Q ⟺
   conditional independence (the GMRF theorem); covariance of the same model
   is dense. This single idea decides everything the arc builds.
2. **Formats** — understand: COO for construction, compressed for compute;
   CSC of A = CSR of Aᵀ, so one compressed format suffices; structure
   immutability is what buys arithmetic speed. build: COO builder → CSR +
   SpMV.
3. **Iterative rung** — build: Arc B's matrix-free CG over SpMV + Jacobi
   preconditioner. If CG was built right, this tick is a one-liner — the
   payoff of position 3.
4. **Sparse Cholesky** — understand: fill-in; elimination order (minimum
   fill is NP-hard, AMD is the heuristic); the elimination tree; the
   symbolic/numeric split — analyze the pattern once, refactor values many
   times, which is exactly a sampler loop's shape. build: simplicial sparse
   Cholesky with log-det. Ceiling: CSparse scale (~2k lines total). AMD is a
   stretch tick.
5. **Capstone: GMRF model** — build: a CAR/ICAR spatial model (or 1D Matérn
   via the SPDE→GMRF route), log-density through the sparse factor, sampled
   with Arc F's NUTS. Sparse earning its keep on a real posterior.

## Milestones

- **M1** sin/cos wired under the ULP oracle; legacy orphans deleted.
- **M2** lstsq + linear regression on data.
- **M3** correctly-rounded expf, proved by exhaustive float32 sweep.
- **M4** logistic regression fit by own tape + own L-BFGS.
- **M5** Laplace approximation of a real posterior.
- **M6** lgamma lands; distributions brick fully in log space.
- **M7** HMC conserving H on a Gaussian, visibly out-mixing MH.
- **M8** NUTS + R-hat/ESS matching a reference on eight schools.
- **M9** GMRF posterior via sparse Cholesky log-det.

## Out of shape (callouts, not path items)

- Tracked debug binaries at repo root (`reproduce_issue`, `test_discover*`)
  plus loose repro sources — delete; they slip past .gitignore.
- Working copy is an undescribed commit mid-landing `comm/`.
- `math/sin.h`, `math/cos.h`: pre-oracle legacy, include the orphaned
  `chebyshev.h` — replaced by A3.
- `special/gamma.h`, `special/erf.h`: NR ports outside the tested regime —
  superseded by A9–A10.
- `quadature/` (typo'd name) with the broken Hermite — replaced by Arc E.
- Three bayes + three symbolic generations (~46k LOC) unbuilt. Not this
  path's problem: Arcs D/F rebuild fresh; the symbolic stack is its own
  future arc and still awaits the consolidation pass matrix got.

## Sources shelf

One canonical source per arc, for when a tick wants depth. Ignore until then.

- **A**: Muller, *Elementary Functions* (the book for this exact task);
  Goldberg's "What Every Computer Scientist Should Know About Floating-Point"
  as the free prerequisite. Tools: Sollya (fpminimax), MPFR (oracle).
- **B**: Trefethen & Bau, *Numerical Linear Algebra* (learn); Golub & Van
  Loan (cross-check pseudocode and error bounds).
- **C**: Nocedal & Wright ch. 2–8; JAX autodiff cookbook for the jvp/vjp
  worldview; Griewank & Walther ch. 2–4 for tape theory.
- **D**: Devroye, *Non-Uniform Random Variate Generation* (free online).
- **E**: Golub & Welsch 1969.
- **F**: Neal 2011 (implement from this) → Betancourt 2017 (why) → Hoffman &
  Gelman 2014 (NUTS spec) → Stan reference manual (modern variant) →
  Vehtari et al. 2021 (R-hat/ESS). Reference code: minimc, littlemcmc.
- **G**: Davis, *Direct Methods for Sparse Linear Systems* (CSparse is ch. 9);
  Saad, *Iterative Methods* (free) for the Krylov side.
