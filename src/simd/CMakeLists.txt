add_library(simd INTERFACE simd.h)

add_executable(simd_test simd_test.cpp)
target_link_libraries(simd_test simd unit)
