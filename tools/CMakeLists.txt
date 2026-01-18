# Tools for debugging and development

# Apply compiler flags to all tools
link_libraries(tempura_flags)

add_executable(pattern_debugger pattern_debugger.cpp)
target_link_libraries(pattern_debugger PRIVATE plot)

add_executable(generate_mosaic_patterns generate_mosaic_patterns.cpp)
