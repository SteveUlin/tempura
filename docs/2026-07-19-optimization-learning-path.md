# Optimization Learning Path — Arc C, Classical Smooth Regime

**For:** sulin · deterministic, small-n, exact gradients from your own jvp/vjp autodiff. Unconstrained smooth minimization is not a special case here — it *is* the whole regime, so the path runs the trunk of Nocedal & Wright (Ch. 2, 3, 6, 7) and cuts everything downstream of it.

**Shape of each stage:** READ (sections + honest hours) → DERIVE (reconstruct by hand *first*) → BUILD (the tempura brick + its oracle) → PAYOFF. The DERIVE box states the target you should land on — reconstruct toward it, then check yourself against it; don't read it as the lede. Reading never runs more than ~4h before a brick lands. You write every brick; this maps and reviews, never implements.

**Spine:** backtracking Armijo → GD+momentum baselines → strong-Wolfe → L-BFGS → logistic MLE → Laplace bridge. Newton-CG hangs off Arc B and is staged separately once `cg.h` exists.

**Honest budget:** ~18h reading across the spine (Stages 0–6), ~1.5h more for the Newton-CG bridge; optional side-reads add a few hours on top. Building is the rest and dominates. At 1–3h evening sessions plus weekends, the spine is **~2.5–3 months**. The SKIP list is what keeps it months instead of a year you abandon.

---

## Stage 0 — Optimality conditions (the ground floor)

You are about to write code that *stops* at a minimum. First pin down what "minimum" means formally, so every oracle downstream has a target it can assert against.

**READ** (~2h)
- N&W **§2.1** (first/second-order necessary & sufficient conditions via Taylor's theorem) — the definitions your convergence tests check.
- N&W **§2.2** (line-search vs trust-region as the two master strategies; the search-direction catalog) — *orientation only, skim.* It names steepest-descent / Newton / quasi-Newton / CG so the later chapters have a frame. Don't linger.

**DERIVE** (before reading §2.1)
- From the second-order Taylor expansion `f(x+p) = f(x) + ∇fᵀp + ½pᵀ∇²f p + o(‖p‖²)`, recover the two conditions yourself: `∇f = 0` (else pick `p = −∇f`) and `∇²f ⪰ 0` (else pick `p` along a negative-curvature eigenvector). You've done the descent-lemma algebra already — this is the same Taylor bound read at a stationary point.

**BUILD** — nothing yet. This is the one pure-reading stage; it's short (~2.3h of reading stands before the first brick lands in Stage 1) and it pays for itself in every oracle that asserts `∇f ≈ 0`.

**PAYOFF:** a formal stopping test (`‖∇f‖ < tol`) and the SPD-Hessian condition the Laplace bridge later leans on.

---

## Stage 1 — Backtracking Armijo line search

You've already derived the Armijo condition, both naive-decrease failure modes, and the descent-lemma lower bound on the accepted step. So this stage is **almost entirely build** — resist re-reading theory you own.

**READ** (~20 min)
- **Armijo 1966**, page 2 only — confirm the rule you derived is verbatim his: shrink `α` by a factor until `f(x+αp) ≤ f(x) + c·α·∇fᵀp`. The rest of the paper is an ε–δ convergence proof in a Banach-space generality you don't need. *Do not read the proof.*

**DERIVE** (you have this — just write it down as the brick's contract)
- The guaranteed per-step decrease: with `c₁ ∈ (0,1)` and shrink factor `ρ`, backtracking on an `L`-smooth `f` terminates with `α ≥ min(α₀, 2ρ(1−c₁)·(−∇fᵀp)/(L‖p‖²))`. This inequality is your oracle's expected bound.

**BUILD** — finish `src/optim/line_search.h` (currently comment-only, stalled pre-Wolfe). Implement `backtrackingArmijo(ϕ, ϕ′0, α₀, c₁, ρ)` taking `ϕ` as a callable (matrix-free discipline: it takes the 1-D restriction, not `f` + `x` + `p`). Note the contract: this returns the *first* geometric trial satisfying Armijo, **not** the line minimizer.
- **Oracle** (`line_search_test.cpp`, currently an empty shell): on a `QuadraticFn` restriction, `ϕ` is an exact parabola with known `ϕ′(0)` and `ϕ″(0)`, so the exact minimizer `α* = −ϕ′(0)/ϕ″(0)` and the Armijo threshold are both closed-form. Pick `α₀`, `ρ`, `c₁` so the accepted trial index is predictable, then assert (a) the returned `α` satisfies the Armijo inequality, and (b) it is exactly `α₀ρᵏ` for the predicted `k` — bit-for-bit at points where intermediates are binary-exact, matching the `objectives_test.cpp` house style. On `RosenbrockFn` from a fixed start, assert `f` strictly decreases and Armijo holds.

**PAYOFF:** the first real Arc C brick lands. Every optimizer below plugs a search direction into this.

---

## Stage 2 — Descent baselines: GD, then momentum, on Rosenbrock

Now build the playground that makes curvature *visible*. This is where you feel, empirically, why plain gradient descent is bad — the felt problem that forces quasi-Newton into existence two stages later.

### 2a — Gradient descent

**READ** (~2h)
- Boyd & Vandenberghe **§9.2–9.3** (general descent framework; gradient descent + backtracking; the descent lemma and strong-convexity bounds). This is the same descent lemma you derived, stated cleanly with the convergence bookkeeping.

**DERIVE** (before Boyd 9.3)
- GD's linear rate on a quadratic `f = ½xᵀAx`. Diagonalize: each eigen-coordinate contracts by `(1 − αλᵢ)`. The optimal *constant* step `α = 2/(λmin+λmax)` gives spectral radius `(κ−1)/(κ+1)` where `κ = λmax/λmin`. This *is* the "ill-conditioning cripples GD" result — you should be able to invent it from the eigendecomposition alone.

**BUILD** — `src/optim/descent.h`: `gradientDescent(f, x₀, {tol, maxIter})` using your Stage-1 line search and tape-gradients.
- **Oracle** (`descent_test.cpp`): (a) with the line search, converges to Rosenbrock's `(a, a²)` within `atol`; (b) **rate oracle** — on a diagonal `QuadraticFn` with a chosen `κ`, run GD with the *optimal constant step* `α = 2/(λmin+λmax)` and assert the asymptotic per-step ratio `‖xₖ₊₁−x*‖/‖xₖ−x*‖ → (κ−1)/(κ+1)` within tol. The fixed-step variant is what makes the closed-form rate assertable — a backtracking line search has no clean closed-form rate, so it proves convergence (part a) but can't anchor the rate. The rate oracle is the one that proves you understand *why*, not just *that*.

### 2b — Momentum

**READ** (~2h)
- Boyd **§9.4** (steepest descent, incl. non-Euclidean norms — the seed of quasi-Newton's implicit metric) + N&W **§3.3** (proved linear rate of steepest descent, quadratic rate of Newton, superlinear for quasi-Newton). N&W 3.3 is your rate roadmap for the whole arc.
- *Optional side-read (~1h):* Distill.pub **"Why Momentum Really Works"** — the damped-oscillator / eigenvalue picture. Read §"First Steps"→§"Choosing a Step-size" for intuition; skip the Colorization and stochastic sections.

**DERIVE**
- Heavy-ball on the same quadratic: the two-term recurrence per eigen-coordinate is a linear system whose spectral radius, tuned, improves the rate to `(√κ−1)/(√κ+1)`. Deriving the `√κ` is the "momentum is curvature-aware, cheaply" insight.

**BUILD** — add `heavyBall(f, x₀, {step, momentum, tol})` to `descent.h`. Heavy-ball takes a fixed step, not a line search — the momentum term supplies the acceleration, and a fixed step is what the closed-form rate is a property of.
- **Oracle:** (a) momentum reaches Rosenbrock's min in strictly fewer iterations than plain GD from the same start; (b) **rate oracle** — on the diagonal quadratic, run heavy-ball with the *optimally tuned* constant step and momentum and assert the asymptotic contraction `→ (√κ−1)/(√κ+1)` within tol. The tuned-constant run is required: the `√κ` rate is a property of the optimal step/momentum pair, not of any line-searched iteration. **Visual oracle:** overlay GD vs momentum trajectories on Rosenbrock contours via `plot.h` — the banana-crawl vs the faster descent is the picture that motivates everything after.

**PAYOFF:** you now *feel* the condition-number problem. L-BFGS becomes the answer to a question you've physically hit, not a recipe handed down.

---

## Stage 3 — Strong-Wolfe line search

Armijo alone lets `α` shrink pathologically toward zero: steps that decrease but never *progress*. The fix is the curvature condition — and, not coincidentally, that exact inequality is what keeps a BFGS secant update positive-definite. Build this now because L-BFGS silently assumes it.

**READ** (~3.5h)
- N&W **§3.1** (Wolfe & Goldstein conditions; existence proof for step lengths satisfying Wolfe).
- N&W **§3.5** (interpolation-based step selection; **Algorithm 3.5** — the bracket-then-zoom strong-Wolfe search). This is your implementation blueprint.
- **Wolfe 1969** §on the curvature condition (~40 min, genuinely pleasant SIAM Review prose). Note that **Wolfe 1971 (Part II)** exists as a correction to a convergence lemma — know it's there; don't read it closely.

**DERIVE** (before §3.1)
- Show Armijo-only admits `α → 0`: for small `α`, `ϕ(α) ≈ ϕ(0) + α ϕ′(0)` satisfies sufficient decrease for *any* tiny step, so nothing forces progress. Then derive the curvature condition `ϕ′(α) ≥ c₂ ϕ′(0)` as "the trial point's slope must have risen" — a lower fence complementing Armijo's upper fence. Finally, connect: for direction `p` and step `s = αp`, `yᵀs = (∇f₊ − ∇f)ᵀs`, and the curvature condition gives `yᵀs > 0`. **This last line is the entire reason L-BFGS works** — derive it yourself and the "L-BFGS occasionally emits an ascent direction" failure mode becomes an *expected* symptom of a sloppy line search, not a mystery bug.

**BUILD** — extend `line_search.h` with `strongWolfe(ϕ, ϕ′, {c₁, c₂})` implementing bracket + zoom (Algorithm 3.5), cubic/quadratic safeguarded interpolation.
- **Oracle:** a battery of 1-D `ϕ` including a nonconvex one with multiple stationary points — assert the returned `α` satisfies *both* the Armijo and curvature inequalities; assert zoom terminates within a bounded iteration count on each; explicitly assert `yᵀs > 0` at the returned step. This is the invariant the next stage depends on.

**PAYOFF:** a line search robust enough to feed a quasi-Newton method, and the `yᵀs > 0` guarantee in hand as a *derived* fact.

---

## Stage 4 — L-BFGS (two-loop recursion)

The centerpiece. You've felt why GD is slow (Stage 2) and secured the curvature guarantee it needs (Stage 3). Now approximate the inverse Hessian from the last `m` steps in `O(mn)` — no matrix stored, matching your matrix-free house rule.

**READ** (~4h, split — read the derivation half, build, then read the recipe)
- N&W **§6.1** (BFGS derived from the secant equation + minimal-change; the inverse-Hessian update and initial scaling). *Skip §6.2 SR1, §6.3 Broyden class, §6.4 convergence proofs on this pass — see SKIP.*
- N&W **§7.2** (the L-BFGS two-loop recursion; its relationship to CG; compact representation).
- **Tibshirani CMU 10-725 quasi-Newton lecture** — the single best free source for the secant→BFGS→L-BFGS thread; Boyd and Bubeck both skip quasi-Newton entirely. (Read *after* Boyd, as a second pass; it assumes Boyd-level fluency.)
- **Liu & Nocedal 1989**, first few pages — *the* implementation source for the two-loop recursion; read it while writing the loop, not before or after.
- *Optional-joy, post-hoc (~50 min):* **Nocedal 1980** — where the recursion's algebra comes from, by unrolling BFGS over `m` steps. Read only after your code works, for the derivation, not the recipe.

**DERIVE** (before §6.1)
- The secant equation `Bₖ₊₁ sₖ = yₖ` (the update must reproduce the observed gradient change — a finite-difference curvature match). Then the BFGS rank-2 inverse update as the minimal-change PSD-preserving solution (Sherman–Morrison turns the `B`-update into an `H = B⁻¹` update). Then unroll: applying `Hₖ` to `∇fₖ` using only stored `(sᵢ, yᵢ)` pairs collapses into the forward/backward two-loop sweep. Deriving the two loops by unrolling — rather than transcribing them — is the reconstructive win here.

**BUILD** — `src/optim/lbfgs.h`: `lbfgs(f, x₀, {m, tol})` using tape-gradients + the Stage-3 strong-Wolfe search.
- **Oracle:** (a) on Rosenbrock, converges in far fewer iterations than Stage-2 GD/momentum from the same start; (b) **two-loop identity oracle** — on a small problem, assert the two-loop output equals a materialized dense-BFGS `Hₖ · ∇fₖ` product built from the same `m` stored pairs (a test-only reference implementation you also write), close to machine precision. That equality is the proof the recursion is *correct*, not merely convergent. (c) `yᵀs > 0` skip-guard fires correctly if the line search ever violates curvature.

**PAYOFF:** the workhorse optimizer this arc is built around — the one the MLE and Laplace stages both call. (Robust real-world line-search hardening — Moré-Thuente — is deferred; see SKIP.)

---

## Stage 5 — Logistic-regression MLE (the payoff model)

Point the optimizer at a real statistical model. This is where autodiff (reverse-mode tape gradients) + L-BFGS + a genuine likelihood compose into something you'd actually use.

**READ** (~1.5h)
- A logistic-likelihood source — **MacKay, *Information Theory, Inference, and Learning Algorithms*, Ch. 39** (free PDF) *or* **Bishop PRML §4.3.2**. Short; you mostly need the log-likelihood, its gradient, and its Hessian.

**DERIVE** (before reading — this is very reconstructable)
- The Bernoulli log-likelihood `ℓ(β) = Σ [yᵢ log σ(xᵢᵀβ) + (1−yᵢ) log(1−σ(xᵢᵀβ))]`. Differentiate: gradient `= Xᵀ(σ(Xβ) − y)`, Hessian `= −XᵀSX` with `S = diag(σ(1−σ))`. The Hessian is negative semidefinite → the negative log-likelihood is convex → L-BFGS converges to the global MLE. Deriving the PSD structure yourself is what tells you the problem is safe for a quasi-Newton method.

**BUILD** — `src/optim/logistic.h`: define the negative log-likelihood as a tempura objective (generic `auto` params, so it runs under `Dual` and `Tape` like the objectives zoo), minimize with `lbfgs`.
- **Oracle** (`logistic_test.cpp`): synthetic data drawn from known `β` (well-separated but not linearly separable — complete separation sends the MLE to infinity, a good edge case to *document*, not hit). Assert (a) recovered `β̂` matches a reference fit (hardcode coefficients from R `glm` / scikit-learn) within a stated tolerance; (b) `‖∇ℓ(β̂)‖ < tol` (Stage-0 stopping condition); (c) negative-log-likelihood is monotone non-increasing across iterations.

**PAYOFF:** MLE via *your* autodiff + *your* optimizer on a real model. And you now have a MAP/MLE point — exactly what the Laplace bridge expands around.

---

## Stage 6 — Laplace bridge (posterior ≈ Gaussian at the mode)

The arc's capstone and the seam into the MCMC/HMC arc. Approximate a posterior by the Gaussian matching its mode and curvature. Every ingredient except the composition already exists in tempura: `hessian()` and `hvp` (in `autodiff/hessian.h`, forward-over-reverse, never forms `H` wastefully) and `cholesky.h` (log-det free from the factor's diagonal).

**READ** (~2h) — **none of the six optimization resources covers Laplace; source it separately:**
- **Bishop PRML §4.4** ("The Laplace Approximation") *or* **MacKay Ch. 27**. Short and self-contained.

**DERIVE** (before reading — fully reconstructable from Stage 0's Taylor machinery)
- Second-order Taylor of the log-posterior `log p(θ|D)` around the MAP `θ*`: the linear term vanishes (`∇ = 0` at the mode), leaving `log p ≈ const − ½(θ−θ*)ᵀ A (θ−θ*)` with `A = −∇² log p(θ*)`. Match to a Gaussian: mean `θ*`, covariance `A⁻¹`. Recover the normalizing constant from the multivariate Gaussian integral: `Z ≈ p(θ*,D)·(2π)^{d/2} |A|^{−1/2}`, and read `log|A|` off `2·Σ log Lᵢᵢ` from the Cholesky factor `A = LLᵀ`. Deriving that the Gaussian's covariance is the *inverse* Hessian — and that Cholesky hands you the log-det for free — is the whole bridge.

**BUILD** — `src/optim/laplace.h`: `laplace(logPosterior, θ₀)` = (find mode by minimizing `−logPosterior` with `lbfgs`) → (`hessian()` at the mode) → (`cholesky`) → return `(mean=θ*, cov=A⁻¹ via factor, logZ)`.
- **Oracle:** (a) **exactness oracle** — on a Gaussian target the log-posterior is *quadratic*, so its Hessian is constant and Laplace is exact. Split the assertion by what the mode solve does and doesn't touch: the returned covariance and `log|A|` match the true `A⁻¹` and `log det` to machine precision (they don't depend on the mode — the Hessian is the same everywhere), while the mean matches the true mean only to the *optimizer's* tolerance (the mode comes from `lbfgs`, so assert `‖mean − μ_true‖ ≲ tol`, not machine-exact); `logZ` matches the closed-form Gaussian normalizer given that mode. This split is the honest version of "Laplace is exact on a Gaussian." (b) On the Stage-5 logistic posterior with a hand-written Gaussian log-prior (`−½βᵀβ/σ²` — no Arc D distribution brick needed), assert the marginal variances are finite, positive, and shrink as data grows (sanity, not exactness).

**PAYOFF:** Arc C closes. You have posterior-as-Gaussian, which is both a usable Bayesian tool *and* the natural launch point for the MCMC/HMC arc (HMC's mass matrix ≈ the Laplace precision; NUTS is the capstone downstream). Fitting a full Arc-D distribution model is a later payoff, gated on Arc A's `lgamma` and Arc D's log-density brick — the bridge itself lands now on the hand-written logistic posterior.

---

## Bridge stage (parallel, gated on Arc B) — Newton-CG

Newton-CG is on your tick list but arrives through the **dense-linalg arc (Arc B)**, not Arc C — it needs `matrix/cg.h`, which **does not exist yet** (no CG solver anywhere in the tree; only two comment mentions, in `matrix/vec.h` and `autodiff/hessian.h`). Slot it in whenever Arc B lands the matrix-free `cg(matvec, b)` brick. It's the one brick that bridges three arcs (dense-iterative, Newton-CG, sparse), precisely because it's written against an *operator*, not a stored matrix.

**READ** (~1.5h)
- N&W **§7.1** (inexact Newton theory; Line-Search Newton-CG; Steihaug-Toint; the inexact-Newton local-convergence result).
- Boyd **§9.5** (Newton's method, damped + quadratic phases — the convergence story Newton-CG inherits) and **§9.7** (implementation: computing the Newton step, and *why* an iterative solve replaces a factorization for large systems — the motivation for Newton-CG).
- **Tibshirani CMU 10-725 truncated-Newton lecture** for the explicit Newton-CG treatment.

**DERIVE**
- Newton step solves `∇²f · p = −∇f`. Instead of factoring `∇²f`, run linear CG on that SPD system using **only Hessian-vector products** — which you already have as `hvp` (forward-over-reverse). Truncate CG early (inexact Newton): far from the mode a rough solve suffices; the forcing sequence `ηₖ → 0` recovers the quadratic rate near the mode. Derive the negative-curvature early-termination rule (if a CG direction hits `dᵀHd ≤ 0`, stop and step — this is what makes it robust on nonconvex `f` like Rosenbrock).

**BUILD** — first `matrix/cg.h` (`cg(matvec, b, {tol})`, matvec as a callable — matrix-free), then `optim/newton_cg.h` wiring `hvp` as the matvec.
- **Oracle:** (a) `cg` solves a known SPD system to residual tol and matches a direct `cholesky` solve within tol; (b) Newton-CG converges *quadratically* near Rosenbrock's min (assert the error-squaring `‖eₖ₊₁‖ ≈ C‖eₖ‖²` in the final iterations — the rate signature that distinguishes it from L-BFGS's superlinear); (c) negative-curvature termination fires and still produces a descent step from a start where `∇²f` is indefinite.

**PAYOFF:** a second-order optimizer that never forms a Hessian — and the CG brick Arc G's sparse precision-Cholesky rung also needs.

---

## SKIP LIST (ruthless — this is what makes the path finishable)

| Skipped | Why |
|---|---|
| **N&W Ch. 4 (Trust Region), entirely** | The other master strategy. Genuinely worth knowing — but you're building the line-search half, and every Arc C tick is line-search. Revisit only if you hit a problem where line search stalls on indefinite curvature. *Deferred, not condemned.* |
| **N&W Ch. 5 (Conjugate Gradient): §5.2 nonlinear CG skipped; §5.1 linear-CG theory deferred** | Linear CG you get *operationally* in the Newton-CG bridge (via §7.1, cleaner). Nonlinear CG (Fletcher-Reeves/Polak-Ribière) is dominated by L-BFGS in your regime — L-BFGS is why nonlinear CG stopped being the default. Read §5.1's linear-CG theory later *only if* you want the eigenvalue-clustering convergence bound. |
| **N&W §3.4 (Hessian modification)** | Modified Cholesky / eigenvalue tweaks for indefinite Newton steps. Newton-CG's negative-curvature termination handles indefiniteness more cleanly for your matrix-free regime. Skip. |
| **N&W §6.2 SR1, §6.3 Broyden class, §6.4 convergence proofs** | SR1 risks indefiniteness (the thing Wolfe+BFGS specifically avoid); the Broyden class is a unifying-taxonomy read, not a build; §6.4's Dennis-Moré superlinear proof is beautiful but *knowing* L-BFGS converges superlinearly is enough — proving it is optional depth. |
| **N&W Ch. 8 (Calculating Derivatives)** | You have your own jvp/vjp autodiff; `objectives_test.cpp` already cross-checks forward vs reverse. §8.2 is a context-read at most. |
| **N&W Ch. 9 (Derivative-Free)** | You have exact gradients. The entire chapter answers a question you don't have. |
| **N&W Ch. 10 (Least-Squares), Ch. 11 (Nonlinear Equations)** | Gauss-Newton/LM are for sum-of-squares; logistic MLE is not least-squares. Newton for `F(x)=0` overlaps Newton-CG conceptually but isn't on any tick. Skip both. |
| **N&W Ch. 12–19 (all Constrained Optimization)** | Your regime is unconstrained, full stop. KKT, simplex, interior-point, SQP, augmented Lagrangian — pp. 304–597 (~290 pages), zero Arc C ticks. The single largest, cleanest cut. |
| **Bubeck, *Convex Optimization: Algorithms and Complexity*** | Worst-case complexity of black-box methods; no quasi-Newton, no Newton-CG, no Laplace. Optional context (see appendix), not path. |
| **Hager & Zhang 2005 (CG_DESCENT)** | Mistargeted: it's *nonlinear* CG (an outer-loop method), not the *linear* CG inside Newton-CG. Its line-search half duplicates Moré-Thuente. Revisit only if you ever build nonlinear CG as its own method — which the SKIP above says you won't. |
| **Bernstein & Newhouse 2024, Bottou-Curtis-Nocedal 2018** | Both belong to the stochastic/ML era. See appendix for triggers. |
| **Moré-Thuente 1994 (full read)** | *Partial* skip: the safeguarded-interpolation robustness matters, but N&W §3.5 Algorithm 3.5 is a sufficient strong-Wolfe implementation for small-n exact-gradient problems. Read Moré-Thuente only if your Stage-3 line search proves flaky feeding L-BFGS — it's the "harden for real use" read the Stage-4 payoff defers, and your regime may not need it. |

---

## APPENDIX — Optional context (stochastic-era & 2021–2026), with triggers

These are not on the path. Each has one honest moment where it *becomes* worth reading — noted as the trigger. Read none of them "to be complete"; read one when you hit its trigger.

| Resource | What it is | Trigger — read when… |
|---|---|---|
| **Distill "Why Momentum Really Works" (2017)** | Momentum as a damped oscillator; the condition-number mental model. | …you're in **Stage 2b** and want the eigenvalue intuition made visual. Light, parallel, ~1h. (Already noted inline.) |
| **Cohen et al. 2021, "Edge of Stability" (ICLR)** | GD's sharpness climbs to ≈2/η and *hovers* there, oscillating — not the monotone descent the L-smoothness bound predicts. | …just after **Stage 2**, once you have GD code to poke and you notice a step size that "should diverge by the bound I proved" doesn't, quite. A gut-punch to the Armijo worldview, best felt with working code. Optional-joy; nothing downstream needs it. |
| **Nocedal 1980, "Updating Quasi-Newton Matrices with Limited Storage"** | Where the two-loop recursion's algebra comes from, by unrolling BFGS. | …*after* **Stage 4** works. Pure reconstructive-understanding read (derive, don't implement — Liu & Nocedal 1989 is enough to build from). |
| **Garrigos & Gower 2023, "Handbook of Convergence Theorems"** | Clean, teachable convergence proofs with exact rate constants (deterministic-GD sections). | …if, during **Stage 2**, you want the rate constants rigorous rather than intuited. Read only the deterministic-gradient sections; skip everything stochastic/proximal. |
| **Bubeck 2015, "Convex Optimization: Algorithms and Complexity"** | Complexity-theoretic (Nesterov/Renegar) view: *why* rates are what they are, plus lower bounds. | …if you want to know GD's rate is not just achievable but *optimal* among black-box first-order methods. Cross-validates Boyd's Newton/convergence sections from a different angle. Ch. 1/3/4 only. |
| **Bernstein & Newhouse 2024, "Old Optimizer, New Norm"** | Adam/Shampoo reframed as steepest descent under non-Euclidean norms — Boyd §9.4's object, resurfacing at the 2024 frontier. | …after **Stage 2 or 4**, as a "the classical idea still drives frontier design" bookend. Read the intro + the steepest-descent-under-a-norm setup; stop there. Pure curiosity. |
| **Bottou, Curtis & Nocedal 2018, "Optimization Methods for Large-Scale ML"** | The survey bridging deterministic full-gradient methods → the stochastic setting ML runs in (SGD, variance reduction, mini-batch noise). | …at the **start of the next arc** (stochastic optimization / MCMC), *not now*. Every concept it surveys needs something noisy built first; reading it here is reading about noise before you've built anything noisy. |

**The through-line:** the entire optional stack is "what happens when gradients get noisy or the frontier moves." Your Arc C regime is deterministic and exact by design. Build the six-stage spine first; the appendix is what you reach for the day you leave that regime — which, per your spine, is the MCMC/HMC arc that Stage 6's Laplace bridge opens.
