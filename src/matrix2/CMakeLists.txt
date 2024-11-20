add_library(matrix2 INTERFACE matrix.h)
add_executable(matrix2_test matrix_test.cpp)
target_link_libraries(matrix2_test matrix2 unit)

add_library(dense2 INTERFACE dense.h)
