# matrix4 — linear-algebra module: the MdArray owner (mdarray.h, P1684R5) + Dense/
# Vec aliases and view-based kernels (matrix.h). tempura_test() is defined in the
# parent src/CMakeLists.txt.
tempura_test(mdarray     LABEL base)
tempura_test(matrix      LABEL base)
tempura_test(vec         LABEL base)
tempura_test(permutation LABEL base)
tempura_test(permuted    LABEL base)
tempura_test(transpose   LABEL base)
tempura_test(lu_decomposition LABEL base)
tempura_test(to_string   LABEL base)
tempura_test(slices      LABEL base)
tempura_test(gauss_jordan LABEL base)
tempura_test(qr          LABEL base)
tempura_test(symmetric_eigen LABEL base)
tempura_test(svd         LABEL base)
tempura_test(banded      LABEL base)

# Benchmarks — measure the efficiency claims (notably LU eager-swap vs lazy-gather).
# Opt-in via -DTEMPURA_BUILD_BENCHMARKS=ON; not part of the ctest run.
tempura_benchmark(lu_decomposition)
tempura_benchmark(matrix)
