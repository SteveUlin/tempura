# Symbolic2 - Modern C++26 Symbolic Mathematics Library

# Core library (header-only)
add_library(symbolic2_core INTERFACE)
target_sources(symbolic2_core INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/symbolic.h
)
target_include_directories(symbolic2_core INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)
target_link_libraries(symbolic2_core INTERFACE
    meta
)

# Core tests
add_executable(symbolic2_test test/symbolic_test.cpp)
target_link_libraries(symbolic2_test PRIVATE symbolic2_core unit)
add_test(NAME symbolic2_test COMMAND symbolic2_test)

add_executable(symbolic2_simplify_test test/simplify_test.cpp)
target_link_libraries(symbolic2_simplify_test PRIVATE symbolic2_core unit)
add_test(NAME symbolic2_simplify_test COMMAND symbolic2_simplify_test)

add_executable(symbolic2_simplify_stress_test test/simplify_stress_test.cpp)
target_link_libraries(symbolic2_simplify_stress_test PRIVATE symbolic2_core unit)
add_test(NAME symbolic2_simplify_stress_test COMMAND symbolic2_simplify_stress_test)

add_executable(symbolic2_operators_test test/operators_test.cpp)
target_link_libraries(symbolic2_operators_test PRIVATE symbolic2_core unit)
add_test(NAME symbolic2_operators_test COMMAND symbolic2_operators_test)

# Pattern matching test
add_executable(symbolic2_pattern_matching_test test/pattern_matching_test.cpp)
target_link_libraries(symbolic2_pattern_matching_test PRIVATE symbolic2_core)
add_test(NAME symbolic2_pattern_matching_test COMMAND symbolic2_pattern_matching_test)

# Pattern matching predicate test
add_executable(symbolic2_pattern_matching_predicate_test test/pattern_matching_predicate_test.cpp)
target_link_libraries(symbolic2_pattern_matching_predicate_test PRIVATE symbolic2_core)
add_test(NAME symbolic2_pattern_matching_predicate_test COMMAND symbolic2_pattern_matching_predicate_test)

add_executable(symbolic2_pattern_matching_repeated_vars_test test/pattern_matching_repeated_vars_test.cpp)
target_link_libraries(symbolic2_pattern_matching_repeated_vars_test PRIVATE symbolic2_core)
add_test(NAME symbolic2_pattern_matching_repeated_vars_test COMMAND symbolic2_pattern_matching_repeated_vars_test)

# Derivative support
add_library(symbolic2_derivative INTERFACE)
target_sources(symbolic2_derivative INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/derivative.h
)
target_link_libraries(symbolic2_derivative INTERFACE symbolic2_core)

add_executable(symbolic2_derivative_test test/derivative_test.cpp)
target_link_libraries(symbolic2_derivative_test PRIVATE symbolic2_derivative unit)
add_test(NAME symbolic2_derivative_test COMMAND symbolic2_derivative_test)

# ToString support (to be added)
# add_library(symbolic2_to_string INTERFACE)
# target_sources(symbolic2_to_string INTERFACE
#     ${CMAKE_CURRENT_SOURCE_DIR}/to_string.h
# )
# target_link_libraries(symbolic2_to_string INTERFACE symbolic2_core)

# Examples
add_executable(symbolic2_basic_example examples/basic_usage.cpp)
target_link_libraries(symbolic2_basic_example PRIVATE symbolic2_core)

# Design exploration demo
add_executable(simplify_design_demo examples/simplify_design_demo.cpp)
target_link_libraries(simplify_design_demo PRIVATE symbolic2_core)

# Aesthetic variations demo
add_executable(aesthetic_demo examples/aesthetic_demo.cpp)
target_link_libraries(aesthetic_demo PRIVATE symbolic2_core)

# Pattern-based rules demo
add_executable(pattern_demo examples/pattern_demo.cpp)
target_link_libraries(pattern_demo PRIVATE symbolic2_core)

# Note: Uncomment as features are added
# add_executable(symbolic2_advanced_example examples/advanced_simplify.cpp)
# target_link_libraries(symbolic2_advanced_example PRIVATE symbolic2_core)
add_executable(symbolic2_recursive_rewrite_test test/recursive_rewrite_test.cpp)
target_link_libraries(symbolic2_recursive_rewrite_test PRIVATE unit)
