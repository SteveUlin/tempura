# ============================================================================
# Quadrature Library - Numerical integration methods
# ============================================================================
#
# Dependencies:
#   - broadcast_array: Array operations for integration algorithms
#   - interpolate: Polynomial interpolation for Gaussian quadrature
#   - tempura_flags: Compiler settings
#
# ============================================================================

add_library(quadature INTERFACE)
target_include_directories(quadature INTERFACE
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(quadature INTERFACE
    tempura::broadcast_array
    tempura::interpolate
    tempura_flags
)
add_library(tempura::quadature ALIAS quadature)

# ============================================================================
# Tests
# ============================================================================

if(NOT TEMPURA_BUILD_TESTS)
  return()
endif()

# Gaussian quadrature
add_executable(quadature_gaussian_test gaussian_test.cpp)
target_link_libraries(quadature_gaussian_test PRIVATE quadature unit)
add_test(NAME quadature_gaussian_test COMMAND quadature_gaussian_test)
set_tests_properties(quadature_gaussian_test PROPERTIES LABELS "quadature;integration")

# Improper integrals
add_executable(quadature_improper_test improper_test.cpp)
target_link_libraries(quadature_improper_test PRIVATE quadature unit)
add_test(NAME quadature_improper_test COMMAND quadature_improper_test)
set_tests_properties(quadature_improper_test PROPERTIES LABELS "quadature;integration")

# Monte Carlo integration
add_executable(quadature_monte_carlo_test monte_carlo_test.cpp)
target_link_libraries(quadature_monte_carlo_test PRIVATE quadature unit)
add_test(NAME quadature_monte_carlo_test COMMAND quadature_monte_carlo_test)
set_tests_properties(quadature_monte_carlo_test PROPERTIES LABELS "quadature;integration;monte_carlo")

# Newton-Cotes formulas
add_executable(quadature_newton_cotes_test newton_cotes_test.cpp)
target_link_libraries(quadature_newton_cotes_test PRIVATE quadature unit)
add_test(NAME quadature_newton_cotes_test COMMAND quadature_newton_cotes_test)
set_tests_properties(quadature_newton_cotes_test PROPERTIES LABELS "quadature;integration")
