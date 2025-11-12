add_library(quadature INTERFACE)
target_include_directories(quadature INTERFACE
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(quadature INTERFACE tempura_flags)
add_library(tempura::quadature ALIAS quadature)
