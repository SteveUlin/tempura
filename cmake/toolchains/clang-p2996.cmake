# Toolchain file for Bloomberg's Clang P2996 (C++26 reflection)
#
# Usage: cmake --preset clang-p2996
#    or: cmake -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/clang-p2996.cmake ..
#
# Requires: nix develop .#reflection  (sets up env vars for libc++/CRT paths)

set(CMAKE_CXX_COMPILER clang++)
set(CMAKE_C_COMPILER clang)

# P2996 reflection + libc++ (which ships <meta> and <experimental/meta>)
# -nostdinc++ removes default C++ include paths so we use our P2996 libc++ exclusively
set(CMAKE_CXX_FLAGS_INIT "-stdlib=libc++ -nostdinc++ -freflection-latest -std=c++2c")
set(CMAKE_C_FLAGS_INIT "")

# P2996 libc++ headers (must come BEFORE C system headers so libc++'s wrapper
# headers like math.h/string.h can #include_next the underlying C versions)
if(DEFINED ENV{P2996_LIBCXX_INCLUDE})
  set(CMAKE_CXX_FLAGS_INIT "${CMAKE_CXX_FLAGS_INIT} -isystem $ENV{P2996_LIBCXX_INCLUDE}")
endif()

# NixOS C system headers — on NixOS, /usr/include doesn't exist so these must
# be added explicitly. Order matters: after libc++ so #include_next chains work.
if(DEFINED ENV{GLIBC_INCLUDE})
  set(CMAKE_CXX_FLAGS_INIT "${CMAKE_CXX_FLAGS_INIT} -isystem $ENV{GLIBC_INCLUDE}")
  set(CMAKE_C_FLAGS_INIT "${CMAKE_C_FLAGS_INIT} -isystem $ENV{GLIBC_INCLUDE}")
endif()
if(DEFINED ENV{LINUX_INCLUDE})
  set(CMAKE_CXX_FLAGS_INIT "${CMAKE_CXX_FLAGS_INIT} -isystem $ENV{LINUX_INCLUDE}")
  set(CMAKE_C_FLAGS_INIT "${CMAKE_C_FLAGS_INIT} -isystem $ENV{LINUX_INCLUDE}")
endif()

# P2996 libc++ libraries
if(DEFINED ENV{P2996_LIBCXX_LIB})
  set(CMAKE_EXE_LINKER_FLAGS_INIT "-L$ENV{P2996_LIBCXX_LIB} -Wl,-rpath,$ENV{P2996_LIBCXX_LIB}")
  set(CMAKE_SHARED_LINKER_FLAGS_INIT "-L$ENV{P2996_LIBCXX_LIB} -Wl,-rpath,$ENV{P2996_LIBCXX_LIB}")
endif()

# NixOS CRT/linker paths — these env vars are set by the reflection devShell
# Without them, clang can't find crt1.o/crti.o (glibc) or crtbeginS.o (gcc)
if(DEFINED ENV{CLANG_CRT_GLIBC})
  set(CMAKE_CXX_FLAGS_INIT "${CMAKE_CXX_FLAGS_INIT} -B$ENV{CLANG_CRT_GLIBC}")
  set(CMAKE_C_FLAGS_INIT "${CMAKE_C_FLAGS_INIT} -B$ENV{CLANG_CRT_GLIBC}")
endif()

if(DEFINED ENV{CLANG_CRT_GCC})
  # -B for CRT startup objects (crtbeginS.o), -L for libgcc.a
  set(CMAKE_CXX_FLAGS_INIT "${CMAKE_CXX_FLAGS_INIT} -B$ENV{CLANG_CRT_GCC}")
  set(CMAKE_C_FLAGS_INIT "${CMAKE_C_FLAGS_INIT} -B$ENV{CLANG_CRT_GCC}")
  set(CMAKE_EXE_LINKER_FLAGS_INIT "${CMAKE_EXE_LINKER_FLAGS_INIT} -L$ENV{CLANG_CRT_GCC}")
  set(CMAKE_SHARED_LINKER_FLAGS_INIT "${CMAKE_SHARED_LINKER_FLAGS_INIT} -L$ENV{CLANG_CRT_GCC}")
endif()

# libgcc_s.so lives in the GCC runtime lib output, separate from the compiler
if(DEFINED ENV{GCC_LIB_PATH})
  set(CMAKE_EXE_LINKER_FLAGS_INIT "${CMAKE_EXE_LINKER_FLAGS_INIT} -L$ENV{GCC_LIB_PATH}")
  set(CMAKE_SHARED_LINKER_FLAGS_INIT "${CMAKE_SHARED_LINKER_FLAGS_INIT} -L$ENV{GCC_LIB_PATH}")
endif()

if(DEFINED ENV{NIX_DYNAMIC_LINKER})
  set(CMAKE_EXE_LINKER_FLAGS_INIT "${CMAKE_EXE_LINKER_FLAGS_INIT} -Wl,--dynamic-linker=$ENV{NIX_DYNAMIC_LINKER}")
endif()

# Template/constexpr depth — needed for deep expression template types
set(CMAKE_CXX_FLAGS_INIT "${CMAKE_CXX_FLAGS_INIT} -ftemplate-depth=2048 -fconstexpr-depth=2048")
