# Header-only DAG visualization library
add_library(dag INTERFACE)
target_include_directories(dag INTERFACE
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(dag INTERFACE tempura_flags)
add_library(tempura::dag ALIAS dag)

# Tests
if(TEMPURA_BUILD_TESTS)
  add_executable(dag_test dag_test.cc)
  target_link_libraries(dag_test unit dag)
  add_test(NAME dag_test COMMAND dag_test)
  set_property(TEST dag_test PROPERTY LABELS dag)
endif()
