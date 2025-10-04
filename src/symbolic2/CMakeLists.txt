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
    meta_containers
    meta_function_objects
    meta_tags
    meta_tuple
    meta_type_id
    meta_utility
)

# Core tests
add_executable(symbolic2_test test/symbolic_test.cpp)
target_link_libraries(symbolic2_test PRIVATE symbolic2_core unit)
add_test(NAME symbolic2_test COMMAND symbolic2_test)

add_executable(symbolic2_simplify_test test/simplify_test.cpp)
target_link_libraries(symbolic2_simplify_test PRIVATE symbolic2_core unit)
add_test(NAME symbolic2_simplify_test COMMAND symbolic2_simplify_test)

# Derivative support (to be added)
# add_library(symbolic2_derivative INTERFACE)
# target_sources(symbolic2_derivative INTERFACE
#     ${CMAKE_CURRENT_SOURCE_DIR}/derivative.h
# )
# target_link_libraries(symbolic2_derivative INTERFACE symbolic2_core)

# add_executable(symbolic2_derivative_test test/derivative_test.cpp)
# target_link_libraries(symbolic2_derivative_test PRIVATE symbolic2_derivative unit)
# add_test(NAME symbolic2_derivative_test COMMAND symbolic2_derivative_test)

# ToString support (to be added)
# add_library(symbolic2_to_string INTERFACE)
# target_sources(symbolic2_to_string INTERFACE
#     ${CMAKE_CURRENT_SOURCE_DIR}/to_string.h
# )
# target_link_libraries(symbolic2_to_string INTERFACE symbolic2_core)

# Examples
add_executable(symbolic2_basic_example examples/basic_usage.cpp)
target_link_libraries(symbolic2_basic_example PRIVATE symbolic2_core)

# Note: Uncomment as features are added
# add_executable(symbolic2_advanced_example examples/advanced_simplify.cpp)
# target_link_libraries(symbolic2_advanced_example PRIVATE symbolic2_core)
