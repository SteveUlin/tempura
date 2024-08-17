add_library(matrix INTERFACE matrix.h)
add_executable(matrix_test matrix_test.cpp)
target_link_libraries(matrix_test matrix unit)
