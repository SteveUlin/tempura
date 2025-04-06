# tempura

tempura is an experimental C++26 library focused on numerical methods with
a strong emphasis on constexpr templated functions.

Notable features include:
- Compile time symbolic math via template meta-programming
- Automatic differentiation
- Matrix and vector math
- Optimization algorithms
- Numerical Quadrature (Integration)
- Bayesian Statistics

tempura exists mostly as a playground for implementation of numerical
methods new C++ features.

A lot of this code is based on my read through of Numerical Recipes 3rd Ed.
Although the mathematical ideas originate from Numerical Recipes all of the
code is my own implementation. I highly recommend reading the book if you
interested in understanding the foundations of numerical methods, but a lot
of the content is outdated. For examples, Numerical Recipes focuses on single
threaded methods but most modern mathematics focuses on GPU and multi-threaded
algorithms.

I am not much of a fan of Numerical Recipes coding style, so I have
substituted my own. It is more or less the Google Style Guide + clang
modernize. I make heavy use of unicode in the code so make sure you have
a font that can read: α² + β² = ½

## Design Decisions

- Correct is better than fast

  Write unit tests and assert statements when possible. The goal is
  understanding of the methods, not to micro optimize every function.

- Fast is better than slow

  Code should be algorithmically efficient and well reasoned. Just not micro
  optimized. Look for orders of magnitude improvements, not nanoseconds.

- constexpr by default

  Functions and classes should offer some sort constexpr version. No calculation
  at runtime is the fastest calculation.

- Complexity is fine if the end api is simple

  Notably, the compile time symbolic math has some complex implementation
  details, but ends with a nice-ish api. In addition, most algorithms operate
  over generic containers, tuples, and arrays. Not assuming that you want
  to pass in doubles or use tempura::matrix3 objects.

# Wishlist
- [ ] Simple logging library - (move matrix::check macro)
- [x] MatMul

- [ ] Rework Benchmark Lib
- [x] Guarded (mutex wrapper)
- [ ] Lock Free FIFO Queue
- [ ] Compile Time Strings

- [ ] Coroutine Examples
- [ ] Rational Numbers Examples

# Symbolic
- [x] Fix tests
- [x] Exponents
- [x] Derivatives
- [x] Create it's own directory
