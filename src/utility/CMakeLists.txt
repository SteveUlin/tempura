add_library(utility INTERFACE)
target_include_directories(utility INTERFACE
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(utility INTERFACE tempura_flags)
add_library(tempura::utility ALIAS utility)
