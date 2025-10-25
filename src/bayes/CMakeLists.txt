# ============================================================================
# Bayes Library - Probability distributions and statistical inference
# ============================================================================

add_library(bayes INTERFACE)

# Include path must expose src/ for two reasons:
# 1. bayes headers use #include "bayes/normal.h" (self-reference pattern)
# 2. integrators.h depends on broadcast_array.h (another tempura component)
# Note: This gives bayes consumers access to all tempura headers (transitivity)
target_include_directories(bayes INTERFACE
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
    $<INSTALL_INTERFACE:include>
)

# ============================================================================
# Tests - Only build when testing is enabled
# ============================================================================

if(BUILD_TESTING)
    # Helper reduces boilerplate: 3 lines per test → 1 line
    function(add_bayes_test TEST_NAME)
        add_executable(bayes_${TEST_NAME} ${TEST_NAME}.cpp)
        target_link_libraries(bayes_${TEST_NAME} PRIVATE bayes unit)
        add_test(NAME bayes_${TEST_NAME} COMMAND bayes_${TEST_NAME})
        set_tests_properties(bayes_${TEST_NAME} PROPERTIES LABELS "bayes;${ARGN}")
    endfunction()

    # Distribution tests
    add_bayes_test(normal_test distributions)
    add_bayes_test(beta_test distributions)
    add_bayes_test(bernoulli_test distributions)
    add_bayes_test(uniform_test distributions)

    # Random number generator tests
    add_bayes_test(random_test sampling)
    add_bayes_test(random_simd_test sampling)
    add_bayes_test(rng_tests_test sampling quality)

    # Benchmark executable (not a CTest test)
    # Excluded from test runs to avoid slowing down CI
    add_executable(normal_bench normal_bench.cpp)
    target_link_libraries(normal_bench PRIVATE bayes benchmark unit)

    # RNG quality report (standalone analysis tool)
    add_executable(rng_quality_report rng_quality_report.cpp)
    target_link_libraries(rng_quality_report PRIVATE bayes)

    # SIMD RNG quality report (standalone analysis tool)
    add_executable(rng_simd_quality_report rng_simd_quality_report.cpp)
    target_link_libraries(rng_simd_quality_report PRIVATE bayes simd)
endif()
