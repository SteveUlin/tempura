add_library(optimization INTERFACE)
target_include_directories(optimization INTERFACE
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(optimization INTERFACE tempura_flags)
add_library(tempura::optimization ALIAS optimization)
