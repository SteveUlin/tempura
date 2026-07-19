# optim — gradient-based optimization; objectives.h is the shared scalar-generic test bed.
# tempura_test() is defined in the parent src/CMakeLists.txt.
tempura_test(objectives LABEL optim)
tempura_test(line_search LABEL optim)
