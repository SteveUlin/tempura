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
