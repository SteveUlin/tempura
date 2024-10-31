add_library(dual INTERFACE dual.h)
add_executable(dual_test dual_test.cpp)
target_link_libraries(dual_test dual unit)

add_library(taylor INTERFACE taylor.h)
# add_executable(taylor_test taylor_test.cpp)
# target_link_libraries(taylor_test taylor unit)

add_library(node INTERFACE node.h)
add_executable(node_test node_test.cpp)
target_link_libraries(node_test node unit)

add_library(util INTERFACE util.h)
add_executable(util_test util_test.cpp)
target_link_libraries(util_test util unit)
