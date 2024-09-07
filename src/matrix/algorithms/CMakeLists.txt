add_executable(fft fft.cpp)
target_link_libraries(fft kronecker_product dense printer)

add_library(lu_decomposition INTERFACE lu_decomposition.h)

add_executable(lu_decomposition_test lu_decomposition_test.cpp)
target_link_libraries(lu_decomposition_test lu_decomposition unit dense slice matrix)

