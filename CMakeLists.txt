cmake_minimum_required(VERSION 3.28)
project(tempura LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
# Get clang to find stdlib
# https://discourse.nixos.org/t/get-clangd-to-find-standard-headers-in-nix-shell/11268/10
if(CMAKE_EXPORT_COMPILE_COMMANDS)
  set(CMAKE_CXX_STANDARD_INCLUDE_DIRECTORIES 
      ${CMAKE_CXX_IMPLICIT_INCLUDE_DIRECTORIES})
endif()

set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
# set(CMAKE_CXX_FLAGS "-Wall -Wextra -fbracket-depth=1024")
set(CMAKE_CXX_FLAGS "-Wall -Wextra -std=c++2c -fconcepts-diagnostics-depth=4")
# set(CMAKE_CXX_FLAGS "-Wall -Wextra -std=c++2c")
set(CMAKE_CXX_FLAGS_RELEASE "-ftree-vectorize -march=native -ffast-math -O3 -funroll-loops")
# set(CMAKE_CXX_FLAGS_RELEASE "-ftree-vectorize -march=native -O2 -funroll-loops")

# find_package(CUDAToolkit)

# set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)

# set(CMAKE_CUDA_STANDARD 20)
# set(CMAKE_CUDA_STANDARD_REQUIRED ON)

enable_testing()

set(BASEPATH "${CMAKE_SOURCE_DIR}/src")
INCLUDE_DIRECTORIES("${BASEPATH}")
 
# set(CUDA_NVCC_FLAGS ${CUDA_NVCC_FLAGS} -arch=sm68)

add_subdirectory(src)
add_subdirectory(test)
add_subdirectory(examples)
