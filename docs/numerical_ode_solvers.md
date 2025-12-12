# Numerical ODE Solvers: Euler and Runge-Kutta Methods

A deep, intuition-driven guide to solving ordinary differential equations numerically.

## The Core Problem

Given a differential equation:

$$\frac{dy}{dt} = f(t, y)$$

with initial condition $y(t_0) = y_0$, we want to compute $y(t)$ at future times.

**Why can't we just solve it analytically?** Most real-world ODEs don't have closed-form solutions. Even simple-looking equations like $y' = y^2 - t$ have no elementary solution.

---

## Part 1: Euler's Method — The Foundation

### The Key Insight

The derivative tells us the *instantaneous slope*. If we know where we are and which direction we're heading, we can take a small step forward:

```
y(t + h) ≈ y(t) + h · f(t, y(t))
```

This is just the tangent line approximation from calculus.

### Visual Intuition

Imagine walking through fog where you can only see your feet. At each step:
1. Look at the ground's slope (compute $f(t,y)$)
2. Take a small step in that direction (advance by $h$)
3. Repeat

The smaller your steps, the less you drift off course.

### Implementation

```cpp
// Simplest possible Euler solver
template <typename F, typename T>
auto eulerStep(F&& f, T t, auto y, T h) {
    return y + h * f(t, y);
}

// Full integration
template <typename F, typename T>
auto eulerSolve(F&& f, T t0, auto y0, T t_end, T h) {
    auto t = t0;
    auto y = y0;
    while (t < t_end) {
        y = eulerStep(f, t, y, h);
        t += h;
    }
    return y;
}
```

### Error Analysis

**Local truncation error** (error per step): $O(h^2)$

From Taylor expansion:
$$y(t+h) = y(t) + h \cdot y'(t) + \frac{h^2}{2} y''(t) + O(h^3)$$

Euler uses only the first two terms, so we lose the $\frac{h^2}{2} y''$ term.

**Global error** (accumulated over $N = (t_{end} - t_0)/h$ steps): $O(h)$

This is why Euler is called a **first-order method**.

### When Euler Fails Spectacularly

**Stiff equations**: Consider $y' = -1000y$. The true solution decays exponentially, but Euler with $h > 0.002$ will *oscillate wildly* and blow up. This is the **stability problem**.

**Stability condition for $y' = \lambda y$**: $|1 + h\lambda| < 1$

For $\lambda = -1000$, we need $h < 0.002$. Tiny steps = slow computation.

---

## Part 2: Runge-Kutta Methods — Smarter Stepping

### The Core Insight

Instead of using just the slope at the starting point, **sample the slope at multiple points** within the step and take a weighted average. This captures curvature information without computing second derivatives.

### RK2: The Midpoint Method

**Idea**: Take a trial step to the midpoint, evaluate the slope there, then use *that* slope for the full step.

```
k₁ = f(t, y)                    # Slope at start
k₂ = f(t + h/2, y + h·k₁/2)     # Slope at midpoint
y_new = y + h · k₂              # Full step using midpoint slope
```

**Why does this work?** The midpoint slope is a better estimate of the "average" slope over the interval. Taylor expansion shows this cancels the $h^2$ error term.

**Local error**: $O(h^3)$ → **Second-order method**

### RK4: The Workhorse

The classic fourth-order Runge-Kutta method samples four slopes:

```
k₁ = f(t, y)                        # Slope at start
k₂ = f(t + h/2, y + h·k₁/2)         # Slope at first midpoint
k₃ = f(t + h/2, y + h·k₂/2)         # Slope at second midpoint (using k₂)
k₄ = f(t + h, y + h·k₃)             # Slope at end

y_new = y + (h/6) · (k₁ + 2k₂ + 2k₃ + k₄)
```

**The weights (1, 2, 2, 1) / 6** come from Simpson's rule for integration. We're essentially integrating the slope function.

### Visual Intuition for RK4

Think of it as a committee decision:
1. **k₁**: "Here's where we're pointing now"
2. **k₂**: "If we follow k₁ halfway, here's the new direction"
3. **k₃**: "Actually, let's reconsider using k₂'s advice"
4. **k₄**: "And here's where we'd end up"

The weighted average gives midpoint estimates double weight (they're more representative of the interval).

### Implementation

```cpp
template <typename F, typename T, typename Y>
auto rk4Step(F&& f, T t, Y y, T h) {
    auto k1 = f(t, y);
    auto k2 = f(t + h/2, y + (h/2) * k1);
    auto k3 = f(t + h/2, y + (h/2) * k2);
    auto k4 = f(t + h, y + h * k3);

    return y + (h/T{6}) * (k1 + T{2}*k2 + T{2}*k3 + k4);
}
```

### Error Analysis

**Local error**: $O(h^5)$
**Global error**: $O(h^4)$

RK4 is a **fourth-order method**. Halving $h$ reduces error by factor of 16.

---

## Part 3: The Runge-Kutta Family

### General Form

An $s$-stage explicit Runge-Kutta method:

$$k_i = f\left(t + c_i h, \, y + h \sum_{j=1}^{i-1} a_{ij} k_j\right)$$

$$y_{n+1} = y_n + h \sum_{i=1}^{s} b_i k_i$$

The coefficients are organized in a **Butcher tableau**:

```
c₁ |
c₂ | a₂₁
c₃ | a₃₁  a₃₂
⋮  |  ⋮         ⋱
cₛ | aₛ₁  aₛ₂  ⋯  aₛ,ₛ₋₁
---+------------------------
   | b₁   b₂   ⋯  bₛ₋₁   bₛ
```

### RK4 Butcher Tableau

```
0   |
1/2 | 1/2
1/2 | 0    1/2
1   | 0    0    1
----+----------------
    | 1/6  1/3  1/3  1/6
```

### Order Conditions

To achieve order $p$, the coefficients must satisfy certain algebraic conditions derived from Taylor matching. For order 4, there are 8 conditions. Higher orders require exponentially more stages.

| Order | Minimum stages | Function evaluations |
|-------|---------------|---------------------|
| 1     | 1             | 1                   |
| 2     | 2             | 2                   |
| 3     | 3             | 3                   |
| 4     | 4             | 4                   |
| 5     | 6             | 6                   |
| 6     | 7             | 7                   |
| 7     | 9             | 9                   |
| 8     | 11            | 11                  |

**Note**: Beyond order 4, you need more stages than the order number. This is why RK4 is a "sweet spot."

---

## Part 4: Adaptive Step Size Control

### The Problem

Fixed step size is wasteful:
- Too small in smooth regions → slow
- Too large in steep regions → inaccurate

### The Solution: Embedded Methods

Compute two estimates of different orders using *shared* function evaluations:

- Higher order estimate: $\hat{y}$ (more accurate)
- Lower order estimate: $y$ (less accurate)

**Error estimate**: $\epsilon \approx |\hat{y} - y|$

### Dormand-Prince (DOPRI5)

The most popular adaptive RK method. Uses 7 stages to get:
- 5th order solution (for advancing)
- 4th order solution (for error estimation)

**Key property**: First Same As Last (FSAL) — the last $k$ of step $n$ is the first $k$ of step $n+1$. Effectively 6 evaluations per step.

### Step Size Controller

```cpp
template <typename T>
T computeNewStepSize(T h, T error, T tolerance, int order) {
    // Safety factor (typically 0.8-0.9)
    constexpr T safety = T{0.9};

    // Don't change step size too drastically
    constexpr T min_factor = T{0.2};
    constexpr T max_factor = T{5.0};

    if (error < T{1e-15}) {
        return h * max_factor;  // Error essentially zero, can grow
    }

    // Optimal step size ratio: (tolerance/error)^(1/(order+1))
    T factor = safety * std::pow(tolerance / error, T{1} / T{order + 1});
    factor = std::clamp(factor, min_factor, max_factor);

    return h * factor;
}
```

### PI Controller (Advanced)

Simple step size control can oscillate. A PI (proportional-integral) controller smooths this:

$$h_{new} = h \cdot \left(\frac{\epsilon_{n-1}}{\epsilon_n}\right)^{\beta_1} \cdot \left(\frac{tol}{\epsilon_n}\right)^{\beta_2}$$

Typical values: $\beta_1 = 0.7/p$, $\beta_2 = 0.4/p$ where $p$ is the method order.

---

## Part 5: Efficient Implementation Strategies

### 1. Minimize Memory Allocations

For systems of ODEs, $y$ is a vector. Avoid allocating new vectors for each $k_i$:

```cpp
template <size_t N, typename T>
struct RK4Workspace {
    std::array<T, N> k1, k2, k3, k4, temp;
};

// Reuse workspace across steps
template <size_t N, typename F, typename T>
void rk4StepInPlace(F&& f, T t, std::array<T, N>& y, T h,
                     RK4Workspace<N, T>& ws) {
    f(t, y, ws.k1);

    for (size_t i = 0; i < N; ++i)
        ws.temp[i] = y[i] + (h/2) * ws.k1[i];
    f(t + h/2, ws.temp, ws.k2);

    // ... etc
}
```

### 2. Expression Templates (Avoid Temporaries)

When $y$ is a vector type, naive `y + h * k` creates multiple temporaries:

```cpp
// Bad: creates 2 temporary vectors
auto result = y + h * k1;

// Better: fused operation
template <typename Y, typename K, typename T>
void axpy(Y& result, const Y& y, T a, const K& k) {
    for (size_t i = 0; i < y.size(); ++i)
        result[i] = y[i] + a * k[i];
}
```

Or use expression templates (like Eigen) to defer evaluation.

### 3. SIMD Vectorization

For large systems, process 4-8 components simultaneously:

```cpp
// Process 8 doubles at once with AVX-512
void rk4StepSIMD(double* y, const double* k1, const double* k2,
                  const double* k3, const double* k4,
                  double h, size_t n) {
    __m512d h6 = _mm512_set1_pd(h / 6.0);
    __m512d two = _mm512_set1_pd(2.0);

    for (size_t i = 0; i < n; i += 8) {
        __m512d vy  = _mm512_load_pd(y + i);
        __m512d vk1 = _mm512_load_pd(k1 + i);
        __m512d vk2 = _mm512_load_pd(k2 + i);
        __m512d vk3 = _mm512_load_pd(k3 + i);
        __m512d vk4 = _mm512_load_pd(k4 + i);

        // k1 + 2*k2 + 2*k3 + k4
        __m512d sum = _mm512_add_pd(vk1, vk4);
        sum = _mm512_fmadd_pd(two, vk2, sum);
        sum = _mm512_fmadd_pd(two, vk3, sum);

        // y + (h/6) * sum
        vy = _mm512_fmadd_pd(h6, sum, vy);
        _mm512_store_pd(y + i, vy);
    }
}
```

### 4. Dense Output (Interpolation)

When you need $y$ at arbitrary times (not just step boundaries), compute a polynomial interpolant:

```cpp
// Hermite cubic interpolation between steps
template <typename T, typename Y>
Y interpolate(T theta, const Y& y0, const Y& y1,
              const Y& f0, const Y& f1, T h) {
    // theta ∈ [0, 1] is fractional position in step
    T theta2 = theta * theta;
    T theta3 = theta2 * theta;

    // Hermite basis functions
    T h00 = 2*theta3 - 3*theta2 + 1;
    T h10 = theta3 - 2*theta2 + theta;
    T h01 = -2*theta3 + 3*theta2;
    T h11 = theta3 - theta2;

    return h00*y0 + h*h10*f0 + h01*y1 + h*h11*f1;
}
```

### 5. Event Detection

For problems like "when does the ball hit the ground?", use root-finding on the dense output:

```cpp
template <typename EventFunc, typename T>
std::optional<T> detectEvent(EventFunc&& g, T t0, T t1,
                              const auto& interpolate) {
    T g0 = g(t0, interpolate(0));
    T g1 = g(t1, interpolate(1));

    if (g0 * g1 >= 0) return std::nullopt;  // No sign change

    // Bisection to find root
    while (t1 - t0 > 1e-10) {
        T mid = (t0 + t1) / 2;
        T theta = (mid - t0) / (t1 - t0);
        T gmid = g(mid, interpolate(theta));

        if (g0 * gmid < 0) { t1 = mid; g1 = gmid; }
        else               { t0 = mid; g0 = gmid; }
    }
    return (t0 + t1) / 2;
}
```

### 6. Parallel-Across-Method

When solving many independent ODEs, parallelize across problems:

```cpp
// Solve N independent systems in parallel
template <typename F, typename T>
void batchRK4(F&& f, const std::vector<T>& t0,
              std::vector<std::vector<T>>& y,
              T t_end, T h) {
    #pragma omp parallel for
    for (size_t i = 0; i < y.size(); ++i) {
        // Each thread has its own workspace
        solveSingleODE(f, t0[i], y[i], t_end, h);
    }
}
```

---

## Part 6: Handling Stiff Equations

### What is Stiffness?

A system is **stiff** when:
- It has widely separated time scales
- Some components decay much faster than others
- Explicit methods require impractically small steps for stability

**Example**: Chemical reaction with fast and slow species:
```
A → B    (fast, rate = 1000)
B → C    (slow, rate = 1)
```

### Implicit Methods

Instead of $y_{n+1} = y_n + h \cdot f(t_n, y_n)$ (explicit Euler), use:

$$y_{n+1} = y_n + h \cdot f(t_{n+1}, y_{n+1})$$ (implicit Euler)

This requires solving a (possibly nonlinear) equation at each step, but has **unconditional stability**.

### Implicit Runge-Kutta

The $a_{ij}$ coefficients can depend on $j \geq i$, requiring implicit solves. The simplest is:

**Implicit Midpoint**:
$$y_{n+1} = y_n + h \cdot f\left(t_n + \frac{h}{2}, \frac{y_n + y_{n+1}}{2}\right)$$

### When to Use What

| Problem Type | Recommended Method |
|--------------|-------------------|
| Non-stiff, low accuracy | RK4 |
| Non-stiff, high accuracy | DOPRI5, RK8(7) |
| Non-stiff, very high accuracy | Bulirsch-Stoer |
| Mildly stiff | Rosenbrock methods |
| Very stiff | RADAU5, BDF methods |
| Hamiltonian systems | Symplectic integrators |

---

## Part 7: Systems of ODEs

### First-Order Systems

Most real problems involve systems:

$$\frac{d\mathbf{y}}{dt} = \mathbf{f}(t, \mathbf{y})$$

where $\mathbf{y} = (y_1, y_2, \ldots, y_n)^T$.

**The same RK formulas apply** — just use vector operations.

### Higher-Order ODEs

Convert to first-order systems. For $y'' = g(t, y, y')$:

Let $v = y'$, then:
$$\frac{d}{dt}\begin{pmatrix} y \\ v \end{pmatrix} = \begin{pmatrix} v \\ g(t, y, v) \end{pmatrix}$$

**Example**: Pendulum $\theta'' = -\frac{g}{L}\sin\theta$

```cpp
void pendulum(double t, const std::array<double, 2>& state,
              std::array<double, 2>& deriv) {
    double theta = state[0];
    double omega = state[1];

    deriv[0] = omega;                    // dθ/dt = ω
    deriv[1] = -(g/L) * std::sin(theta); // dω/dt = -(g/L)sin(θ)
}
```

---

## Part 8: Choosing Step Size and Tolerance

### Rules of Thumb

1. **Start with $h = (t_{end} - t_0) / 100$** and see if results are stable

2. **Richardson extrapolation** to estimate error:
   - Solve with step $h$, get $y_h$
   - Solve with step $h/2$, get $y_{h/2}$
   - For order $p$ method: error $\approx \frac{y_{h/2} - y_h}{2^p - 1}$

3. **Conservation laws**: If your system conserves energy/momentum, check it:
   ```cpp
   double energy = computeEnergy(y);
   // Energy drift indicates accuracy issues
   ```

### Tolerance Selection

For adaptive methods:
- **Relative tolerance** (`rtol`): Controls relative error $|y - \hat{y}|/|y|$
- **Absolute tolerance** (`atol`): Controls absolute error when $y \approx 0$

Combined: error $<$ `atol` + `rtol` × $|y|$

Typical values:
- `rtol = 1e-6`, `atol = 1e-9` for general work
- `rtol = 1e-12`, `atol = 1e-14` for high precision

---

## Complete Example: Lorenz Attractor

```cpp
#include <array>
#include <cmath>
#include <print>

// Lorenz system parameters
constexpr double sigma = 10.0;
constexpr double rho = 28.0;
constexpr double beta = 8.0 / 3.0;

using State = std::array<double, 3>;

void lorenz(double /*t*/, const State& y, State& dydt) {
    dydt[0] = sigma * (y[1] - y[0]);
    dydt[1] = y[0] * (rho - y[2]) - y[1];
    dydt[2] = y[0] * y[1] - beta * y[2];
}

template <typename F>
State rk4Step(F&& f, double t, const State& y, double h) {
    State k1, k2, k3, k4, temp, result;

    f(t, y, k1);

    for (int i = 0; i < 3; ++i) temp[i] = y[i] + 0.5*h*k1[i];
    f(t + 0.5*h, temp, k2);

    for (int i = 0; i < 3; ++i) temp[i] = y[i] + 0.5*h*k2[i];
    f(t + 0.5*h, temp, k3);

    for (int i = 0; i < 3; ++i) temp[i] = y[i] + h*k3[i];
    f(t + h, temp, k4);

    for (int i = 0; i < 3; ++i)
        result[i] = y[i] + (h/6.0) * (k1[i] + 2*k2[i] + 2*k3[i] + k4[i]);

    return result;
}

int main() {
    State y = {1.0, 1.0, 1.0};  // Initial conditions
    double t = 0.0;
    double h = 0.01;
    double t_end = 50.0;

    while (t < t_end) {
        std::println("{:.4f}: ({:.6f}, {:.6f}, {:.6f})", t, y[0], y[1], y[2]);
        y = rk4Step(lorenz, t, y, h);
        t += h;
    }
}
```

---

## References

### Textbooks

1. **Hairer, Nørsett, Wanner** — *Solving Ordinary Differential Equations I: Nonstiff Problems* (Springer, 1993)
   - The definitive reference. Chapters 2-3 cover RK methods exhaustively.

2. **Hairer & Wanner** — *Solving Ordinary Differential Equations II: Stiff and Differential-Algebraic Problems* (Springer, 1996)
   - Essential for stiff systems.

3. **Press et al.** — *Numerical Recipes* (Cambridge, 3rd ed.)
   - Chapter 17. Practical with code examples.

4. **Butcher, J.C.** — *Numerical Methods for Ordinary Differential Equations* (Wiley, 2016)
   - By the inventor of Butcher tableaux. Theoretical depth.

### Papers

5. **Dormand & Prince (1980)** — "A family of embedded Runge-Kutta formulae"
   - Original DOPRI5 paper. J. Comp. Appl. Math. 6:19-26.

6. **Shampine (1986)** — "Some Practical Runge-Kutta Formulas"
   - Practical guidance on implementation. Math. Comp. 46:135-150.

7. **Söderlind (2002)** — "Automatic Control and Adaptive Time-Stepping"
   - PI controllers for step size. Numer. Algorithms 31:281-310.

### Online Resources

8. **Scholarpedia: Runge-Kutta methods**
   https://www.scholarpedia.org/article/Runge-Kutta_methods
   - Written by Butcher himself.

9. **Ernst Hairer's homepage**
   https://www.unige.ch/~hairer/
   - FORTRAN codes for all major methods (DOPRI5, RADAU5, etc.)

10. **MIT OCW 18.337 — Parallel Computing and Scientific Machine Learning**
    https://github.com/mitmath/18337
    - Julia-based, excellent lecture notes on ODE solvers.

---

## Summary: Key Takeaways

| Concept | Intuition |
|---------|-----------|
| Euler | Follow the tangent line (linear approximation) |
| RK methods | Average slopes from multiple sample points |
| Order $p$ | Halving $h$ reduces error by $2^p$ |
| Adaptive | Let the method choose $h$ based on local error |
| Stiffness | Fast decays need implicit methods |
| FSAL | Reuse last $k$ as first $k$ of next step |

**The hierarchy**:
- Euler: Teaching tool, rarely used in practice
- RK4: Workhorse for non-stiff problems
- DOPRI5: Standard adaptive method
- RADAU5/BDF: Stiff problems

**For most problems**: Start with an adaptive RK4/5 method. Switch to implicit methods only if step size gets impractically small.
