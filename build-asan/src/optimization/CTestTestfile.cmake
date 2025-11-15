# CMake generated Testfile for 
# Source directory: /home/ulins/tempura/src/optimization
# Build directory: /home/ulins/tempura/build-asan/src/optimization
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test([=[optimization_bracket_method_test]=] "/home/ulins/tempura/build-asan/src/optimization/optimization_bracket_method_test")
set_tests_properties([=[optimization_bracket_method_test]=] PROPERTIES  LABELS "optimization;algorithms" _BACKTRACE_TRIPLES "/home/ulins/tempura/src/optimization/CMakeLists.txt;33;add_test;/home/ulins/tempura/src/optimization/CMakeLists.txt;0;")
add_test([=[optimization_downhill_simplex_test]=] "/home/ulins/tempura/build-asan/src/optimization/optimization_downhill_simplex_test")
set_tests_properties([=[optimization_downhill_simplex_test]=] PROPERTIES  LABELS "optimization;algorithms" _BACKTRACE_TRIPLES "/home/ulins/tempura/src/optimization/CMakeLists.txt;39;add_test;/home/ulins/tempura/src/optimization/CMakeLists.txt;0;")
