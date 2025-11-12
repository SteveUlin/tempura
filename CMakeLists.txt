cmake_minimum_required(VERSION 3.28)
project(tempura LANGUAGES CXX VERSION 0.1.0)

# ============================================================================
# Build Options
# ============================================================================

option(TEMPURA_BUILD_TESTS "Build tests" ON)
option(TEMPURA_BUILD_EXAMPLES "Build examples" ON)

# ============================================================================
# External Dependencies
# ============================================================================

find_package(glfw3 REQUIRED)
find_package(OpenGL REQUIRED)
find_package(Vulkan REQUIRED)

# ============================================================================
# Compilation Database and IDE Support
# ============================================================================

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Get clangd to find stdlib
# https://discourse.nixos.org/t/get-clangd-to-find-standard-headers-in-nix-shell/11268/10
if(CMAKE_EXPORT_COMPILE_COMMANDS)
  set(CMAKE_CXX_STANDARD_INCLUDE_DIRECTORIES
      ${CMAKE_CXX_IMPLICIT_INCLUDE_DIRECTORIES})
endif()

# ============================================================================
# Global Build Settings
# ============================================================================

# Enable link time optimization
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)

set(CMAKE_CXX_STANDARD_REQUIRED TRUE)

# ============================================================================
# Compiler Flags Interface Targets
# ============================================================================

# Base compiler flags (debug and release)
add_library(tempura_compiler_flags INTERFACE)
target_compile_options(tempura_compiler_flags INTERFACE
  -Wall
  -Wextra
  -g
  -std=c++2c
  -Wno-unknown-pragmas
  -Wno-unused-but-set-variable
  -Wno-unused-variable
  -Wno-unused-local-typedefs
  -Wno-unused-parameter
  -Wno-sign-compare
  -mavx512f
  -mavx512dq
  -ftemplate-depth=2048
  -fconstexpr-depth=2048
)

# Release-specific optimization flags
add_library(tempura_release_flags INTERFACE)
target_compile_options(tempura_release_flags INTERFACE
  $<$<CONFIG:Release>:-ftree-vectorize>
  $<$<CONFIG:Release>:-march=native>
  $<$<CONFIG:Release>:-ffast-math>
  $<$<CONFIG:Release>:-funsafe-math-optimizations>
  $<$<CONFIG:Release>:-fno-math-errno>
  $<$<CONFIG:Release>:-O3>
  $<$<CONFIG:Release>:-funroll-loops>
  $<$<CONFIG:Release>:-mavx512f>
)

# Combined flags target
add_library(tempura_flags INTERFACE)
target_link_libraries(tempura_flags INTERFACE
  tempura_compiler_flags
  tempura_release_flags
)

# ============================================================================
# Testing
# ============================================================================

if(TEMPURA_BUILD_TESTS)
  enable_testing()
endif()

# ============================================================================
# Subdirectories
# ============================================================================

add_subdirectory(src)

if(TEMPURA_BUILD_TESTS)
  add_subdirectory(test)
endif()

if(TEMPURA_BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()
