cmake_minimum_required(VERSION 3.28)
project(tempura LANGUAGES CXX VERSION 0.1.0)

# ============================================================================
# Build Options
# ============================================================================

option(TEMPURA_BUILD_TESTS "Build tests" ON)
option(TEMPURA_BUILD_EXAMPLES "Build examples" ON)
set(TEMPURA_COMPILE_TIMEOUT "0" CACHE STRING "Per-file compilation timeout in seconds (0=off)")
option(TEMPURA_PROFILE_COMPILE "Add -ftime-report (GCC) or -ftime-trace (Clang)" OFF)

# ============================================================================
# External Dependencies
# ============================================================================
# Graphics packages only needed for examples — make optional so core lib builds
# without them (e.g., in the reflection devShell which lacks glfw/Vulkan).

if(TEMPURA_BUILD_EXAMPLES)
  find_package(glfw3 QUIET)
  find_package(OpenGL QUIET)
  find_package(Vulkan QUIET)
  if(NOT glfw3_FOUND OR NOT OpenGL_FOUND OR NOT Vulkan_FOUND)
    message(WARNING "Some graphics packages not found — disabling graphics examples")
  endif()
else()
  # Try to find them anyway for other targets, but don't fail
  find_package(glfw3 QUIET)
  find_package(OpenGL QUIET)
  find_package(Vulkan QUIET)
endif()

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

# Enable link time optimization (unless preset/cache overrides it to OFF)
# LTO helps template-heavy code by deferring codegen to link-time (10% faster overall)
if(NOT DEFINED CMAKE_INTERPROCEDURAL_OPTIMIZATION)
  set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
endif()

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
  -Wno-unused-variable
  -Wno-unused-parameter
  -Wno-sign-compare
  -ftemplate-depth=2048
  -fconstexpr-depth=2048
  # GCC-specific warning flags
  $<$<CXX_COMPILER_ID:GNU>:-Wno-unused-but-set-variable>
  $<$<CXX_COMPILER_ID:GNU>:-Wno-unused-local-typedefs>
  # Clang spells it without trailing 's'
  $<$<CXX_COMPILER_ID:Clang>:-Wno-unused-local-typedef>
)

# AVX-512 flags — guarded because the P2996 Clang fork may not support all of them
option(TEMPURA_ENABLE_AVX512 "Enable AVX-512 instruction set flags" ON)
if(TEMPURA_ENABLE_AVX512)
  target_compile_options(tempura_compiler_flags INTERFACE
    -mavx512f -mavx512dq -mavx512bw -mavx512vl
    -mavx512vbmi -mavx512vbmi2 -mavx512bitalg
    -mavx512vpopcntdq -mavx512vnni
    -mgfni -mvaes -mvpclmulqdq
    -maes -mrdseed -mbmi -mbmi2
  )
endif()

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
# Compile Timeout (per-file)
# ============================================================================
# Wraps the compiler with tools/timeout_compile.sh via CMAKE_CXX_COMPILER_LAUNCHER.
# Files exceeding the timeout are killed with a clear diagnostic.

if(NOT TEMPURA_COMPILE_TIMEOUT STREQUAL "0" AND TEMPURA_COMPILE_TIMEOUT)
  set(CMAKE_CXX_COMPILER_LAUNCHER
    "${CMAKE_SOURCE_DIR}/tools/timeout_compile.sh" "${TEMPURA_COMPILE_TIMEOUT}")
  message(STATUS "Compile timeout: ${TEMPURA_COMPILE_TIMEOUT}s per file")
endif()

# ============================================================================
# Compile Profiling
# ============================================================================
# Adds -ftime-report (GCC) or -ftime-trace (Clang) to see phase timing.

if(TEMPURA_PROFILE_COMPILE)
  if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    add_compile_options(-ftime-report)
    message(STATUS "Compile profiling: -ftime-report (GCC)")
  elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_compile_options(-ftime-trace)
    message(STATUS "Compile profiling: -ftime-trace (Clang)")
  else()
    message(WARNING "Compile profiling not supported for ${CMAKE_CXX_COMPILER_ID}")
  endif()
endif()

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

if(TEMPURA_BUILD_EXAMPLES)
  add_subdirectory(examples)
  add_subdirectory(tools)
  add_subdirectory(experiments)
endif()
