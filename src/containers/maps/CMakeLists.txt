add_library(linear_probing INTERFACE linear_probing.h)
target_include_directories(linear_probing INTERFACE
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(linear_probing INTERFACE tempura_flags)
add_library(tempura::linear_probing ALIAS linear_probing)

if(TEMPURA_BUILD_TESTS)
  add_executable(linear_probing_test linear_probing_test.cpp)
  target_link_libraries(linear_probing_test unit linear_probing)
  add_test(NAME linear_probing_test COMMAND linear_probing_test)
  set_property(TEST linear_probing_test PROPERTY LABELS containers)
endif()
