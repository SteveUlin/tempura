add_library(sin INTERFACE sin.h)
add_executable(sin_test sin_test.cpp)
target_link_libraries(sin_test sin unit)

add_library(cos INTERFACE cos.h)
add_executable(cos_test cos_test.cpp)
target_link_libraries(cos_test cos unit)

add_library(exp INTERFACE exp.h)
add_executable(exp_test exp_test.cpp)
target_link_libraries(exp_test exp unit)
