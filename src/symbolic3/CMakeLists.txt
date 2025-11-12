# Symbolic3: Combinator-based symbolic computation system

add_library(symbolic3 INTERFACE)
target_include_directories(symbolic3 INTERFACE
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(symbolic3 INTERFACE meta tempura_flags)
add_library(tempura::symbolic3 ALIAS symbolic3)

# ============================================================================
# Tests
# ============================================================================

if(NOT TEMPURA_BUILD_TESTS)
  return()
endif()

# ============================================================================
# Core Tests
# ============================================================================

# Basic functionality test
add_executable(symbolic3_basic_test test/basic_test.cpp)
target_link_libraries(symbolic3_basic_test PRIVATE symbolic3 unit)
add_test(NAME symbolic3_basic_test COMMAND symbolic3_basic_test)
set_tests_properties(symbolic3_basic_test PROPERTIES LABELS "symbolic3;base")

# Pattern matching tests
add_executable(matching_test test/matching_test.cpp)
target_link_libraries(matching_test PRIVATE symbolic3 unit)
add_test(NAME symbolic3_matching COMMAND matching_test)
set_tests_properties(symbolic3_matching PROPERTIES LABELS "symbolic3;base")

add_executable(pattern_binding_test test/pattern_binding_test.cpp)
target_link_libraries(pattern_binding_test PRIVATE symbolic3 unit)
add_test(NAME symbolic3_pattern_binding COMMAND pattern_binding_test)
set_tests_properties(symbolic3_pattern_binding PROPERTIES LABELS "symbolic3;base")

# Composable predicates tests
add_executable(composable_predicates_test test/composable_predicates_test.cpp)
target_link_libraries(composable_predicates_test PRIVATE symbolic3 unit)
add_test(NAME symbolic3_composable_predicates COMMAND composable_predicates_test)
set_tests_properties(symbolic3_composable_predicates PROPERTIES LABELS "symbolic3;base;predicates")

# ============================================================================
# Calculus Tests
# ============================================================================

# Derivative tests
add_executable(derivative_test test/derivative_test.cpp)
target_link_libraries(derivative_test PRIVATE symbolic3)
add_test(NAME symbolic3_derivative COMMAND derivative_test)
set_tests_properties(symbolic3_derivative PROPERTIES LABELS "symbolic3;calculus")

# Evaluation tests
add_executable(evaluate_test test/evaluate_test.cpp)
target_link_libraries(evaluate_test PRIVATE symbolic3 unit)
add_test(NAME symbolic3_evaluate COMMAND evaluate_test)
set_tests_properties(symbolic3_evaluate PROPERTIES LABELS "symbolic3;calculus")

# ============================================================================
# Simplification Tests
# ============================================================================

# Consolidated simplification tests
add_executable(simplification_basic_test test/simplification_basic_test.cpp)
target_link_libraries(simplification_basic_test PRIVATE symbolic3 unit)
add_test(NAME symbolic3_simplification_basic COMMAND simplification_basic_test)
set_tests_properties(symbolic3_simplification_basic PROPERTIES LABELS "symbolic3;consolidated;simplification")

add_executable(simplification_advanced_test test/simplification_advanced_test.cpp)
target_link_libraries(simplification_advanced_test PRIVATE symbolic3 unit)
add_test(NAME symbolic3_simplification_advanced COMMAND simplification_advanced_test)
set_tests_properties(symbolic3_simplification_advanced PROPERTIES LABELS "symbolic3;consolidated;simplification")

add_executable(simplification_pipeline_test test/simplification_pipeline_test.cpp)
target_link_libraries(simplification_pipeline_test PRIVATE symbolic3 unit)
add_test(NAME symbolic3_simplification_pipeline COMMAND simplification_pipeline_test)
set_tests_properties(symbolic3_simplification_pipeline PROPERTIES LABELS "symbolic3;consolidated;simplification")

# Two-stage simplification pipeline tests
add_executable(two_stage_simplify_test test/two_stage_simplify_test.cpp)
target_link_libraries(two_stage_simplify_test PRIVATE symbolic3 unit)
add_test(NAME symbolic3_two_stage_simplify COMMAND two_stage_simplify_test)
set_tests_properties(symbolic3_two_stage_simplify PROPERTIES LABELS "symbolic3;simplification")

# Oscillation prevention tests - verifies rules don't create infinite loops
add_executable(oscillation_prevention_test test/oscillation_prevention_test.cpp)
target_link_libraries(oscillation_prevention_test PRIVATE symbolic3 unit)
add_test(NAME symbolic3_oscillation_prevention COMMAND oscillation_prevention_test)
set_tests_properties(symbolic3_oscillation_prevention PROPERTIES LABELS "symbolic3;simplification;stability")

# ============================================================================
# Term Structure and Ordering Tests
# ============================================================================

# Term ordering tests (algebraic term grouping for addition)
add_executable(term_ordering_test test/term_ordering_test.cpp)
target_link_libraries(term_ordering_test PRIVATE symbolic3 unit)
add_test(NAME symbolic3_term_ordering COMMAND term_ordering_test)
set_tests_properties(symbolic3_term_ordering PROPERTIES LABELS "symbolic3;term_structure;simplification")

# Multiplication ordering tests (power grouping)
add_executable(mult_ordering_test test/mult_ordering_test.cpp)
target_link_libraries(mult_ordering_test PRIVATE symbolic3 unit)
add_test(NAME symbolic3_mult_ordering COMMAND mult_ordering_test)
set_tests_properties(symbolic3_mult_ordering PROPERTIES LABELS "symbolic3;term_structure;simplification")

# Term structure tests (sophisticated sorting for term collection)
add_executable(term_structure_test test/term_structure_test.cpp)
target_link_libraries(term_structure_test PRIVATE symbolic3 unit)
add_test(NAME symbolic3_term_structure COMMAND term_structure_test)
set_tests_properties(symbolic3_term_structure PROPERTIES LABELS "symbolic3;term_structure")

# Sophisticated sorting demo
add_executable(sophisticated_sorting_demo test/sophisticated_sorting_demo.cpp)
target_link_libraries(sophisticated_sorting_demo PRIVATE symbolic3 unit)
add_test(NAME symbolic3_sophisticated_sorting_demo COMMAND sophisticated_sorting_demo)
set_tests_properties(symbolic3_sophisticated_sorting_demo PROPERTIES LABELS "symbolic3;demo")

# ============================================================================
# Exact Math Tests
# ============================================================================

# Fraction test - exact rational arithmetic
add_executable(fraction_test test/fraction_test.cpp)
target_link_libraries(fraction_test PRIVATE symbolic3 unit)
add_test(NAME symbolic3_fraction COMMAND fraction_test)
set_tests_properties(symbolic3_fraction PROPERTIES LABELS "symbolic3;exact_math")

# Exact Division Test - Automatic promotion of integer division to fractions
add_executable(exact_division_test test/exact_division_test.cpp)
target_link_libraries(exact_division_test PRIVATE symbolic3 unit)
add_test(NAME symbolic3_exact_division COMMAND exact_division_test)
set_tests_properties(symbolic3_exact_division PROPERTIES LABELS "symbolic3;exact_math")

# Hyperbolic Functions Test - sinh, cosh, tanh
add_executable(hyperbolic_test test/hyperbolic_test.cpp)
target_link_libraries(hyperbolic_test PRIVATE symbolic3 unit)
add_test(NAME symbolic3_hyperbolic COMMAND hyperbolic_test)
set_tests_properties(symbolic3_hyperbolic PROPERTIES LABELS "symbolic3;transcendental")

# Compile-time Debugging Utilities Test
add_executable(debug_test test/debug_test.cpp)
target_link_libraries(debug_test PRIVATE symbolic3 unit)
add_test(NAME symbolic3_debug COMMAND debug_test)
set_tests_properties(symbolic3_debug PROPERTIES LABELS "symbolic3;utilities")

# Operator Display Traits Test
add_executable(operator_display_test test/operator_display_test.cpp)
target_link_libraries(operator_display_test PRIVATE symbolic3 unit)
add_test(NAME symbolic3_operator_display COMMAND operator_display_test)
set_tests_properties(symbolic3_operator_display PROPERTIES LABELS "symbolic3;display")

# String Conversion Test - compile-time and runtime string conversion with custom variable names
add_executable(to_string_test test/to_string_test.cpp)
target_link_libraries(to_string_test PRIVATE symbolic3 unit)
add_test(NAME symbolic3_to_string COMMAND to_string_test)
set_tests_properties(symbolic3_to_string PROPERTIES LABELS "symbolic3;string_conversion;utilities")

# Pretty Print Test - PRETTY_PRINT macro with auto-generated symbol names
add_executable(pretty_print_test test/pretty_print_test.cpp)
target_link_libraries(pretty_print_test PRIVATE symbolic3 unit)
add_test(NAME symbolic3_pretty_print COMMAND pretty_print_test)
set_tests_properties(symbolic3_pretty_print PROPERTIES LABELS "symbolic3;printing;macros")


