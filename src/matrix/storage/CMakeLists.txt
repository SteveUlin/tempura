add_library(dense INTERFACE dense.h)
add_executable(dense_test dense_test.cpp)
target_link_libraries(dense_test matrix dense unit profiler)

add_library(identity INTERFACE identity.h)
add_executable(identity_test identity_test.cpp)
target_link_libraries(identity_test identity unit)

add_library(permutation INTERFACE permutation.h)
add_executable(permutation_test permutation_test.cpp)
target_link_libraries(permutation_test permutation unit printer identity)

