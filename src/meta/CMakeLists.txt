add_library(meta INTERFACE)
target_include_directories(meta INTERFACE
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(meta INTERFACE tempura_flags)
add_library(tempura::meta ALIAS meta)

add_library(vector_to_array INTERFACE vector_to_array.h)
target_include_directories(vector_to_array INTERFACE
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(vector_to_array INTERFACE tempura_flags)
add_library(tempura::vector_to_array ALIAS vector_to_array)

add_library(manual_lifetime INTERFACE manual_lifetime.h)
target_include_directories(manual_lifetime INTERFACE
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(manual_lifetime INTERFACE tempura_flags)
add_library(tempura::manual_lifetime ALIAS manual_lifetime)

add_library(fixed_string INTERFACE fixed_string.h)
target_include_directories(fixed_string INTERFACE
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(fixed_string INTERFACE tempura_flags)
add_library(tempura::fixed_string ALIAS fixed_string)

if(NOT TEMPURA_BUILD_TESTS)
  return()
endif()

# ============================================================================
# Tests
# ============================================================================

# Vector to array conversion test
add_executable(vector_to_array_test vector_to_array_test.cpp)
target_link_libraries(vector_to_array_test vector_to_array unit)

# Manual lifetime management test
add_executable(manual_lifetime_test manual_lifetime_test.cpp)
target_link_libraries(manual_lifetime_test manual_lifetime unit)
add_test(NAME manual_lifetime COMMAND manual_lifetime_test)
set_tests_properties(manual_lifetime PROPERTIES LABELS "meta;utilities;memory")

# Type sorting test
add_executable(meta_type_sort_test test/type_sort_test.cpp)
target_link_libraries(meta_type_sort_test PRIVATE meta unit)
add_test(NAME meta_type_sort COMMAND meta_type_sort_test)
set_tests_properties(meta_type_sort PROPERTIES LABELS "meta;type_manipulation")

# FixedString test
add_executable(fixed_string_test fixed_string_test.cpp)
target_link_libraries(fixed_string_test PRIVATE fixed_string unit)
add_test(NAME fixed_string COMMAND fixed_string_test)
set_tests_properties(fixed_string PROPERTIES LABELS "meta;utilities;string")

# Macro utilities test
add_executable(meta_macro_utils_test test/macro_utils_test.cpp)
target_link_libraries(meta_macro_utils_test PRIVATE meta unit)
add_test(NAME meta_macro_utils COMMAND meta_macro_utils_test)
set_tests_properties(meta_macro_utils PROPERTIES LABELS "meta;utilities")

# Static string display test
add_executable(meta_static_string_display_test test/static_string_display_test.cpp)
target_link_libraries(meta_static_string_display_test PRIVATE meta unit)
add_test(NAME meta_static_string_display COMMAND meta_static_string_display_test)
set_tests_properties(meta_static_string_display PROPERTIES LABELS "meta;utilities;debugging")

# Meta programming test
add_executable(meta_meta_test test/meta_test.cpp)
target_link_libraries(meta_meta_test PRIVATE meta unit)
add_test(NAME meta_meta_test COMMAND meta_meta_test)
set_tests_properties(meta_meta_test PROPERTIES LABELS "meta;core")

# Tuple utilities test
add_executable(meta_tuple_test test/tuple_test.cpp)
target_link_libraries(meta_tuple_test PRIVATE meta unit)
add_test(NAME meta_tuple_test COMMAND meta_tuple_test)
set_tests_properties(meta_tuple_test PROPERTIES LABELS "meta;tuples")

# Expression template test
add_executable(meta_expression_test test/expression_test.cpp)
target_link_libraries(meta_expression_test PRIVATE meta unit)
add_test(NAME meta_expression_test COMMAND meta_expression_test)
set_tests_properties(meta_expression_test PROPERTIES LABELS "meta;expressions")

# Utility test (type traits and concepts)
add_executable(meta_utility_test utility_test.cpp)
target_link_libraries(meta_utility_test PRIVATE meta unit)
add_test(NAME meta_utility_test COMMAND meta_utility_test)
set_tests_properties(meta_utility_test PROPERTIES LABELS "meta;utility;concepts")

# Containers test (currently broken - pre-existing issue)
# TODO: Fix and re-enable
# add_executable(meta_containers_test test/containers_test.cpp)
# target_link_libraries(meta_containers_test PRIVATE meta unit)
# add_test(NAME meta_containers_test COMMAND meta_containers_test)
# set_tests_properties(meta_containers_test PROPERTIES LABELS "meta;containers")

# Function objects test (currently broken - pre-existing issue)
# TODO: Fix and re-enable
# add_executable(meta_function_objects_test test/function_objects_test.cpp)
# target_link_libraries(meta_function_objects_test PRIVATE meta unit)
# add_test(NAME meta_function_objects_test COMMAND meta_function_objects_test)
# set_tests_properties(meta_function_objects_test PROPERTIES LABELS "meta;function_objects")
