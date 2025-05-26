cmake_minimum_required(VERSION 3.28)
project(tempura LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
# Get clangd to find stdlib
# https://discourse.nixos.org/t/get-clangd-to-find-standard-headers-in-nix-shell/11268/10
if(CMAKE_EXPORT_COMPILE_COMMANDS)
  set(CMAKE_CXX_STANDARD_INCLUDE_DIRECTORIES 
      ${CMAKE_CXX_IMPLICIT_INCLUDE_DIRECTORIES})
endif()

# Enable link time optimization
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)

set(CMAKE_CXX_FLAGS "-Wall -Wextra -g -std=c++2c -Wno-unknown-pragmas -mavx512f")
set(CMAKE_CXX_FLAGS_RELEASE "-ftree-vectorize -march=native -ffast-math -O3 -funroll-loops -mavx512f")

set(CMAKE_CXX_STANDARD_REQUIRED TRUE)

enable_testing()

set(BASEPATH "${CMAKE_SOURCE_DIR}/src")
INCLUDE_DIRECTORIES("${BASEPATH}")
 
add_subdirectory(src)
add_subdirectory(test)
add_subdirectory(examples)
