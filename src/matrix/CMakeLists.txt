add_library(matrix INTERFACE matrix.h)

add_library(dense INTERFACE dense.h)
add_executable(dense_test dense_test.cpp)
target_link_libraries(dense_test matrix dense unit)

add_library(slice INTERFACE slice.h)
add_executable(slice_test slice_test.cpp)
target_link_libraries(slice_test matrix dense slice unit)

add_library(addition INTERFACE addition.h)
add_executable(addition_test addition_test.cpp)
target_link_libraries(addition_test matrix dense unit)
