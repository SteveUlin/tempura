add_library(matrix2 INTERFACE)
target_include_directories(matrix2 INTERFACE
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(matrix2 INTERFACE tempura_flags)
add_library(tempura::matrix2 ALIAS matrix2)
