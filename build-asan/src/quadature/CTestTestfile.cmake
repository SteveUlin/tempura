# CMake generated Testfile for 
# Source directory: /home/ulins/tempura/src/quadature
# Build directory: /home/ulins/tempura/build-asan/src/quadature
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test([=[quadature_gaussian_test]=] "/home/ulins/tempura/build-asan/src/quadature/quadature_gaussian_test")
set_tests_properties([=[quadature_gaussian_test]=] PROPERTIES  LABELS "quadature;integration" _BACKTRACE_TRIPLES "/home/ulins/tempura/src/quadature/CMakeLists.txt;35;add_test;/home/ulins/tempura/src/quadature/CMakeLists.txt;0;")
add_test([=[quadature_improper_test]=] "/home/ulins/tempura/build-asan/src/quadature/quadature_improper_test")
set_tests_properties([=[quadature_improper_test]=] PROPERTIES  LABELS "quadature;integration" _BACKTRACE_TRIPLES "/home/ulins/tempura/src/quadature/CMakeLists.txt;41;add_test;/home/ulins/tempura/src/quadature/CMakeLists.txt;0;")
add_test([=[quadature_monte_carlo_test]=] "/home/ulins/tempura/build-asan/src/quadature/quadature_monte_carlo_test")
set_tests_properties([=[quadature_monte_carlo_test]=] PROPERTIES  LABELS "quadature;integration;monte_carlo" _BACKTRACE_TRIPLES "/home/ulins/tempura/src/quadature/CMakeLists.txt;47;add_test;/home/ulins/tempura/src/quadature/CMakeLists.txt;0;")
add_test([=[quadature_newton_cotes_test]=] "/home/ulins/tempura/build-asan/src/quadature/quadature_newton_cotes_test")
set_tests_properties([=[quadature_newton_cotes_test]=] PROPERTIES  LABELS "quadature;integration" _BACKTRACE_TRIPLES "/home/ulins/tempura/src/quadature/CMakeLists.txt;53;add_test;/home/ulins/tempura/src/quadature/CMakeLists.txt;0;")
