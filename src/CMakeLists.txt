# ============================================================================
# tempura — one static library, one-line test bricks
# ============================================================================
# `tempura` is a STATIC library: it owns the src/ include root + shared flags,
# carries the few non-templated .cpp files (most code is templated, so it lives
# in headers — but not by principle), and every test links it. The real
# dependency graph lives in each header's #include lines, not in CMake. Add a
# brick by dropping foo.h (+ foo.cpp if it has non-templated code, beside the
# header) + foo_test.cpp, and one tempura_test(foo) line.
# ============================================================================

add_library(tempura STATIC)
add_library(tempura::tempura ALIAS tempura)
target_sources(tempura PRIVATE json.cpp)
target_include_directories(tempura PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)
target_link_libraries(tempura PUBLIC tempura_flags)
target_compile_features(tempura PUBLIC cxx_std_26)

# tempura_test(name [LABEL <label>] [DEPS <targets...>] [SERIAL])
#   Builds name_test from name_test.cpp, links the library, registers a ctest.
#   No-op when tests are disabled, so calls stay unconditional one-liners.
#   SERIAL: never run concurrently with other tests (for timing-sensitive tests whose
#   noise tolerances trip under parallel-ctest core contention, e.g. the benchmark oracle).
function(tempura_test name)
  if(NOT TEMPURA_BUILD_TESTS)
    return()
  endif()
  cmake_parse_arguments(ARG "SERIAL" "LABEL" "DEPS" ${ARGN})
  add_executable(${name}_test ${name}_test.cpp)
  target_link_libraries(${name}_test PRIVATE tempura::tempura ${ARG_DEPS})
  add_test(NAME ${name}_test COMMAND ${name}_test)
  if(ARG_LABEL)
    set_property(TEST ${name}_test PROPERTY LABELS ${ARG_LABEL})
  endif()
  if(ARG_SERIAL)
    set_property(TEST ${name}_test PROPERTY RUN_SERIAL TRUE)
  endif()
endfunction()

# tempura_benchmark(name [DEPS <targets...>])
#   Builds name_bench from name_bench.cpp, links the library, attaches it to the
#   `benchmarks` aggregate target. NOT a ctest test — benchmarks are slow and
#   environment-sensitive, so they are opt-in (TEMPURA_BUILD_BENCHMARKS) and run by
#   hand. The measurement-correctness oracle lives in a normal _test instead.
option(TEMPURA_BUILD_BENCHMARKS "Build benchmark executables" OFF)
if(TEMPURA_BUILD_BENCHMARKS AND NOT TARGET benchmarks)
  add_custom_target(benchmarks)
endif()
function(tempura_benchmark name)
  if(NOT TEMPURA_BUILD_BENCHMARKS)
    return()
  endif()
  cmake_parse_arguments(ARG "" "" "DEPS" ${ARGN})
  add_executable(${name}_bench ${name}_bench.cpp)
  target_link_libraries(${name}_bench PRIVATE tempura::tempura ${ARG_DEPS})
  add_dependencies(benchmarks ${name}_bench)
endfunction()

# tempura_demo(name [DEPS <targets...>])
#   Builds name_demo from name_demo.cpp. A demo is neither a test (no pass/fail)
#   nor a benchmark (no timing) — it renders to a real terminal for eyeballing,
#   so it is opt-in (TEMPURA_BUILD_DEMOS) and run by hand.
option(TEMPURA_BUILD_DEMOS "Build demo executables" OFF)
function(tempura_demo name)
  if(NOT TEMPURA_BUILD_DEMOS)
    return()
  endif()
  cmake_parse_arguments(ARG "" "" "DEPS" ${ARGN})
  add_executable(${name}_demo ${name}_demo.cpp)
  target_link_libraries(${name}_demo PRIVATE tempura::tempura ${ARG_DEPS})
endfunction()

tempura_test(scalar       LABEL base)
tempura_test(comparison   LABEL base)
tempura_test(unit         LABEL base)
tempura_test(json         LABEL base)
tempura_test(sequence     LABEL base)
tempura_test(interpolate  LABEL base)
tempura_test(root_finding LABEL base)
tempura_test(benchmark    LABEL base SERIAL)  # measurement-correctness oracle; SERIAL: timing-sensitive
tempura_test(plot           LABEL plot)  # terminal plotting
tempura_demo(plot)                       # visual spotcheck: cmake --build … --target plot_demo
tempura_test(block_graphics LABEL plot)  # mosaic drawing: best-fit glyphs + LAB color
tempura_demo(block_graphics)             # visual spotcheck of the mosaic renderer

# Module subdirectories — each owns its own CMakeLists with tempura_test() bricks.
add_subdirectory(matrix)
add_subdirectory(autodiff)
add_subdirectory(math)
