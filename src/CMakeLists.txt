# ============================================================================
# Component Subdirectories
# ============================================================================
#
# Dependency Graph (→ means "depends on"):
#
# Core Infrastructure (no dependencies):
#   - meta: Template metaprogramming utilities
#   - simd: SIMD vectorization primitives
#   - utility: General utility functions
#   - synchronization: Thread-safe data structures
#   - matrix2: Constexpr-first matrix library
#
# Top-level Libraries (minimal dependencies):
#   - broadcast_array → (none)
#   - sequence → (none)
#   - interpolate → (none)
#   - polynomial, json, chebyshev, root_finding, image, unit → (none)
#
# Mid-level Libraries (depend on infrastructure):
#   - symbolic3 → meta
#   - quadature → broadcast_array, interpolate
#   - math (sin, cos, exp) → (none)
#
# High-level Libraries (depend on mid-level):
#   - special → quadature, sequence
#   - optimization → matrix2
#   - bayes → broadcast_array, meta, simd, special
#   - task → (async execution system, depends on synchronization)
#
# Note: All libraries implicitly depend on tempura_flags (compiler settings)
# See docs/DEPENDENCIES.md for detailed dependency documentation
# See docs/dependencies.dot for visual dependency graph
# ============================================================================

# add_subdirectory(autodiff)
# add_subdirectory(cuda)
# add_subdirectory(matrix)
add_subdirectory(bigint)
add_subdirectory(modular)
add_subdirectory(symbolic3)  # Combinator-based symbolic system (canonical)
add_subdirectory(bayes)
add_subdirectory(matrix2)
add_subdirectory(matrix3)
add_subdirectory(meta)
add_subdirectory(math)
add_subdirectory(optimization)
add_subdirectory(quadature)
add_subdirectory(simd)
add_subdirectory(special)
add_subdirectory(synchronization)
add_subdirectory(task)
add_subdirectory(units)
add_subdirectory(utility)
add_subdirectory(games)

# ============================================================================
# Top-Level Library Targets
# ============================================================================

add_library(benchmark INTERFACE benchmark.h)
target_include_directories(benchmark INTERFACE
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(benchmark INTERFACE tempura_flags)
add_library(tempura::benchmark ALIAS benchmark)

add_library(chebyshev INTERFACE chebyshev.h)
target_include_directories(chebyshev INTERFACE
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(chebyshev INTERFACE tempura_flags)
add_library(tempura::chebyshev ALIAS chebyshev)

add_library(interpolate INTERFACE interpolate.h)
target_include_directories(interpolate INTERFACE
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(interpolate INTERFACE tempura_flags)
add_library(tempura::interpolate ALIAS interpolate)

add_library(json INTERFACE json.h)
target_include_directories(json INTERFACE
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(json INTERFACE tempura_flags)
add_library(tempura::json ALIAS json)

add_library(polynomial INTERFACE polynomial.h)
target_include_directories(polynomial INTERFACE
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(polynomial INTERFACE tempura_flags)
add_library(tempura::polynomial ALIAS polynomial)

add_library(sequence INTERFACE sequence.h)
target_include_directories(sequence INTERFACE
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(sequence INTERFACE tempura_flags)
add_library(tempura::sequence ALIAS sequence)

add_library(unit INTERFACE unit.h)
target_include_directories(unit INTERFACE
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(unit INTERFACE tempura_flags)
add_library(tempura::unit ALIAS unit)

add_library(root_finding INTERFACE root_finding.h)
target_include_directories(root_finding INTERFACE
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(root_finding INTERFACE tempura_flags)
add_library(tempura::root_finding ALIAS root_finding)

add_library(broadcast_array INTERFACE broadcast_array.h)
target_include_directories(broadcast_array INTERFACE
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(broadcast_array INTERFACE tempura_flags)
add_library(tempura::broadcast_array ALIAS broadcast_array)

add_library(function_traits INTERFACE function_traits.h)
target_include_directories(function_traits INTERFACE
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(function_traits INTERFACE tempura_flags)
add_library(tempura::function_traits ALIAS function_traits)

add_library(image INTERFACE image.h)
target_include_directories(image INTERFACE
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(image INTERFACE tempura_flags)
add_library(tempura::image ALIAS image)

# Profiler is a shared library (not INTERFACE)
add_library(profiler SHARED)
target_sources(profiler PUBLIC profiler.h profiler.cpp)
target_include_directories(profiler PUBLIC
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(profiler PUBLIC tempura_flags)
add_library(tempura::profiler ALIAS profiler)

# ============================================================================
# Tests
# ============================================================================

if(TEMPURA_BUILD_TESTS)
  add_executable(chebyshev_test chebyshev_test.cpp)
  target_link_libraries(chebyshev_test unit chebyshev)
  add_test(NAME chebyshev_test COMMAND chebyshev_test)
  set_property(TEST chebyshev_test PROPERTY LABELS base)

  add_executable(broadcast_array_test broadcast_array_test.cpp)
  target_link_libraries(broadcast_array_test broadcast_array unit)
  add_test(NAME broadcast_array_test COMMAND broadcast_array_test)
  set_property(TEST broadcast_array_test PROPERTY LABELS base)

  add_executable(image_test image_test.cpp)
  target_link_libraries(image_test image unit)
  add_test(NAME image_test COMMAND image_test)
  set_property(TEST image_test PROPERTY LABELS base)

  add_executable(polynomial_test polynomial_test.cpp)
  target_link_libraries(polynomial_test unit polynomial)
  add_test(NAME polynomial_test COMMAND polynomial_test)
  set_property(TEST polynomial_test PROPERTY LABELS base)

  add_executable(json_test json_test.cpp)
  target_link_libraries(json_test unit json)
  add_test(NAME json_test COMMAND json_test)
  set_property(TEST json_test PROPERTY LABELS base)

  add_executable(sequence_test sequence_test.cpp)
  target_link_libraries(sequence_test unit sequence)
  add_test(NAME sequence_test COMMAND sequence_test)
  set_property(TEST sequence_test PROPERTY LABELS base)

  add_executable(root_finding_test root_finding_test.cpp)
  target_link_libraries(root_finding_test unit root_finding)
  add_test(NAME root_finding_test COMMAND root_finding_test)
  set_property(TEST root_finding_test PROPERTY LABELS base)

  add_executable(interpolate_test interpolate_test.cpp)
  target_link_libraries(interpolate_test unit interpolate)
  add_test(NAME interpolate_test COMMAND interpolate_test)
  set_property(TEST interpolate_test PROPERTY LABELS base)
endif()


