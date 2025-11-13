# ============================================================================
# Optimization Library - Numerical optimization algorithms
# ============================================================================
#
# Dependencies:
#   - matrix2: Matrix operations for multi-dimensional optimization
#   - tempura_flags: Compiler settings
#
# ============================================================================

add_library(optimization INTERFACE)
target_include_directories(optimization INTERFACE
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(optimization INTERFACE
    tempura::matrix2
    tempura_flags
)
add_library(tempura::optimization ALIAS optimization)

# ============================================================================
# Tests
# ============================================================================

if(NOT TEMPURA_BUILD_TESTS)
  return()
endif()

# Bracket method for function minimization
add_executable(optimization_bracket_method_test bracket_method_test.cpp)
target_link_libraries(optimization_bracket_method_test PRIVATE optimization unit)
add_test(NAME optimization_bracket_method_test COMMAND optimization_bracket_method_test)
set_tests_properties(optimization_bracket_method_test PROPERTIES LABELS "optimization;algorithms")

# Downhill simplex method (Nelder-Mead)
add_executable(optimization_downhill_simplex_test downhill_simplex_test.cpp)
target_link_libraries(optimization_downhill_simplex_test PRIVATE optimization unit)
add_test(NAME optimization_downhill_simplex_test COMMAND optimization_downhill_simplex_test)
set_tests_properties(optimization_downhill_simplex_test PROPERTIES LABELS "optimization;algorithms")
