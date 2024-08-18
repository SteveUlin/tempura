add_subdirectory(cuda)
add_subdirectory(matrix)

add_library(benchmark INTERFACE benchmark.h)
add_library(profiler STATIC profiler.h profiler.cpp)
add_library(unit INTERFACE unit.h)

add_library(fifo_queue INTERFACE fifo_queue.h)
add_executable(fifo_queue_test fifo_queue_test.cpp)
target_link_libraries(fifo_queue_test fifo_queue unit benchmark profiler)

add_library(guarded INTERFACE guarded.h)
add_executable(guarded_test guarded_test.cpp)
target_link_libraries(guarded_test guarded unit)

add_library(image INTERFACE image.h)
add_executable(image_test image_test.cpp)
