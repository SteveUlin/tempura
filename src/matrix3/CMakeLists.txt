add_library(matrix3 INTERFACE)
target_include_directories(matrix3 INTERFACE
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(matrix3 INTERFACE tempura_flags)
add_library(tempura::matrix3 ALIAS matrix3)

# ============================================================================
# Tests
# ============================================================================

if(TEMPURA_BUILD_TESTS)
  add_executable(matrix3_base_test base_test.cpp)
  target_link_libraries(matrix3_base_test PRIVATE matrix3 unit)
  add_test(NAME matrix3_base_test COMMAND matrix3_base_test)
  set_tests_properties(matrix3_base_test PROPERTIES LABELS "matrix3")
endif()
