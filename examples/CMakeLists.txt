add_subdirectory(idioms)

# Examples use target-based includes via target_link_libraries()
# (Include paths come from the libraries they link to)

# Apply compiler flags to all examples
link_libraries(tempura_flags)

add_executable(hello_world_main hello_world.cpp)

# add_executable(hessian_main hessian.cpp)  # Disabled - autodiff has compilation issues

add_executable(profile_main profile.cpp)
target_link_libraries(profile_main PRIVATE profiler)

add_executable(sample_normal_main sample_normal.cpp)
target_link_libraries(sample_normal_main PRIVATE bayes)

# add_executable(normal_reverse_autodiff_main normal_reverse_autodiff.cpp)  # Disabled - autodiff has compilation issues

add_executable(histogram histogram_main.cpp)
target_link_libraries(histogram PRIVATE bayes)

add_executable(plot plot_main.cpp)
target_link_libraries(plot PRIVATE bayes)
add_executable(text_size text_size.cpp)
# add_executable(symbols_main symbols.cpp)  # Disabled - uses old symbolic system
add_executable(symbolic3_debug_demo symbolic3_debug_demo.cpp)
target_link_libraries(symbolic3_debug_demo PRIVATE symbolic3)
add_executable(symbolic3_simplify_demo symbolic3_simplify_demo.cpp)
target_link_libraries(symbolic3_simplify_demo PRIVATE symbolic3)
add_executable(advanced_simplify_demo advanced_simplify_demo.cpp)
target_link_libraries(advanced_simplify_demo PRIVATE symbolic3)
add_executable(hyperbolic_demo hyperbolic_demo.cpp)
target_link_libraries(hyperbolic_demo PRIVATE symbolic3)
add_executable(term_sorting_visual term_sorting_visual.cpp)
target_link_libraries(term_sorting_visual PRIVATE symbolic3)
add_executable(fraction_demo fraction_demo.cpp)
target_link_libraries(fraction_demo PRIVATE symbolic3)
add_executable(pretty_print_demo pretty_print_demo.cpp)
target_link_libraries(pretty_print_demo PRIVATE symbolic3)
add_executable(string_display_debugging string_display_debugging.cpp)
target_link_libraries(string_display_debugging PRIVATE symbolic3)
# add_executable(constexpr_debugging_demo constexpr_debugging_demo.cpp)  # Disabled - has compilation errors
# target_link_libraries(constexpr_debugging_demo PRIVATE symbolic3)
# add_executable(simplify_demo simplify_demo.cpp)  # Disabled - needs StaticString formatter
# target_link_libraries(simplify_demo PRIVATE symbolic3)
add_executable(image_main image_main.cpp)
add_executable(simd_main simd_main.cpp)
target_link_libraries(simd_main PRIVATE profiler)

add_executable(scheduler_demo scheduler_demo.cpp)

add_executable(factorial_demo factorial_demo.cpp)
target_link_libraries(factorial_demo PRIVATE dynamic_bigint)

# add_executable(glfw_main glfw_main.cpp)  # Disabled - missing GL/gl.h
# target_link_libraries(glfw_main PRIVATE glfw OpenGL::GL)
# add_executable(hello_triangle_main hello_triangle.cpp)  # Disabled - missing GL/gl.h
# target_link_libraries(hello_triangle_main PRIVATE glfw Vulkan::Vulkan)

# add_executable(single_producer_consumer_queue single_producer_consumer_queue.cpp)
# target_link_libraries(single_producer_consumer_queue PRIVATE profiler)

# add_executable(unit_main unit_main.cpp)
# target_link_libraries(unit_main PRIVATE unit)

