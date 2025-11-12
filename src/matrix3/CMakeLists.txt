add_library(matrix3 INTERFACE)
target_include_directories(matrix3 INTERFACE
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(matrix3 INTERFACE tempura_flags)
add_library(tempura::matrix3 ALIAS matrix3)
