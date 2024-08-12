add_subdirectory(cuda)

add_library(benchmark INTERFACE benchmark.h)
add_library(profiler STATIC profiler.h profiler.cpp)
add_library(unit INTERFACE unit.h)

add_library(fifo_queue INTERFACE fifo_queue.h)
add_executable(fifo_queue_test fifo_queue_test.cpp)
target_link_libraries(fifo_queue_test fifo_queue unit benchmark profiler)

add_library(guarded INTERFACE guarded.h)
add_executable(guarded_test guarded_test.cpp)
target_link_libraries(guarded_test guarded unit)
