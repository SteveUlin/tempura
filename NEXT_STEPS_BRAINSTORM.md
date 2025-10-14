# Tempura Next Steps: Strategic Roadmap

**Date**: October 13, 2025
**Status**: Post-Documentation Cleanup & Code Review Phase
**Current State**: Symbolic3 core complete, well-documented, and stable

---

## Current State Assessment

### ✅ Completed & Stable

- **Symbolic3 Core**: Combinator-based symbolic computation with compile-time evaluation
- **Pattern Matching**: Comprehensive rewrite system with variable capture
- **Simplification**: Two-stage pipeline with oscillation prevention
- **Exact Arithmetic**: Fraction support for precise rational arithmetic
- **Transcendental Functions**: exp, log, sin, cos, tan + hyperbolic variants
- **Documentation**: Comprehensive inline comments, oscillation prevention tests
- **Matrix2**: Constexpr matrix library with static/dynamic extents
- **Meta**: Value-based template metaprogramming utilities

### 🔧 Partially Implemented

- **Autodiff**: Basic dual numbers and computational graphs exist
- **Factoring**: Simple term collection works, but could be more sophisticated
- **Distribution**: Intentionally disabled to prevent oscillation (needs predicates)
- **Canonical Forms**: Basic ordering works, but not always unique

### ❌ Not Started / Ideas Only

- **Equation Solving**: Symbolic equation solver
- **Integration**: Symbolic integration (Risch algorithm?)
- **Limits**: Limit computation
- **Series Expansion**: Taylor/Laurent series
- **Advanced Simplification**: Trigonometric identities with predicates

---

## Strategic Direction: Three Paths Forward

### Path 1: **Symbolic Math Maturity** 🎯 (Recommended)

**Goal**: Make symbolic3 a production-ready symbolic math library

**Philosophy**: "Depth over breadth" - perfect what we have before adding more

#### Phase 1: Simplification Excellence (2-3 weeks)

1. **Conditional Distribution Rules**

   - Add predicates that only distribute when it reduces complexity
   - Example: `x*(2+3)` → distribute and fold to `5*x`
   - Example: `x*(a+b)` → keep factored (no simplification)
   - Implement complexity metrics (expression depth, node count)

2. **Enhanced Canonical Forms**

   - Ensure all equivalent expressions reach identical forms
   - Fix associativity normalization issues found in tests
   - Add comprehensive canonical form test suite
   - Document canonical form guarantees

3. **Advanced Factoring**

   - Factor out common subexpressions: `x*a + x*b + y*a + y*b` → `(x+y)*(a+b)`
   - Greatest Common Divisor factoring for polynomials
   - Partial fraction decomposition
   - Factor by grouping strategies

4. **Trigonometric Sophistication**

   - Conditional Pythagorean identity expansion
   - Sum/difference angle formulas: `sin(a±b)`, `cos(a±b)`
   - Double/half angle formulas
   - Product-to-sum and sum-to-product conversions
   - Complexity-aware application (only simplify, never complicate)

5. **Performance Optimization**
   - Profile compile-time performance
   - Optimize BindingContext lookup (binary search?)
   - Reduce template instantiation depth
   - Add compilation benchmarks

**Deliverables**:

- Robust simplification that rivals Mathematica/SymPy for basic algebra
- Comprehensive test suite (>1000 test cases)
- Performance benchmarks showing compile-time costs
- User guide with worked examples

**Risk**: Could spend months perfecting simplification without new features

---

#### Phase 2: Calculus Foundation (3-4 weeks)

1. **Symbolic Differentiation** (Extend existing autodiff)

   - Already have basic derivative rules
   - Add chain rule for nested functions
   - Add product rule, quotient rule
   - Implicit differentiation
   - Partial derivatives for multivariable

2. **Symbolic Integration**

   - Start with power rule, exponential, trig integrals
   - U-substitution recognition
   - Integration by parts heuristics
   - Table lookup for common integrals
   - (Advanced: Risch algorithm for elementary functions)

3. **Limits**

   - L'Hôpital's rule
   - Limit of rational functions
   - Squeeze theorem application
   - One-sided limits

4. **Series Expansion**
   - Taylor series to arbitrary order
   - Maclaurin series
   - Laurent series (for poles)
   - Automatic series composition

**Deliverables**:

- Working calculus toolkit comparable to early CAS systems
- Integration test suite from calculus textbooks
- Examples solving common calculus problems

**Risk**: Integration is notoriously difficult - may need to punt on general case

---

#### Phase 3: Equation Solving (2-3 weeks)

1. **Algebraic Equations**

   - Linear equations: `ax + b = 0`
   - Quadratic equations: `ax² + bx + c = 0`
   - Higher-order polynomials (numerical + symbolic)
   - Systems of linear equations (matrix methods)

2. **Transcendental Equations**

   - Isolate variable heuristics
   - Lambert W function for `x*e^x = c`
   - Numerical solving with symbolic derivatives

3. **Inequalities**
   - Interval arithmetic
   - Sign analysis
   - Critical point identification

**Deliverables**:

- `solve(equation, variable)` function
- System solver for linear systems
- Inequality solver for rational functions

**Risk**: General equation solving is undecidable - need good heuristics

---

### Path 2: **Numerical Methods Integration** 🔬

**Goal**: Bridge symbolic and numerical computation

**Philosophy**: "Best of both worlds" - symbolic setup, numerical execution

#### Components:

1. **Symbolic → Numerical Pipeline**

   - JIT compilation of symbolic expressions to machine code
   - Generate optimized C++ functions from symbolic
   - SIMD vectorization of symbolic computations
   - GPU kernel generation (CUDA/Vulkan)

2. **Hybrid Optimization**

   - Symbolic differentiation for gradients
   - Numerical optimization (L-BFGS, conjugate gradient)
   - Automatic Jacobian/Hessian computation
   - Use existing `optimization/` directory

3. **Numerical Analysis**

   - Symbolic error analysis
   - Stability analysis of numerical schemes
   - Automatic precision estimation

4. **Matrix Integration**
   - Symbolic matrix operations using matrix2
   - Eigenvalue computation (symbolic + numerical)
   - Matrix decompositions with symbolic elements

**Deliverables**:

- Fast numerical evaluation of symbolic expressions
- Optimization library using symbolic gradients
- Hybrid symbolic-numeric PDE solver

**Advantages**:

- Leverages existing optimization/quadrature code
- Practical applications immediately
- Differentiates from pure symbolic libraries

**Risks**:

- Dilutes focus from symbolic core
- JIT compilation is complex (LLVM integration?)
- May duplicate existing tools (JAX, PyTorch)

---

### Path 3: **Domain-Specific Applications** 🎓

**Goal**: Apply symbolic3 to specific problem domains

**Philosophy**: "Prove the value" - show real-world utility

#### Potential Domains:

**A. Quantum Computing Simulation**

- Symbolic manipulation of quantum states
- Circuit optimization using symbolic rewrites
- Hamiltonian simplification
- Uses tensor algebra (related to matrix2)

**B. Physics Engine**

- Symbolic Lagrangian/Hamiltonian mechanics
- Automatic equation of motion derivation
- Conservation law verification
- Constraint-based dynamics

**C. Control Theory**

- Transfer function simplification
- State-space representation
- Stability analysis (symbolic Routh-Hurwitz)
- Controller design

**D. Computer Algebra for Education**

- Interactive math problem solver
- Step-by-step solution display
- Automatic problem generation
- Visualization of transformations

**E. Compiler Optimizations**

- Loop optimization using symbolic analysis
- Symbolic execution for program verification
- Automatic parallelization via dependency analysis

**Deliverables** (choose one domain):

- Fully-featured domain-specific library
- Research paper demonstrating novel capabilities
- Benchmarks vs existing tools

**Advantages**:

- Concrete value demonstration
- Potential research publications
- Real users and feedback

**Risks**:

- Requires domain expertise
- May expose limitations of symbolic3
- Could become maintenance burden

---

## Recommended Immediate Actions (Next 2 Weeks)

### 1. Fix Known Issues from Oscillation Tests

Several tests were commented out due to unimplemented features:

- **Hyperbolic identity simplification**: `cosh²(x) - sinh²(x)` should → `1`
- **Subtraction simplification**: `x - (-x)` should → `2x`
- **Fraction-constant conversion**: `Fraction<6,1>` should → `Constant<6>`
- **Associativity normalization**: Different associations should reach same form

**Time estimate**: 3-5 days
**Impact**: High - these are basic simplifications users expect

---

### 2. Add Complexity Metrics

Implement objective measures of expression complexity:

```cpp
template<Symbolic S>
constexpr SizeT complexity(S expr) {
  // Options:
  // 1. Node count (size of expression tree)
  // 2. Depth (maximum nesting level)
  // 3. Operation cost (weighted by operation type)
  // 4. Custom metric (domain-specific)
}
```

Use this to guide simplification:

```cpp
// Only apply rule if it reduces complexity
Rewrite{
  pattern, replacement,
  [](auto ctx) {
    auto before = complexity(get_matched_expr(ctx));
    auto after = complexity(get_replacement(ctx));
    return after < before;
  }
}
```

**Time estimate**: 2-3 days
**Impact**: High - enables conditional simplification rules

---

### 3. Implement Conditional Distribution

```cpp
// Distribute only if it simplifies
constexpr auto smart_distribution = Rewrite{
  x_ * (a_ + b_),
  x_ * a_ + x_ * b_,
  [](auto ctx) {
    auto factored = get(ctx, x_) * (get(ctx, a_) + get(ctx, b_));
    auto distributed = get(ctx, x_) * get(ctx, a_) + get(ctx, x_) * get(ctx, b_);

    // Distribute if:
    // 1. Distribution enables constant folding
    // 2. Distribution exposes cancellation
    // 3. Overall complexity reduces
    return should_distribute(factored, distributed);
  }
};
```

**Time estimate**: 4-5 days
**Impact**: Medium-High - resolves distribution/factoring oscillation elegantly

---

### 4. Documentation: Comprehensive User Guide

Create `src/symbolic3/USER_GUIDE.md` with:

- Getting started tutorial
- Common patterns and idioms
- Performance considerations
- Troubleshooting compile errors
- Examples from calculus, physics, engineering
- Comparison with other CAS systems

**Time estimate**: 3-4 days
**Impact**: Medium - makes library accessible to new users

---

### 5. Benchmarking Infrastructure

Create `src/symbolic3/benchmarks/` with:

- Compile-time benchmarks (template instantiation depth, time)
- Runtime evaluation benchmarks
- Comparison with SymPy, Mathematica (via interface)
- Memory usage tracking
- Regression detection

**Time estimate**: 3-4 days
**Impact**: Medium - prevents performance regressions

---

## Medium-Term Goals (1-3 Months)

### Symbolic Math Path (Recommended)

1. **Month 1**: Fix known issues + complexity metrics + conditional distribution
2. **Month 2**: Advanced factoring + enhanced canonical forms
3. **Month 3**: Trigonometric sophistication + performance optimization

**Expected Outcome**: World-class compile-time symbolic algebra library

### Numerical Integration Path (Alternative)

1. **Month 1**: Symbolic → numerical code generation
2. **Month 2**: Hybrid optimization using autodiff
3. **Month 3**: Benchmarks vs JAX/PyTorch

**Expected Outcome**: Unique symbolic-numerical hybrid tool

### Domain Application Path (Alternative)

1. **Month 1**: Choose domain + prototype
2. **Month 2**: Full implementation
3. **Month 3**: Paper + benchmarks

**Expected Outcome**: Published research demonstrating novel capabilities

---

## Long-Term Vision (6-12 Months)

### Vision A: **Compile-Time CAS**

"The world's first production-ready compile-time computer algebra system"

- Rival Mathematica/SymPy for core algebra
- All computation at compile-time (zero runtime cost)
- Template error messages as good as runtime errors
- Integrated with C++26 constexpr ecosystem

**Market**: C++ developers needing symbolic math without Python dependencies

---

### Vision B: **C++ Scientific Computing Stack**

"The SciPy/NumPy of C++26"

- Symbolic3: symbolic algebra
- Matrix2: linear algebra
- Autodiff: automatic differentiation
- Optimization: numerical optimization
- Quadrature: numerical integration
- Special: special functions
- Bayes: statistical inference

**Market**: High-performance scientific computing in pure C++

---

### Vision C: **Research Testbed**

"Exploring the limits of compile-time computation"

- Push boundaries of what's possible with constexpr
- Novel algorithms for compile-time evaluation
- Publications in PL and symbolic computation venues
- Influence C++ standard (constexpr improvements)

**Market**: Programming language researchers, C++ standards committee

---

## Open Questions for Decision

1. **Primary user persona?**

   - Academic researchers?
   - Industry HPC developers?
   - C++ library authors?
   - Students learning symbolic math?

2. **Performance priority?**

   - Compile-time speed (fast builds)?
   - Runtime evaluation speed (fast execution)?
   - Memory usage (template bloat)?
   - Development velocity (rapid prototyping)?

3. **Feature breadth vs depth?**

   - Many basic features (CAS feature parity)?
   - Few perfect features (world-class core)?
   - Novel capabilities (research-driven)?

4. **Integration strategy?**

   - Standalone library?
   - Part of larger scientific stack?
   - Header-only for easy adoption?
   - Requires build system integration?

5. **Success criteria?**
   - GitHub stars / community adoption?
   - Research publications?
   - Production use in industry?
   - C++ standard influence?

---

## Recommendation: Start with Path 1 (Symbolic Math Maturity)

**Rationale**:

1. **Foundation first**: Core algebra must be rock-solid before building on it
2. **Clear value**: Developers immediately understand "symbolic math in C++"
3. **Manageable scope**: Can make meaningful progress in weeks
4. **Extensible**: Good algebra enables calculus, solving, optimization
5. **Differentiating**: Compile-time CAS is genuinely novel

**Next Sprint (2 weeks)**:

- [ ] Fix oscillation test failures (3 days)
- [ ] Implement complexity metrics (2 days)
- [ ] Add conditional distribution (4 days)
- [ ] Write user guide chapter 1 (2 days)
- [ ] Set up benchmarking infrastructure (2 days)

**Success Metrics**:

- All oscillation tests passing
- 95%+ canonical form convergence
- User guide covers basics
- Compile-time benchmarks tracked

---

## Additional Considerations

### Testing Philosophy

- **Current**: Good coverage of happy paths
- **Needed**: Edge cases, error conditions, compile-time error messages
- **Goal**: 90%+ branch coverage, comprehensive property-based tests

### API Stability

- **Current**: Experimental, breaking changes acceptable
- **Transition**: Semantic versioning, deprecation warnings
- **Goal**: Stable 1.0 API with compatibility guarantees

### Community Building

- **Blog posts**: Explaining compile-time symbolic math
- **Conference talks**: CppCon, C++Now, PLDI
- **Documentation**: High-quality examples and tutorials
- **Engagement**: Respond to issues, accept PRs, build community

### Tooling

- **Visualization**: Expression tree viewer
- **Debugging**: Better compile-time error messages
- **IDE support**: Hover information, autocomplete
- **REPL**: Interactive symbolic math shell

---

## Conclusion

Tempura is at an exciting inflection point. The core symbolic3 library is solid, well-documented, and stable. The question is: **what should it become?**

**My recommendation**: Double down on symbolic math maturity (Path 1). Perfect the algebra, add calculus, and build a reputation as the definitive compile-time CAS. Once that foundation is solid, branching into numerical methods (Path 2) or domain applications (Path 3) becomes much easier.

The key insight: **constraint breeds creativity**. By focusing narrowly on symbolic algebra and doing it exceptionally well, Tempura can establish a unique niche in the C++ ecosystem.

---

**Next step**: Review this document, pick a path, and define the next 2-week sprint!
