add_library(meta INTERFACE)
target_include_directories(meta INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

add_library(vector_to_array INTERFACE vector_to_array.h)
add_executable(vector_to_array_test vector_to_array_test.cpp)
target_link_libraries(vector_to_array_test vector_to_array unit)

add_executable(type_sort_test test/type_sort_test.cpp)
target_link_libraries(type_sort_test PRIVATE meta unit)
add_test(NAME meta_type_sort COMMAND type_sort_test)

add_executable(macro_utils_test test/macro_utils_test.cpp)
target_link_libraries(macro_utils_test PRIVATE meta unit)
add_test(NAME meta_macro_utils COMMAND macro_utils_test)
