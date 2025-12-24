# Find liburing
find_path(LIBURING_INCLUDE_DIR liburing.h
  HINTS ENV C_INCLUDE_PATH
  PATH_SUFFIXES include
)
find_library(LIBURING_LIBRARY uring
  HINTS ENV LIBRARY_PATH
  PATH_SUFFIXES lib lib64
)

if(NOT LIBURING_INCLUDE_DIR OR NOT LIBURING_LIBRARY)
  message(STATUS "liburing not found, searching nix store...")
  # Fallback for nix environments
  file(GLOB LIBURING_NIX_DIRS "/nix/store/*liburing*-dev/include")
  if(LIBURING_NIX_DIRS)
    list(GET LIBURING_NIX_DIRS 0 LIBURING_INCLUDE_DIR)
  endif()
  file(GLOB LIBURING_NIX_LIBS "/nix/store/*liburing*/lib/liburing.so")
  if(LIBURING_NIX_LIBS)
    list(GET LIBURING_NIX_LIBS 0 LIBURING_LIBRARY)
  endif()
endif()

message(STATUS "liburing include: ${LIBURING_INCLUDE_DIR}")
message(STATUS "liburing library: ${LIBURING_LIBRARY}")

add_library(io_uring INTERFACE io_uring.h)
target_include_directories(io_uring INTERFACE
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
  $<INSTALL_INTERFACE:include>
  ${LIBURING_INCLUDE_DIR}
)
target_link_libraries(io_uring INTERFACE tempura_flags ${LIBURING_LIBRARY})
add_library(tempura::io_uring ALIAS io_uring)

add_library(io_uring_context INTERFACE io_uring_context.h)
target_include_directories(io_uring_context INTERFACE
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(io_uring_context INTERFACE io_uring tempura_flags)
add_library(tempura::io_uring_context ALIAS io_uring_context)

add_library(io_uring_sender INTERFACE io_uring_sender.h)
target_include_directories(io_uring_sender INTERFACE
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(io_uring_sender INTERFACE io_uring_context task tempura_flags)
add_library(tempura::io_uring_sender ALIAS io_uring_sender)

if(NOT TEMPURA_BUILD_TESTS)
  return()
endif()

add_executable(io_uring_test io_uring_test.cpp)
target_link_libraries(io_uring_test io_uring_context unit)

add_executable(io_uring_sender_test io_uring_sender_test.cpp)
target_link_libraries(io_uring_sender_test io_uring_sender task unit)
