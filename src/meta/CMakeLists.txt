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

if(NOT TEMPURA_BUILD_TESTS)
  return()
endif()

add_executable(vector_to_array_test vector_to_array_test.cpp)
target_link_libraries(vector_to_array_test vector_to_array unit)

add_executable(type_sort_test test/type_sort_test.cpp)
target_link_libraries(type_sort_test PRIVATE meta unit)
add_test(NAME meta_type_sort COMMAND type_sort_test)

add_executable(macro_utils_test test/macro_utils_test.cpp)
target_link_libraries(macro_utils_test PRIVATE meta unit)
add_test(NAME meta_macro_utils COMMAND macro_utils_test)

add_executable(static_string_display_test test/static_string_display_test.cpp)
target_link_libraries(static_string_display_test PRIVATE meta unit)
add_test(NAME meta_static_string_display COMMAND static_string_display_test)
set_tests_properties(meta_static_string_display PROPERTIES LABELS "meta;utilities;debugging")
