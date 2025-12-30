add_subdirectory(maps)

add_library(list INTERFACE list.h)
target_include_directories(list INTERFACE
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(list INTERFACE tempura_flags)
add_library(tempura::list ALIAS list)

if(TEMPURA_BUILD_TESTS)
  add_executable(list_test list_test.cpp)
  target_link_libraries(list_test unit list)
  add_test(NAME list_test COMMAND list_test)
  set_property(TEST list_test PROPERTY LABELS containers)
endif()
