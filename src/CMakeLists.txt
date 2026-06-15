# ============================================================================
# Components — gcc-trunk C++26 migration (2026-06-15)
# ============================================================================
# The build is stripped to a single verified core target (scalar) compiled with
# GCC trunk under -std=c++26 -freflection. Every other module is dropped from
# the default build and re-added incrementally, each confirmed to compile under
# the new toolchain before it returns. The full prior contents live in jj
# history (recover a module with `jj diff` against the pre-migration commit).
# ============================================================================

add_library(scalar INTERFACE scalar.h)
target_include_directories(scalar INTERFACE
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(scalar INTERFACE tempura_flags)
add_library(tempura::scalar ALIAS scalar)

# Owning N-d array (std::mdarray reimplementation) over the std mdspan/extents
# primitives — the storage foundation the matrix layer is built on. Element-
# agnostic like std::mdarray, so it depends only on flags, not on scalar.
add_library(mdarray INTERFACE mdarray.h)
target_include_directories(mdarray INTERFACE
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(mdarray INTERFACE tempura_flags)
add_library(tempura::mdarray ALIAS mdarray)

# ============================================================================
# Tests
# ============================================================================

if(TEMPURA_BUILD_TESTS)
  add_executable(scalar_test scalar_test.cpp)
  target_link_libraries(scalar_test scalar)
  add_test(NAME scalar_test COMMAND scalar_test)
  set_property(TEST scalar_test PROPERTY LABELS base)

  add_executable(mdarray_test mdarray_test.cpp)
  target_link_libraries(mdarray_test mdarray)
  add_test(NAME mdarray_test COMMAND mdarray_test)
  set_property(TEST mdarray_test PROPERTY LABELS base)
endif()
