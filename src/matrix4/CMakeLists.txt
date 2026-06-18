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
