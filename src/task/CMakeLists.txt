# Task Execution Library - Sender/Receiver async system
#
# Interface library (header-only) for async task execution based on P2300

add_library(task INTERFACE task.h)
target_include_directories(task INTERFACE
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(task INTERFACE tempura_flags)
add_library(tempura::task ALIAS task)

if(NOT TEMPURA_BUILD_TESTS)
  return()
endif()

add_executable(task_test task_test.cpp)
target_link_libraries(task_test task unit)
add_test(NAME task_test COMMAND task_test)
set_property(TEST task_test PROPERTY LABELS base)

add_executable(schedulers_test schedulers_test.cpp)
target_link_libraries(schedulers_test task unit event_loop threadpool)
add_test(NAME schedulers_test COMMAND schedulers_test)
set_property(TEST schedulers_test PROPERTY LABELS base)
