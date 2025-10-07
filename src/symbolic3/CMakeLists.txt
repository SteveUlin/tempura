# Symbolic3: Combinator-based symbolic computation system

add_library(symbolic3 INTERFACE)
target_include_directories(symbolic3 INTERFACE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(symbolic3 INTERFACE meta)

# Tests
add_executable(symbolic3_basic_test
  test/basic_test.cpp
)

target_link_libraries(symbolic3_basic_test PRIVATE symbolic3 unit)

add_test(NAME symbolic3_basic_test COMMAND symbolic3_basic_test)
set_tests_properties(symbolic3_basic_test PROPERTIES LABELS "symbolic3;base")

# Matching Tests
add_executable(matching_test test/matching_test.cpp)
target_link_libraries(matching_test PRIVATE symbolic3 unit)
add_test(NAME symbolic3_matching COMMAND matching_test)
set_tests_properties(symbolic3_matching PROPERTIES LABELS "symbolic3;base")

# Pattern Binding Tests
add_executable(pattern_binding_test test/pattern_binding_test.cpp)
target_link_libraries(pattern_binding_test PRIVATE symbolic3 unit)
add_test(NAME symbolic3_pattern_binding COMMAND pattern_binding_test)
set_tests_properties(symbolic3_pattern_binding PROPERTIES LABELS "symbolic3;base")

# Simplify Tests
add_executable(simplify_test test/simplify_test.cpp)
target_link_libraries(simplify_test PRIVATE symbolic3)
add_test(NAME symbolic3_simplify COMMAND simplify_test)
set_tests_properties(symbolic3_simplify PROPERTIES LABELS "symbolic3")



# Note: Full tests (strategy_test, traversal_test, transforms_test) are TODO
# They need fixes to the type predicates and strategy implementations

add_executable(transcendental_test test/transcendental_test.cpp)
target_link_libraries(transcendental_test PRIVATE symbolic3)
add_test(NAME symbolic3_transcendental COMMAND transcendental_test)
set_tests_properties(symbolic3_transcendental PROPERTIES LABELS "symbolic3")

# Traversal + Simplification Integration Test
add_executable(traversal_simplify_test test/traversal_simplify_test.cpp)
target_link_libraries(traversal_simplify_test PRIVATE symbolic3)
add_test(NAME symbolic3_traversal_simplify COMMAND traversal_simplify_test)
set_tests_properties(symbolic3_traversal_simplify PROPERTIES LABELS "symbolic3;integration")

# Full Simplification Pipelines Test
add_executable(full_simplify_test test/full_simplify_test.cpp)
target_link_libraries(full_simplify_test PRIVATE symbolic3)
add_test(NAME symbolic3_full_simplify COMMAND full_simplify_test)
set_tests_properties(symbolic3_full_simplify PROPERTIES LABELS "symbolic3;integration")

# Derivative Tests
add_executable(derivative_test test/derivative_test.cpp)
target_link_libraries(derivative_test PRIVATE symbolic3)
add_test(NAME symbolic3_derivative COMMAND derivative_test)
set_tests_properties(symbolic3_derivative PROPERTIES LABELS "symbolic3;calculus")

# Evaluation Tests
add_executable(evaluate_test test/evaluate_test.cpp)
target_link_libraries(evaluate_test PRIVATE symbolic3 unit)
add_test(NAME symbolic3_evaluate COMMAND evaluate_test)
set_tests_properties(symbolic3_evaluate PROPERTIES LABELS "symbolic3;calculus")

# String conversion and debugging utilities test
add_executable(to_string_test test/to_string_test.cpp)
target_link_libraries(to_string_test PRIVATE symbolic3)
add_test(NAME symbolic3_to_string COMMAND to_string_test)
set_tests_properties(symbolic3_to_string PROPERTIES LABELS "symbolic3;utilities")

# Term Collecting and Canonical Ordering test
add_executable(term_collecting_test test/term_collecting_test.cpp)
target_link_libraries(term_collecting_test PRIVATE symbolic3)
add_test(NAME symbolic3_term_collecting COMMAND term_collecting_test)
set_tests_properties(symbolic3_term_collecting PROPERTIES LABELS "symbolic3;simplification")

# Advanced simplification rules test (logarithms, exponentials, trig identities)
add_executable(advanced_simplify_test test/advanced_simplify_test.cpp)
target_link_libraries(advanced_simplify_test PRIVATE symbolic3)
add_test(NAME symbolic3_advanced_simplify COMMAND advanced_simplify_test)
set_tests_properties(symbolic3_advanced_simplify PROPERTIES LABELS "symbolic3;simplification")

# Term Collecting Debug (quick debug tool - not a formal test)
add_executable(term_collecting_debug test/term_collecting_debug.cpp)
target_link_libraries(term_collecting_debug PRIVATE symbolic3)

# Manual Step Debugging (understanding simplification iteration)
add_executable(manual_step_test test/manual_step_test.cpp)
target_link_libraries(manual_step_test PRIVATE symbolic3)

# Nested Simplification Test (debug innermost traversal)
add_executable(nested_simplify_test test/nested_simplify_test.cpp)
target_link_libraries(nested_simplify_test PRIVATE symbolic3)

# Factoring Debug (test factoring rules in isolation)
add_executable(factoring_debug test/factoring_debug.cpp)
target_link_libraries(factoring_debug PRIVATE symbolic3)

# Constant Folding Test
add_executable(fold_test test/fold_test.cpp)
target_link_libraries(fold_test PRIVATE symbolic3)

# Pi and e Constants Test
add_executable(pi_e_test test/pi_e_test.cpp)
target_link_libraries(pi_e_test PRIVATE symbolic3)

# Fraction Test - Exact rational arithmetic
add_executable(fraction_test test/fraction_test.cpp)
target_link_libraries(fraction_test PRIVATE symbolic3 unit)
add_test(NAME symbolic3_fraction COMMAND fraction_test)
set_tests_properties(symbolic3_fraction PROPERTIES LABELS "symbolic3;exact_math")

# Compile-time Debugging Utilities Test
add_executable(debug_test test/debug_test.cpp)
target_link_libraries(debug_test PRIVATE symbolic3 unit)
add_test(NAME symbolic3_debug COMMAND debug_test)
set_tests_properties(symbolic3_debug PROPERTIES LABELS "symbolic3;utilities")


