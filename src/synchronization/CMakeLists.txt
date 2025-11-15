add_library(guarded INTERFACE guarded.h)
target_include_directories(guarded INTERFACE
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(guarded INTERFACE tempura_flags)
add_library(tempura::guarded ALIAS guarded)

add_library(hierarchical_mutex INTERFACE hierarchical_mutex.h)
target_include_directories(hierarchical_mutex INTERFACE
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(hierarchical_mutex INTERFACE tempura_flags)
add_library(tempura::hierarchical_mutex ALIAS hierarchical_mutex)

add_library(fifo_queue INTERFACE fifo_queue.h)
target_include_directories(fifo_queue INTERFACE
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(fifo_queue INTERFACE tempura_flags)
add_library(tempura::fifo_queue ALIAS fifo_queue)

add_library(threadsafe_queue INTERFACE threadsafe_queue.h)
target_include_directories(threadsafe_queue INTERFACE
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(threadsafe_queue INTERFACE tempura_flags)
add_library(tempura::threadsafe_queue ALIAS threadsafe_queue)

add_library(threadsafe_stack INTERFACE threadsafe_stack.h)
target_include_directories(threadsafe_stack INTERFACE
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(threadsafe_stack INTERFACE tempura_flags)
add_library(tempura::threadsafe_stack ALIAS threadsafe_stack)

add_library(event_loop INTERFACE event_loop.h)
target_include_directories(event_loop INTERFACE
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(event_loop INTERFACE tempura_flags)
add_library(tempura::event_loop ALIAS event_loop)

add_library(threadpool SHARED)
target_sources(threadpool PUBLIC threadpool.h threadpool.cpp)
target_include_directories(threadpool PUBLIC
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(threadpool PUBLIC tempura_flags)
add_library(tempura::threadpool ALIAS threadpool)

if(NOT TEMPURA_BUILD_TESTS)
  return()
endif()

add_executable(guarded_test guarded_test.cpp)
target_link_libraries(guarded_test guarded unit)

add_executable(hierarchical_mutex_test hierarchical_mutex_test.cpp)
target_link_libraries(hierarchical_mutex_test hierarchical_mutex unit)

add_executable(fifo_queue_test fifo_queue_test.cpp)
target_link_libraries(fifo_queue_test fifo_queue unit benchmark profiler)

add_executable(threadsafe_queue_test threadsafe_queue_test.cpp)
target_link_libraries(threadsafe_queue_test threadsafe_queue unit)

add_executable(threadsafe_stack_test threadsafe_stack_test.cpp)
target_link_libraries(threadsafe_stack_test threadsafe_stack unit)

add_executable(threadpool_test threadpool_test.cpp)
target_link_libraries(threadpool_test threadpool unit profiler)

add_executable(event_loop_test event_loop_test.cpp)
target_link_libraries(event_loop_test event_loop unit)
