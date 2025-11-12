add_library(simd INTERFACE simd.h)
target_include_directories(simd INTERFACE
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(simd INTERFACE tempura_flags)
add_library(tempura::simd ALIAS simd)

if(NOT TEMPURA_BUILD_TESTS)
  return()
endif()

add_executable(simd_test simd_test.cpp)
target_link_libraries(simd_test simd unit)
