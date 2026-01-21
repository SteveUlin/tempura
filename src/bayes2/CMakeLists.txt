# ============================================================================
# bayes2 - Compile-time probabilistic programming DSL
# ============================================================================
#
# A refined probabilistic programming DSL with compile-time DAG construction
# and first-class MCMC integration.
#
# ============================================================================

add_library(bayes2 INTERFACE)
target_include_directories(bayes2 INTERFACE
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
    $<INSTALL_INTERFACE:include>
)
target_link_libraries(bayes2 INTERFACE
    tempura::symbolic3
    tempura::prob
    tempura_flags
)
add_library(tempura::bayes2 ALIAS bayes2)

# ============================================================================
# Tests
# ============================================================================

if(NOT TEMPURA_BUILD_TESTS)
  return()
endif()

add_executable(bayes2_test bayes2_test.cc)
target_link_libraries(bayes2_test PRIVATE bayes2 unit)
add_test(NAME bayes2_test COMMAND bayes2_test)
set_tests_properties(bayes2_test PROPERTIES LABELS "bayes2")

add_executable(reparam_test reparam_test.cc)
target_link_libraries(reparam_test PRIVATE bayes2 unit)
add_test(NAME reparam_test COMMAND reparam_test)
set_tests_properties(reparam_test PROPERTIES LABELS "bayes2")

add_executable(plate_test plate_test.cc)
target_link_libraries(plate_test PRIVATE bayes2 unit)
add_test(NAME plate_test COMMAND plate_test)
set_tests_properties(plate_test PROPERTIES LABELS "bayes2")

add_executable(bayes2_indexed_test indexed_test.cpp)
target_link_libraries(bayes2_indexed_test PRIVATE bayes2 unit)
add_test(NAME bayes2_indexed COMMAND bayes2_indexed_test)
set_tests_properties(bayes2_indexed PROPERTIES LABELS "bayes2;indexed;plates")
