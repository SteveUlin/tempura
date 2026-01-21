# ============================================================================
# prob: Generic probability distribution classes
# ============================================================================
#
# Class-based probability distributions with generic parameter types.
# Each distribution is parameterized by copyable types, enabling use with
# scalars, symbolic expressions, or other distributions for hierarchical models.
# ============================================================================

add_library(prob INTERFACE)
target_include_directories(prob INTERFACE
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(prob INTERFACE tempura_flags)
add_library(tempura::prob ALIAS prob)

# ============================================================================
# Tests
# ============================================================================

if(NOT TEMPURA_BUILD_TESTS)
  return()
endif()

add_executable(normal_test normal_test.cc)
target_link_libraries(normal_test PRIVATE prob unit)
add_test(NAME prob_normal COMMAND normal_test)
set_tests_properties(prob_normal PROPERTIES LABELS "prob")

add_executable(half_normal_test half_normal_test.cc)
target_link_libraries(half_normal_test PRIVATE prob unit)
add_test(NAME prob_half_normal COMMAND half_normal_test)
set_tests_properties(prob_half_normal PROPERTIES LABELS "prob")

add_executable(exponential_test exponential_test.cc)
target_link_libraries(exponential_test PRIVATE prob unit)
add_test(NAME prob_exponential COMMAND exponential_test)
set_tests_properties(prob_exponential PROPERTIES LABELS "prob")

add_executable(gamma_test gamma_test.cc)
target_link_libraries(gamma_test PRIVATE prob unit)
add_test(NAME prob_gamma COMMAND gamma_test)
set_tests_properties(prob_gamma PROPERTIES LABELS "prob")

add_executable(beta_test beta_test.cc)
target_link_libraries(beta_test PRIVATE prob unit)
add_test(NAME prob_beta COMMAND beta_test)
set_tests_properties(prob_beta PROPERTIES LABELS "prob")

add_executable(uniform_test uniform_test.cc)
target_link_libraries(uniform_test PRIVATE prob unit)
add_test(NAME prob_uniform COMMAND uniform_test)
set_tests_properties(prob_uniform PROPERTIES LABELS "prob")

add_executable(cauchy_test cauchy_test.cc)
target_link_libraries(cauchy_test PRIVATE prob unit)
add_test(NAME prob_cauchy COMMAND cauchy_test)
set_tests_properties(prob_cauchy PROPERTIES LABELS "prob")

add_executable(bernoulli_test bernoulli_test.cc)
target_link_libraries(bernoulli_test PRIVATE prob unit)
add_test(NAME prob_bernoulli COMMAND bernoulli_test)
set_tests_properties(prob_bernoulli PROPERTIES LABELS "prob")

add_executable(binomial_test binomial_test.cc)
target_link_libraries(binomial_test PRIVATE prob unit)
add_test(NAME prob_binomial COMMAND binomial_test)
set_tests_properties(prob_binomial PROPERTIES LABELS "prob")

add_executable(logistic_test logistic_test.cc)
target_link_libraries(logistic_test PRIVATE prob unit)
add_test(NAME prob_logistic COMMAND logistic_test)
set_tests_properties(prob_logistic PROPERTIES LABELS "prob")

add_executable(poisson_test poisson_test.cc)
target_link_libraries(poisson_test PRIVATE prob unit)
add_test(NAME prob_poisson COMMAND poisson_test)
set_tests_properties(prob_poisson PROPERTIES LABELS "prob")

add_executable(student_t_test student_t_test.cc)
target_link_libraries(student_t_test PRIVATE prob unit)
add_test(NAME prob_student_t COMMAND student_t_test)
set_tests_properties(prob_student_t PROPERTIES LABELS "prob")
