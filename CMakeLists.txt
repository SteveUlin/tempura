cmake_minimum_required(VERSION 3.28)
project(tempura CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE INTERNAL "")
if(CMAKE_EXPORT_COMPILE_COMMANDS)
  set(CMAKE_CXX_STANDARD_INCLUDE_DIRECTORIES 
      ${CMAKE_CXX_IMPLICIT_INCLUDE_DIRECTORIES})
endif()

# Set the version of C++ for the project
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_FLAGS "-Wall -Wextra")
set(CMAKE_CXX_FLAGS_DEBUG "-g")
set(CMAKE_CXX_FLAGS_RELEASE "-Ofast -ftree-vectorize -march=native -ffast-math")

include_directories(src)
add_subdirectory(src)
add_subdirectory(examples)

