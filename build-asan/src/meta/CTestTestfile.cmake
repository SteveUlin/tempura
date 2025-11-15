# CMake generated Testfile for 
# Source directory: /home/ulins/tempura/src/meta
# Build directory: /home/ulins/tempura/build-asan/src/meta
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test([=[meta_type_sort]=] "/home/ulins/tempura/build-asan/src/meta/meta_type_sort_test")
set_tests_properties([=[meta_type_sort]=] PROPERTIES  LABELS "meta;type_manipulation" _BACKTRACE_TRIPLES "/home/ulins/tempura/src/meta/CMakeLists.txt;32;add_test;/home/ulins/tempura/src/meta/CMakeLists.txt;0;")
add_test([=[meta_macro_utils]=] "/home/ulins/tempura/build-asan/src/meta/meta_macro_utils_test")
set_tests_properties([=[meta_macro_utils]=] PROPERTIES  LABELS "meta;utilities" _BACKTRACE_TRIPLES "/home/ulins/tempura/src/meta/CMakeLists.txt;38;add_test;/home/ulins/tempura/src/meta/CMakeLists.txt;0;")
add_test([=[meta_static_string_display]=] "/home/ulins/tempura/build-asan/src/meta/meta_static_string_display_test")
set_tests_properties([=[meta_static_string_display]=] PROPERTIES  LABELS "meta;utilities;debugging" _BACKTRACE_TRIPLES "/home/ulins/tempura/src/meta/CMakeLists.txt;44;add_test;/home/ulins/tempura/src/meta/CMakeLists.txt;0;")
add_test([=[meta_meta_test]=] "/home/ulins/tempura/build-asan/src/meta/meta_meta_test")
set_tests_properties([=[meta_meta_test]=] PROPERTIES  LABELS "meta;core" _BACKTRACE_TRIPLES "/home/ulins/tempura/src/meta/CMakeLists.txt;50;add_test;/home/ulins/tempura/src/meta/CMakeLists.txt;0;")
add_test([=[meta_tuple_test]=] "/home/ulins/tempura/build-asan/src/meta/meta_tuple_test")
set_tests_properties([=[meta_tuple_test]=] PROPERTIES  LABELS "meta;tuples" _BACKTRACE_TRIPLES "/home/ulins/tempura/src/meta/CMakeLists.txt;56;add_test;/home/ulins/tempura/src/meta/CMakeLists.txt;0;")
add_test([=[meta_expression_test]=] "/home/ulins/tempura/build-asan/src/meta/meta_expression_test")
set_tests_properties([=[meta_expression_test]=] PROPERTIES  LABELS "meta;expressions" _BACKTRACE_TRIPLES "/home/ulins/tempura/src/meta/CMakeLists.txt;62;add_test;/home/ulins/tempura/src/meta/CMakeLists.txt;0;")
