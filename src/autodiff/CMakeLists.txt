# autodiff — automatic-differentiation exploration. Forward (dual), reverse (tape), and
# higher-order (jet) modes, composed via a jvp/vjp transform layer (see README.md).
# tempura_test() is defined in the parent src/CMakeLists.txt.
tempura_test(dual LABEL autodiff)
tempura_test(tape LABEL autodiff)
tempura_test(transform LABEL autodiff)
tempura_test(jacobian LABEL autodiff)
