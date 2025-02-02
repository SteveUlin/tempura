# add_subdirectory(autodiff)
# add_subdirectory(cuda)
# add_subdirectory(matrix)
add_subdirectory(matrix2)
add_subdirectory(quadature)
# add_subdirectory(symbolic)
# add_subdirectory(bayes)

add_library(benchmark INTERFACE benchmark.h)

add_library(broadcast_array INTERFACE broadcast_array.h)
add_executable(broadcast_array_test broadcast_array_test.cpp)
target_link_libraries(broadcast_array_test broadcast_array unit)

add_library(profiler SHARED)
target_sources(profiler PUBLIC profiler.h profiler.cpp)

add_library(unit INTERFACE unit.h)

add_library(interpolate INTERFACE interpolate.h)

add_library(function_traits INTERFACE function_traits.h)

# add_library(fifo_queue INTERFACE fifo_queue.h)
# add_executable(fifo_queue_test fifo_queue_test.cpp)
# target_link_libraries(fifo_queue_test fifo_queue unit benchmark profiler)

add_library(guarded INTERFACE guarded.h)
add_executable(guarded_test guarded_test.cpp)
target_link_libraries(guarded_test guarded unit)

add_library(image INTERFACE image.h)
add_executable(image_test image_test.cpp)
target_link_libraries(image_test image unit)

add_library(json INTERFACE json.h)
add_executable(json_test json_test.cpp)
target_link_libraries(json_test json unit)


