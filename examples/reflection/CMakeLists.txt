# C++26 Reflection Examples
#
# These examples require GCC with P2996 reflection support.
# Currently, this is available on GCC's devel/c++/p2996 branch.
#
# Once P2996 is merged to trunk, use:
#   nix develop .#trunk
#   g++ -std=c++26 -freflection -o example example.cpp

message(STATUS "Reflection examples require GCC P2996 branch - skipping CMake targets")
message(STATUS "See examples/reflection/README.md for manual compilation instructions")

# The examples are not added as CMake targets because:
# 1. Standard GCC trunk doesn't have the reflection operators yet
# 2. The P2996 branch needs to be built separately
#
# Manual compilation (once you have a P2996-enabled GCC):
#   g++ -std=c++26 -freflection -o basic_reflection basic_reflection.cpp
#   g++ -std=c++26 -freflection -o struct_iteration struct_iteration.cpp
#   g++ -std=c++26 -freflection -o enum_to_string enum_to_string.cpp
#   g++ -std=c++26 -freflection -o json_serialization json_serialization.cpp
#   g++ -std=c++26 -freflection -o define_aggregate define_aggregate.cpp
#   g++ -std=c++26 -freflection -o function_reflection function_reflection.cpp
