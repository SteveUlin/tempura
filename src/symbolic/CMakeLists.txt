add_library(derivative INTERFACE derivative.h)
add_executable(derivative_test derivative_test.cpp)
target_link_libraries(derivative_test derivative matchers unit)

add_library(matchers INTERFACE matchers.h)
add_executable(matchers_test matchers_test.cpp)
target_link_libraries(matchers_test matchers unit)

add_library(meta INTERFACE meta.h)
add_executable(meta_test meta_test.cpp)
target_link_libraries(meta_test meta unit)

add_library(operators INTERFACE operators.h)
add_executable(operators_test operators_test.cpp)
target_link_libraries(operators_test operators unit)

add_library(simplify INTERFACE simplify.h)
add_executable(simplify_test simplify_test.cpp)
target_link_libraries(simplify_test simplify unit)

add_library(symbolic INTERFACE symbolic.h)
add_executable(symbolic_test symbolic_test.cpp)
target_link_libraries(symbolic_test symbolic unit)

add_library(to_string INTERFACE to_string.h)
add_executable(to_string_test to_string_test.cpp)
target_link_libraries(to_string_test to_string unit)

add_library(type_list INTERFACE type_list.h)
add_executable(type_list_test type_list_test.cpp)
target_link_libraries(type_list_test type_list unit)

add_library(type_name INTERFACE type_name.h)
add_executable(type_name_test type_name_test.cpp)
target_link_libraries(type_name_test type_name unit)

