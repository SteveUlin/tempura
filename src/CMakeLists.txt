# ============================================================================
# tempura — one INTERFACE target, one-line test bricks
# ============================================================================
# The whole library is a single INTERFACE target: `tempura` owns the src/
# include root and the shared compiler flags, and every test links it. There
# are no per-header libraries — for header-only code their link edges enforce
# nothing (all share this include root), so the real dependency graph lives in
# each header's #include lines, not in CMake. Add a brick by dropping foo.h +
# foo_test.cpp beside their peers and adding one tempura_test(foo) line.
# ============================================================================

add_library(tempura INTERFACE)
add_library(tempura::tempura ALIAS tempura)
target_include_directories(tempura INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)
target_link_libraries(tempura INTERFACE tempura_flags)
target_compile_features(tempura INTERFACE cxx_std_26)

# tempura_test(name [LABEL <label>] [DEPS <targets...>])
#   Builds name_test from name_test.cpp, links the library, registers a ctest.
#   No-op when tests are disabled, so calls stay unconditional one-liners.
function(tempura_test name)
  if(NOT TEMPURA_BUILD_TESTS)
    return()
  endif()
  cmake_parse_arguments(ARG "" "LABEL" "DEPS" ${ARGN})
  add_executable(${name}_test ${name}_test.cpp)
  target_link_libraries(${name}_test PRIVATE tempura::tempura ${ARG_DEPS})
  add_test(NAME ${name}_test COMMAND ${name}_test)
  if(ARG_LABEL)
    set_property(TEST ${name}_test PROPERTY LABELS ${ARG_LABEL})
  endif()
endfunction()

# ── bricks ──────────────────────────────────────────────────────────────────
tempura_test(scalar     LABEL base)
tempura_test(comparison LABEL base)
tempura_test(unit       LABEL base)
tempura_test(mdarray    LABEL base)
tempura_test(matrix     LABEL base)
tempura_test(vec        LABEL base)
