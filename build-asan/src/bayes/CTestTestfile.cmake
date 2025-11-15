# CMake generated Testfile for 
# Source directory: /home/ulins/tempura/src/bayes
# Build directory: /home/ulins/tempura/build-asan/src/bayes
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test([=[bayes_normal_test]=] "/home/ulins/tempura/build-asan/src/bayes/bayes_normal_test")
set_tests_properties([=[bayes_normal_test]=] PROPERTIES  LABELS "bayes;distributions" _BACKTRACE_TRIPLES "/home/ulins/tempura/src/bayes/CMakeLists.txt;43;add_test;/home/ulins/tempura/src/bayes/CMakeLists.txt;0;")
add_test([=[bayes_beta_test]=] "/home/ulins/tempura/build-asan/src/bayes/bayes_beta_test")
set_tests_properties([=[bayes_beta_test]=] PROPERTIES  LABELS "bayes;distributions" _BACKTRACE_TRIPLES "/home/ulins/tempura/src/bayes/CMakeLists.txt;49;add_test;/home/ulins/tempura/src/bayes/CMakeLists.txt;0;")
add_test([=[bayes_bernoulli_test]=] "/home/ulins/tempura/build-asan/src/bayes/bayes_bernoulli_test")
set_tests_properties([=[bayes_bernoulli_test]=] PROPERTIES  LABELS "bayes;distributions" _BACKTRACE_TRIPLES "/home/ulins/tempura/src/bayes/CMakeLists.txt;55;add_test;/home/ulins/tempura/src/bayes/CMakeLists.txt;0;")
add_test([=[bayes_binomial_test]=] "/home/ulins/tempura/build-asan/src/bayes/bayes_binomial_test")
set_tests_properties([=[bayes_binomial_test]=] PROPERTIES  LABELS "bayes;distributions" _BACKTRACE_TRIPLES "/home/ulins/tempura/src/bayes/CMakeLists.txt;61;add_test;/home/ulins/tempura/src/bayes/CMakeLists.txt;0;")
add_test([=[bayes_uniform_test]=] "/home/ulins/tempura/build-asan/src/bayes/bayes_uniform_test")
set_tests_properties([=[bayes_uniform_test]=] PROPERTIES  LABELS "bayes;distributions" _BACKTRACE_TRIPLES "/home/ulins/tempura/src/bayes/CMakeLists.txt;67;add_test;/home/ulins/tempura/src/bayes/CMakeLists.txt;0;")
add_test([=[bayes_logistic_test]=] "/home/ulins/tempura/build-asan/src/bayes/bayes_logistic_test")
set_tests_properties([=[bayes_logistic_test]=] PROPERTIES  LABELS "bayes;distributions" _BACKTRACE_TRIPLES "/home/ulins/tempura/src/bayes/CMakeLists.txt;73;add_test;/home/ulins/tempura/src/bayes/CMakeLists.txt;0;")
add_test([=[bayes_cauchy_test]=] "/home/ulins/tempura/build-asan/src/bayes/bayes_cauchy_test")
set_tests_properties([=[bayes_cauchy_test]=] PROPERTIES  LABELS "bayes;distributions" _BACKTRACE_TRIPLES "/home/ulins/tempura/src/bayes/CMakeLists.txt;79;add_test;/home/ulins/tempura/src/bayes/CMakeLists.txt;0;")
add_test([=[bayes_exponential_test]=] "/home/ulins/tempura/build-asan/src/bayes/bayes_exponential_test")
set_tests_properties([=[bayes_exponential_test]=] PROPERTIES  LABELS "bayes;distributions" _BACKTRACE_TRIPLES "/home/ulins/tempura/src/bayes/CMakeLists.txt;85;add_test;/home/ulins/tempura/src/bayes/CMakeLists.txt;0;")
add_test([=[bayes_gamma_test]=] "/home/ulins/tempura/build-asan/src/bayes/bayes_gamma_test")
set_tests_properties([=[bayes_gamma_test]=] PROPERTIES  LABELS "bayes;distributions" _BACKTRACE_TRIPLES "/home/ulins/tempura/src/bayes/CMakeLists.txt;91;add_test;/home/ulins/tempura/src/bayes/CMakeLists.txt;0;")
add_test([=[bayes_random_test]=] "/home/ulins/tempura/build-asan/src/bayes/bayes_random_test")
set_tests_properties([=[bayes_random_test]=] PROPERTIES  LABELS "bayes;sampling" _BACKTRACE_TRIPLES "/home/ulins/tempura/src/bayes/CMakeLists.txt;101;add_test;/home/ulins/tempura/src/bayes/CMakeLists.txt;0;")
add_test([=[bayes_random_simd_test]=] "/home/ulins/tempura/build-asan/src/bayes/bayes_random_simd_test")
set_tests_properties([=[bayes_random_simd_test]=] PROPERTIES  LABELS "bayes;sampling" _BACKTRACE_TRIPLES "/home/ulins/tempura/src/bayes/CMakeLists.txt;107;add_test;/home/ulins/tempura/src/bayes/CMakeLists.txt;0;")
add_test([=[bayes_rng_tests_test]=] "/home/ulins/tempura/build-asan/src/bayes/bayes_rng_tests_test")
set_tests_properties([=[bayes_rng_tests_test]=] PROPERTIES  LABELS "bayes;sampling;quality" _BACKTRACE_TRIPLES "/home/ulins/tempura/src/bayes/CMakeLists.txt;113;add_test;/home/ulins/tempura/src/bayes/CMakeLists.txt;0;")
