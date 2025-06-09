add_library(meta INTERFACE)
target_include_directories(meta INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

add_library(vector_to_array INTERFACE vector_to_array.h)
add_executable(vector_to_array_test vector_to_array_test.cpp)
target_link_libraries(vector_to_array_test vector_to_array unit)
