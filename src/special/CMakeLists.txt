add_library(special INTERFACE)
target_include_directories(special INTERFACE
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(special INTERFACE tempura_flags)
add_library(tempura::special ALIAS special)
