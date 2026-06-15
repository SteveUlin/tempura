# Toolchain file for GCC trunk (C++26 with P2996 reflection)
#
# Usage: cmake --preset gcc-trunk
#    or: cmake -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/gcc-trunk.cmake ..
#
# Requires: nix develop .#trunk  (provides GCC trunk with -freflection)

set(CMAKE_CXX_COMPILER g++)
set(CMAKE_C_COMPILER gcc)

# GCC trunk ships <meta> in libstdc++ — no -nostdinc++ or custom include paths needed
set(CMAKE_CXX_FLAGS_INIT "-freflection -ftemplate-depth=2048 -fconstexpr-depth=2048")
