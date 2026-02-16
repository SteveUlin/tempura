# Reflection experiments — quick-iteration sandbox
#
# Add executables here freely. Each target links tempura_flags for compiler
# settings and can pull in any library from src/ (meta, symbolic4, bayes, etc).
#
# Build a single experiment:
#   cmake --build build-reflection --target hello_reflection
#
# Build all experiments:
#   cmake --build build-reflection --target all

link_libraries(tempura_flags)

# --- Add experiments below ---

add_executable(hello_reflection hello_reflection.cpp)
target_link_libraries(hello_reflection PRIVATE meta)

add_executable(enum_kit enum_kit.cpp)
target_link_libraries(enum_kit PRIVATE meta)
