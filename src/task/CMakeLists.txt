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

add_executable(concepts_test concepts_test.cpp)
target_link_libraries(concepts_test task unit)
add_test(NAME concepts_test COMMAND concepts_test)
set_property(TEST concepts_test PROPERTY LABELS base)

add_executable(sender_basics_test sender_basics_test.cpp)
target_link_libraries(sender_basics_test task unit)
add_test(NAME sender_basics_test COMMAND sender_basics_test)
set_property(TEST sender_basics_test PROPERTY LABELS base)

add_executable(sender_advanced_test sender_advanced_test.cpp)
target_link_libraries(sender_advanced_test task unit)
add_test(NAME sender_advanced_test COMMAND sender_advanced_test)
set_property(TEST sender_advanced_test PROPERTY LABELS base)

add_executable(schedulers_test schedulers_test.cpp)
target_link_libraries(schedulers_test task unit event_loop threadpool)
add_test(NAME schedulers_test COMMAND schedulers_test)
set_property(TEST schedulers_test PROPERTY LABELS base)

# Comprehensive whenAll test - now enabled with recursive inheritance storage
# Previously disabled because std::tuple required movable operation states.
# Now uses OperationTuple (recursive template inheritance) for move-free storage.
add_executable(when_all_test when_all_test.cpp)
target_link_libraries(when_all_test task unit threadpool)
add_test(NAME when_all_test COMMAND when_all_test)
set_property(TEST when_all_test PROPERTY LABELS base)

# WhenAny race composition test
add_executable(when_any_test when_any_test.cpp)
target_link_libraries(when_any_test task unit threadpool)
add_test(NAME when_any_test COMMAND when_any_test)
set_property(TEST when_any_test PROPERTY LABELS base)

# Stop token infrastructure test
add_executable(stop_token_test stop_token_test.cpp)
target_link_libraries(stop_token_test task unit)
add_test(NAME stop_token_test COMMAND stop_token_test)
set_property(TEST stop_token_test PROPERTY LABELS base)

# Environment and query infrastructure test
add_executable(env_test env_test.cpp)
target_link_libraries(env_test task unit event_loop threadpool)
add_test(NAME env_test COMMAND env_test)
set_property(TEST env_test PROPERTY LABELS base)

# Scheduler transition algorithms test (on, etc.)
add_executable(on_test on_test.cpp)
target_link_libraries(on_test task unit threadpool)
add_test(NAME on_test COMMAND on_test)
set_property(TEST on_test PROPERTY LABELS base)

# Repeat algorithms test
add_executable(repeat_effect_test repeat_effect_test.cpp)
target_link_libraries(repeat_effect_test task unit)
add_test(NAME repeat_effect_test COMMAND repeat_effect_test)
set_property(TEST repeat_effect_test PROPERTY LABELS base)

# Stopped channel handling test (letStopped, uponStopped)
add_executable(stopped_test stopped_test.cpp)
target_link_libraries(stopped_test task unit)
add_test(NAME stopped_test COMMAND stopped_test)
set_property(TEST stopped_test PROPERTY LABELS base)
