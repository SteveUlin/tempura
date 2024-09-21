add_library(dual INTERFACE dual.h)
add_executable(dual_test dual_test.cpp)
target_link_libraries(dual_test dual unit)
