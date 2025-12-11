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

  add_executable(matrix3_inline_coordinate_list_test inline_coordinate_list_test.cpp)
  target_link_libraries(matrix3_inline_coordinate_list_test PRIVATE matrix3 unit)
  add_test(NAME matrix3_inline_coordinate_list_test COMMAND matrix3_inline_coordinate_list_test)
  set_tests_properties(matrix3_inline_coordinate_list_test PROPERTIES LABELS "matrix3")

  add_executable(matrix3_complex_test complex_test.cpp)
  target_link_libraries(matrix3_complex_test PRIVATE matrix3 unit)
  add_test(NAME matrix3_complex_test COMMAND matrix3_complex_test)
  set_tests_properties(matrix3_complex_test PROPERTIES LABELS "matrix3")

  add_executable(matrix3_banded_test banded_test.cpp)
  target_link_libraries(matrix3_banded_test PRIVATE matrix3 unit)
  add_test(NAME matrix3_banded_test COMMAND matrix3_banded_test)
  set_tests_properties(matrix3_banded_test PROPERTIES LABELS "matrix3")

  add_executable(matrix3_block_test block_test.cpp)
  target_link_libraries(matrix3_block_test PRIVATE matrix3 unit)
  add_test(NAME matrix3_block_test COMMAND matrix3_block_test)
  set_tests_properties(matrix3_block_test PROPERTIES LABELS "matrix3")

  add_executable(matrix3_permutation_test permutation_test.cpp)
  target_link_libraries(matrix3_permutation_test PRIVATE matrix3 unit)
  add_test(NAME matrix3_permutation_test COMMAND matrix3_permutation_test)
  set_tests_properties(matrix3_permutation_test PROPERTIES LABELS "matrix3")

  add_executable(matrix3_permuted_test permuted_test.cpp)
  target_link_libraries(matrix3_permuted_test PRIVATE matrix3 unit)
  add_test(NAME matrix3_permuted_test COMMAND matrix3_permuted_test)
  set_tests_properties(matrix3_permuted_test PROPERTIES LABELS "matrix3")

  add_executable(matrix3_addition_test addition_test.cpp)
  target_link_libraries(matrix3_addition_test PRIVATE matrix3 unit)
  add_test(NAME matrix3_addition_test COMMAND matrix3_addition_test)
  set_tests_properties(matrix3_addition_test PROPERTIES LABELS "matrix3")

  add_executable(matrix3_multiplication_test multiplication_test.cpp)
  target_link_libraries(matrix3_multiplication_test PRIVATE matrix3 unit)
  add_test(NAME matrix3_multiplication_test COMMAND matrix3_multiplication_test)
  set_tests_properties(matrix3_multiplication_test PROPERTIES LABELS "matrix3")
endif()
