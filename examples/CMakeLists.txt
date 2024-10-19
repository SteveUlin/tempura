add_executable(hello_world_main hello_world.cpp)

add_executable(profile_main profile.cpp)
target_link_libraries(profile_main PRIVATE profiler)

add_executable(sample_normal_main sample_normal.cpp)

# add_executable(single_producer_consumer_queue single_producer_consumer_queue.cpp)
# target_link_libraries(single_producer_consumer_queue PRIVATE profiler)

# add_executable(unit_main unit_main.cpp)
# target_link_libraries(unit_main PRIVATE unit)

