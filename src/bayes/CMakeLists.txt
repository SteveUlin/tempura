# ============================================================================
# Bayes Library - Probability distributions and statistical inference
# ============================================================================
#
# Dependencies:
#   - broadcast_array: Array broadcasting for vectorized operations
#   - meta: Type metaprogramming for distribution traits
#   - simd: SIMD vectorization for random number generation
#   - special: Special functions (gamma, erf) for distributions
#   - tempura_flags: Compiler settings
#
# ============================================================================

add_library(bayes INTERFACE)
target_include_directories(bayes INTERFACE
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
    $<INSTALL_INTERFACE:include>
)
target_link_libraries(bayes INTERFACE
    tempura::broadcast_array
    tempura::meta
    tempura::simd
    tempura::special
    tempura_flags
)
add_library(tempura::bayes ALIAS bayes)

# ============================================================================
# Tests
# ============================================================================

if(NOT TEMPURA_BUILD_TESTS)
  return()
endif()

# ============================================================================
# Distribution Tests
# ============================================================================

# Normal (Gaussian) distribution
add_executable(bayes_normal_test normal_test.cpp)
target_link_libraries(bayes_normal_test PRIVATE bayes unit)
add_test(NAME bayes_normal_test COMMAND bayes_normal_test)
set_tests_properties(bayes_normal_test PROPERTIES LABELS "bayes;distributions")

# Beta distribution
add_executable(bayes_beta_test beta_test.cpp)
target_link_libraries(bayes_beta_test PRIVATE bayes unit)
add_test(NAME bayes_beta_test COMMAND bayes_beta_test)
set_tests_properties(bayes_beta_test PROPERTIES LABELS "bayes;distributions")

# Bernoulli distribution
add_executable(bayes_bernoulli_test bernoulli_test.cpp)
target_link_libraries(bayes_bernoulli_test PRIVATE bayes unit)
add_test(NAME bayes_bernoulli_test COMMAND bayes_bernoulli_test)
set_tests_properties(bayes_bernoulli_test PROPERTIES LABELS "bayes;distributions")

# Binomial distribution
add_executable(bayes_binomial_test binomial_test.cpp)
target_link_libraries(bayes_binomial_test PRIVATE bayes unit)
add_test(NAME bayes_binomial_test COMMAND bayes_binomial_test)
set_tests_properties(bayes_binomial_test PROPERTIES LABELS "bayes;distributions")

# Uniform distribution
add_executable(bayes_uniform_test uniform_test.cpp)
target_link_libraries(bayes_uniform_test PRIVATE bayes unit)
add_test(NAME bayes_uniform_test COMMAND bayes_uniform_test)
set_tests_properties(bayes_uniform_test PROPERTIES LABELS "bayes;distributions")

# Logistic distribution
add_executable(bayes_logistic_test logistic_test.cpp)
target_link_libraries(bayes_logistic_test PRIVATE bayes unit)
add_test(NAME bayes_logistic_test COMMAND bayes_logistic_test)
set_tests_properties(bayes_logistic_test PROPERTIES LABELS "bayes;distributions")

# Cauchy distribution
add_executable(bayes_cauchy_test cauchy_test.cpp)
target_link_libraries(bayes_cauchy_test PRIVATE bayes unit)
add_test(NAME bayes_cauchy_test COMMAND bayes_cauchy_test)
set_tests_properties(bayes_cauchy_test PROPERTIES LABELS "bayes;distributions")

# Exponential distribution
add_executable(bayes_exponential_test exponential_test.cpp)
target_link_libraries(bayes_exponential_test PRIVATE bayes unit)
add_test(NAME bayes_exponential_test COMMAND bayes_exponential_test)
set_tests_properties(bayes_exponential_test PROPERTIES LABELS "bayes;distributions")

# Gamma distribution
add_executable(bayes_gamma_test gamma_test.cpp)
target_link_libraries(bayes_gamma_test PRIVATE bayes unit)
add_test(NAME bayes_gamma_test COMMAND bayes_gamma_test)
set_tests_properties(bayes_gamma_test PROPERTIES LABELS "bayes;distributions")

# ============================================================================
# Random Number Generator Tests
# ============================================================================

# Basic random number generation tests
add_executable(bayes_random_test random_test.cpp)
target_link_libraries(bayes_random_test PRIVATE bayes unit)
add_test(NAME bayes_random_test COMMAND bayes_random_test)
set_tests_properties(bayes_random_test PROPERTIES LABELS "bayes;sampling")

# SIMD random number generation tests
add_executable(bayes_random_simd_test random_simd_test.cpp)
target_link_libraries(bayes_random_simd_test PRIVATE bayes unit)
add_test(NAME bayes_random_simd_test COMMAND bayes_random_simd_test)
set_tests_properties(bayes_random_simd_test PROPERTIES LABELS "bayes;sampling")

# RNG quality tests
add_executable(bayes_rng_tests_test rng_tests_test.cpp)
target_link_libraries(bayes_rng_tests_test PRIVATE bayes unit)
add_test(NAME bayes_rng_tests_test COMMAND bayes_rng_tests_test)
set_tests_properties(bayes_rng_tests_test PROPERTIES LABELS "bayes;sampling;quality")

# ============================================================================
# Benchmarks and Analysis Tools (not formal tests)
# ============================================================================

# Normal distribution benchmark (not a CTest test)
add_executable(normal_bench normal_bench.cpp)
target_link_libraries(normal_bench PRIVATE bayes benchmark unit)

# RNG quality report (standalone analysis tool)
add_executable(rng_quality_report rng_quality_report.cpp)
target_link_libraries(rng_quality_report PRIVATE bayes)

# SIMD RNG quality report (standalone analysis tool)
add_executable(rng_simd_quality_report rng_simd_quality_report.cpp)
target_link_libraries(rng_simd_quality_report PRIVATE bayes simd)
