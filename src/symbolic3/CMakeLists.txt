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

# Composable Predicates Tests
add_executable(composable_predicates_test test/composable_predicates_test.cpp)
target_link_libraries(composable_predicates_test PRIVATE symbolic3 unit)
add_test(NAME symbolic3_composable_predicates COMMAND composable_predicates_test)
set_tests_properties(symbolic3_composable_predicates PROPERTIES LABELS "symbolic3;base;predicates")

# Simplify Tests (consolidated into simplification_basic/advanced/pipeline)
# add_executable(simplify_test test/simplify_test.cpp)
# target_link_libraries(simplify_test PRIVATE symbolic3)
# add_test(NAME symbolic3_simplify COMMAND simplify_test)
# set_tests_properties(symbolic3_simplify PROPERTIES LABELS "symbolic3")



# Note: Full tests (strategy_test, traversal_test, transforms_test) are TODO
# They need fixes to the type predicates and strategy implementations

# add_executable(transcendental_test test/transcendental_test.cpp)
# target_link_libraries(transcendental_test PRIVATE symbolic3)
# add_test(NAME symbolic3_transcendental COMMAND transcendental_test)
# set_tests_properties(symbolic3_transcendental PROPERTIES LABELS "symbolic3")

# Traversal + Simplification Integration Test (merged into pipeline tests)
# add_executable(traversal_simplify_test test/traversal_simplify_test.cpp)
# target_link_libraries(traversal_simplify_test PRIVATE symbolic3)
# add_test(NAME symbolic3_traversal_simplify COMMAND traversal_simplify_test)
# set_tests_properties(symbolic3_traversal_simplify PROPERTIES LABELS "symbolic3;integration")

# Full Simplification Pipelines Test (now simplification_pipeline_test)
# add_executable(full_simplify_test test/full_simplify_test.cpp)
# target_link_libraries(full_simplify_test PRIVATE symbolic3)
# add_test(NAME symbolic3_full_simplify COMMAND full_simplify_test)
# set_tests_properties(symbolic3_full_simplify PROPERTIES LABELS "symbolic3;integration")

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

# Consolidated Simplification Tests
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

# Term-Structure-Aware Ordering Test (tests algebraic term grouping for addition)
add_executable(term_ordering_test test/term_ordering_test.cpp)
target_link_libraries(term_ordering_test PRIVATE symbolic3 unit)
add_test(NAME symbolic3_term_ordering COMMAND term_ordering_test)
set_tests_properties(symbolic3_term_ordering PROPERTIES LABELS "symbolic3;term_structure;simplification")

# Multiplication Term-Structure-Aware Ordering Test (tests power grouping)
add_executable(mult_ordering_test test/mult_ordering_test.cpp)
target_link_libraries(mult_ordering_test PRIVATE symbolic3 unit)
add_test(NAME symbolic3_mult_ordering COMMAND mult_ordering_test)
set_tests_properties(symbolic3_mult_ordering PROPERTIES LABELS "symbolic3;term_structure;simplification")

# Term Collecting Debug (quick debug tool - not a formal test)
add_executable(term_collecting_debug test/term_collecting_debug.cpp)
target_link_libraries(term_collecting_debug PRIVATE symbolic3)

# Term Collection Test (isolated test for debugging) - TODO: file missing (term_collection_test.cpp doesn't exist)
# add_executable(term_collection_test test/term_collection_test.cpp)
# target_link_libraries(term_collection_test PRIVATE symbolic3 unit)
# add_test(NAME symbolic3_term_collection COMMAND term_collection_test)
# set_tests_properties(symbolic3_term_collection PROPERTIES LABELS "symbolic3;term_structure")

# Manual Step Debugging (understanding simplification iteration)
add_executable(manual_step_test test/manual_step_test.cpp)
target_link_libraries(manual_step_test PRIVATE symbolic3)

# Nested Simplification Test (debug innermost traversal) - TODO: file missing
# add_executable(nested_simplify_test test/nested_simplify_test.cpp)
# target_link_libraries(nested_simplify_test PRIVATE symbolic3)

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

# Canonical Form Test - TODO: file missing, functionality merged into term_structure_test
# add_executable(canonical_test test/canonical_test.cpp)
# target_link_libraries(canonical_test PRIVATE symbolic3 unit)
# add_test(NAME symbolic3_canonical COMMAND canonical_test)
# set_tests_properties(symbolic3_canonical PROPERTIES LABELS "symbolic3;canonical_form")

# Term Structure Test - sophisticated sorting for term collection
add_executable(term_structure_test test/term_structure_test.cpp)
target_link_libraries(term_structure_test PRIVATE symbolic3 unit)
add_test(NAME symbolic3_term_structure COMMAND term_structure_test)
set_tests_properties(symbolic3_term_structure PROPERTIES LABELS "symbolic3;term_structure")

# Sophisticated Sorting Demo - shows how algebraic sorting enables term collection
add_executable(sophisticated_sorting_demo test/sophisticated_sorting_demo.cpp)
target_link_libraries(sophisticated_sorting_demo PRIVATE symbolic3 unit)
add_test(NAME symbolic3_sophisticated_sorting_demo COMMAND sophisticated_sorting_demo)
set_tests_properties(symbolic3_sophisticated_sorting_demo PROPERTIES LABELS "symbolic3;demo")

# Two-Stage Simplify Test - exhaustive test suite for the two-stage simplification pipeline
add_executable(two_stage_simplify_test test/two_stage_simplify_test.cpp)
target_link_libraries(two_stage_simplify_test PRIVATE symbolic3 unit)
add_test(NAME symbolic3_two_stage_simplify COMMAND two_stage_simplify_test)
set_tests_properties(symbolic3_two_stage_simplify PROPERTIES LABELS "symbolic3;simplification")


