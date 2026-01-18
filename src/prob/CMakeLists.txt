# ============================================================================
# prob: Generic probability functions for symbolic and numeric types
# ============================================================================
#
# Dependencies:
#   - symbolic3: For symbolic expression support
#   - tempura_flags: Compiler settings
#
# This library provides log-probability functions that work with both
# symbolic expressions (returning Expression types) and concrete numeric
# values (returning doubles).
# ============================================================================

add_library(prob INTERFACE)
target_include_directories(prob INTERFACE
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(prob INTERFACE tempura::symbolic3 tempura_flags)
add_library(tempura::prob ALIAS prob)

# ============================================================================
# Tests
# ============================================================================

if(NOT TEMPURA_BUILD_TESTS)
  return()
endif()

add_executable(log_prob_test log_prob_test.cc)
target_link_libraries(log_prob_test PRIVATE prob unit)
add_test(NAME prob_log_prob COMMAND log_prob_test)
set_tests_properties(prob_log_prob PROPERTIES LABELS "prob;base")

# Debug executable for investigating gradient issues
add_executable(gradient_debug gradient_debug.cc)
target_link_libraries(gradient_debug PRIVATE prob)
