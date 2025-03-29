add_executable(hello_world_main hello_world.cpp)

add_executable(hessian_main hessian.cpp)

add_executable(profile_main profile.cpp)
target_link_libraries(profile_main PRIVATE profiler)

add_executable(sample_normal_main sample_normal.cpp)

add_executable(normal_reverse_autodiff_main normal_reverse_autodiff.cpp)

add_executable(histogram histogram_main.cpp)
add_executable(plot plot_main.cpp)
add_executable(text_size text_size.cpp)
add_executable(symbols_main symbols.cpp)
add_executable(image_main image_main.cpp)

# add_executable(single_producer_consumer_queue single_producer_consumer_queue.cpp)
# target_link_libraries(single_producer_consumer_queue PRIVATE profiler)

# add_executable(unit_main unit_main.cpp)
# target_link_libraries(unit_main PRIVATE unit)

