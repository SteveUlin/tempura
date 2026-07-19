# autodiff — automatic-differentiation exploration (see README.md).
# tempura_test() is defined in the parent src/CMakeLists.txt.
tempura_test(weighted_dag LABEL autodiff)
tempura_test(dual LABEL autodiff)
tempura_test(tape LABEL autodiff)
