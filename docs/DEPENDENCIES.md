# Tempura Library Dependencies

This document describes the dependency architecture of the Tempura C++26 numerical methods library.

## Overview

Tempura is organized into a layered architecture with explicit dependencies between components. All dependencies are declared through CMake's `target_link_libraries()` mechanism, ensuring that users must explicitly link against what they use (no hidden transitive dependencies).

## Dependency Principles

1. **Explicit is Better Than Implicit**: Every library explicitly declares its dependencies
2. **No Circular Dependencies**: The dependency graph is a directed acyclic graph (DAG)
3. **Minimal Coupling**: Libraries depend only on what they actually use
4. **Layered Architecture**: Dependencies flow from high-level to low-level components

## Dependency Layers

### Layer 0: Core Infrastructure (No Dependencies)

These libraries have no dependencies on other Tempura components:

- **meta**: Template metaprogramming utilities
  - Value-based metaprogramming with `Meta<T>` type IDs
  - Type lists, type sorting, compile-time utilities
  - Used by: `symbolic3`, `bayes`

- **simd**: SIMD vectorization primitives
  - AVX-512 intrinsics wrappers
  - Used by: `bayes`

- **matrix2**: Constexpr-first matrix library
  - Static and dynamic matrices
  - Storage abstractions (Dense, Banded, etc.)
  - Used by: `optimization`

- **utility**: General utility functions
  - Overloaded visitor pattern
  - No current dependencies

- **synchronization**: Thread-safe data structures
  - Guarded types, hierarchical mutexes
  - Thread-safe queues, stacks, thread pools
  - No current dependencies

### Layer 1: Foundation Libraries (Minimal Dependencies)

Top-level single-header libraries with no Tempura dependencies:

- **broadcast_array**: Array broadcasting operations
- **sequence**: Sequence generators
- **interpolate**: Polynomial interpolation
- **polynomial**: Polynomial operations
- **json**: JSON parsing/serialization
- **chebyshev**: Chebyshev approximations
- **root_finding**: Root-finding algorithms
- **image**: Image processing utilities
- **unit**: Testing framework
- **benchmark**: Benchmarking utilities
- **profiler**: Performance profiling (shared library)

### Layer 2: Mid-Level Libraries

Libraries that depend on Layer 0 and Layer 1:

- **symbolic3**: Combinator-based symbolic computation
  - **Dependencies**: `meta`
  - Unique type identity via stateless lambdas
  - Zero-runtime-overhead compile-time symbolic computation
  - Pattern matching and rewrite system

- **quadature**: Numerical integration methods
  - **Dependencies**: `broadcast_array`, `interpolate`
  - Gaussian quadrature, Newton-Cotes formulas
  - Monte Carlo integration, improper integrals

- **math**: Mathematical functions (sin, cos, exp)
  - **Dependencies**: None
  - Individual libraries for each function

### Layer 3: High-Level Libraries

Libraries that depend on multiple lower layers:

- **special**: Special mathematical functions
  - **Dependencies**: `quadature`, `sequence`
  - Gamma function, error function
  - Uses numerical integration for computation

- **optimization**: Numerical optimization algorithms
  - **Dependencies**: `matrix2`
  - Bracket method, downhill simplex (Nelder-Mead)
  - Multi-dimensional optimization

- **bayes**: Probability distributions and statistical inference
  - **Dependencies**: `broadcast_array`, `meta`, `simd`, `special`
  - Probability distributions (Normal, Beta, Gamma, etc.)
  - Random number generation with SIMD support
  - Statistical inference

## Dependency Graph

### Visual Representation

A GraphViz visualization is available at `docs/dependencies.dot`. Generate a PNG/SVG with:

```bash
dot -Tpng docs/dependencies.dot -o docs/dependencies.png
dot -Tsvg docs/dependencies.dot -o docs/dependencies.svg
```

### Text Representation

```
Layer 3 (High-level):
  bayes → broadcast_array, meta, simd, special
  optimization → matrix2
  special → quadature, sequence

Layer 2 (Mid-level):
  symbolic3 → meta
  quadature → broadcast_array, interpolate

Layer 1 (Foundation):
  broadcast_array, sequence, interpolate, polynomial, json,
  chebyshev, root_finding, image, unit, benchmark, profiler

Layer 0 (Core Infrastructure):
  meta, simd, matrix2, utility, synchronization
```

## Building Individual Components

With explicit dependencies, you can build individual component tests:

```bash
# Build and test the bayes library
ninja -C build bayes_normal_test
ctest --test-dir build -R bayes

# Build and test symbolic3 library
ninja -C build symbolic3_basic_test
ctest --test-dir build -R symbolic3

# Build and test optimization library
ninja -C build optimization_bracket_method_test
ctest --test-dir build -R optimization
```

Note: The libraries themselves are INTERFACE targets (header-only), so there are no compiled artifacts. The `tempura::*` aliases are for CMake dependency linking, not ninja build targets.

## Dependency Rationale

### Why does bayes depend on special?

The `bayes` library provides statistical distributions that require special mathematical functions:
- Gamma distribution uses the gamma function (`special/gamma.h`)
- Normal distribution uses the error function (`special/erf.h`)

### Why does special depend on quadature?

Special functions often require numerical integration:
- Gamma function uses quadrature for certain parameter ranges
- Incomplete gamma functions use numerical integration

### Why does optimization depend on matrix2?

Optimization algorithms operate on multi-dimensional spaces:
- Downhill simplex requires matrix operations for simplex vertices
- Multi-dimensional optimization uses matrix-vector operations

### Why does symbolic3 depend on meta?

Symbolic computation requires compile-time type manipulation:
- Unique symbol identity via `Meta<T>` type IDs
- Type list operations for expression trees
- Compile-time sorting and searching

## Avoiding Circular Dependencies

The dependency graph is carefully designed to avoid cycles:

- ✅ `bayes → special → quadature` (acyclic chain)
- ✅ `optimization → matrix2` (direct dependency)
- ✅ `symbolic3 → meta` (direct dependency)

No component depends on `bayes`, `optimization`, or any Layer 3 library, preventing cycles.

## Future Considerations

### Potential Refactorings

1. **Split special into smaller libraries**: `gamma`, `erf`, `bessel` could be separate
2. **Extract RNG from bayes**: Random number generation could be a separate library
3. **Add explicit matrix2 algorithm libraries**: LU decomposition, eigenvalues, etc.

### Adding New Dependencies

When adding a new dependency:

1. Update the component's `CMakeLists.txt` with `target_link_libraries()`
2. Add a dependency comment explaining why it's needed
3. Update this document's dependency graph
4. Verify no circular dependencies: `cmake --graphviz=docs/dependencies.dot build`
5. Run the full build: `ninja -C build`

## Implementation Details

### CMake Configuration

All libraries use the `INTERFACE` library pattern (header-only):

```cmake
add_library(component INTERFACE)
target_include_directories(component INTERFACE
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(component INTERFACE
    tempura::dependency1
    tempura::dependency2
    tempura_flags
)
add_library(tempura::component ALIAS component)
```

### Compiler Flags

All libraries inherit from `tempura_flags`, which provides:
- C++26 standard (`-std=c++2c`)
- AVX-512 support (`-mavx512f`, `-mavx512dq`)
- Template depth limits (`-ftemplate-depth=2048`)
- Release optimizations (`-O3`, `-march=native`, `-ffast-math`)

### Include Paths

All includes use the pattern: `#include "component/header.h"`

Example:
```cpp
#include "symbolic3/symbolic3.h"  // Correct
#include "symbolic3.h"            // Incorrect
```

CMake sets `src/` as the include root, making this pattern consistent.

## Testing

Tests link against the libraries they test:

```cmake
add_executable(component_test test.cpp)
target_link_libraries(component_test PRIVATE component unit)
add_test(NAME component_test COMMAND component_test)
```

The `unit` library provides the testing framework and is available to all tests.

## Summary

Tempura's dependency architecture follows these key principles:

1. **Explicit Dependencies**: Every dependency is declared in CMakeLists.txt
2. **Layered Design**: Clear separation between infrastructure, foundation, and high-level libraries
3. **No Cycles**: Strict DAG structure prevents circular dependencies
4. **Minimal Coupling**: Libraries depend only on what they actually use
5. **Build Granularity**: Individual components can be built independently

This design enables:
- Fast incremental builds
- Clear architectural boundaries
- Easy dependency auditing
- Future modularization and packaging

---

*For questions or clarifications, see the component-specific CMakeLists.txt files or open an issue.*
