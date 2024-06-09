add_library(fifo_queue INTERFACE fifo_queue.h)

add_executable(fifo_queue_test fifo_queue_test.cpp)
target_link_libraries(fifo_queue_test fifo_queue profiler unit benchmark)

add_library(profiler
  STATIC profiler.h profiler.cpp)

add_library(benchmark INTERFACE benchmark.h)

add_library(simplify INTERFACE simplify.h)

add_executable(simplify_test simplify_test.cpp)
target_link_libraries(simplify_test type_list simplify symbolic unit)

add_library(symbolic INTERFACE symbolic.h)

add_executable(symbolic_test symbolic_test.cpp)
target_link_libraries(symbolic_test type_list symbolic unit)

add_library(type_list INTERFACE type_list.h)
target_link_libraries(type_list INTERFACE type_name)

add_executable(type_list_test type_list_test.cpp)
target_link_libraries(type_list_test type_list unit)

add_library(type_name INTERFACE type_name.h)

add_executable(type_name_test type_name_test.cpp)
target_link_libraries(type_name_test type_name unit)

add_library(unit INTERFACE unit.h)
