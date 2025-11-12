add_library(matrix2 INTERFACE)
target_include_directories(matrix2 INTERFACE
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(matrix2 INTERFACE tempura_flags)
add_library(tempura::matrix2 ALIAS matrix2)

# ============================================================================
# Tests
# ============================================================================

if(NOT TEMPURA_BUILD_TESTS)
  return()
endif()

# ============================================================================
# Core Matrix Tests
# ============================================================================

# Basic matrix operations test
add_executable(matrix2_matrix_test matrix_test.cpp)
target_link_libraries(matrix2_matrix_test PRIVATE matrix2 unit)
add_test(NAME matrix2_matrix_test COMMAND matrix2_matrix_test)
set_tests_properties(matrix2_matrix_test PROPERTIES LABELS "matrix2;core")

# Matrix addition test
add_executable(matrix2_addition_test addition_test.cpp)
target_link_libraries(matrix2_addition_test PRIVATE matrix2 unit)
add_test(NAME matrix2_addition_test COMMAND matrix2_addition_test)
set_tests_properties(matrix2_addition_test PROPERTIES LABELS "matrix2;core")

# Matrix multiplication test
add_executable(matrix2_multiplication_test multiplication_test.cpp)
target_link_libraries(matrix2_multiplication_test PRIVATE matrix2 unit)
add_test(NAME matrix2_multiplication_test COMMAND matrix2_multiplication_test)
set_tests_properties(matrix2_multiplication_test PROPERTIES LABELS "matrix2;core")

# Matrix to_string test
add_executable(matrix2_to_string_test to_string_test.cpp)
target_link_libraries(matrix2_to_string_test PRIVATE matrix2 unit)
add_test(NAME matrix2_to_string_test COMMAND matrix2_to_string_test)
set_tests_properties(matrix2_to_string_test PROPERTIES LABELS "matrix2;core")

# ============================================================================
# Storage Tests
# ============================================================================

# Dense storage test
add_executable(matrix2_dense_test storage/dense_test.cpp)
target_link_libraries(matrix2_dense_test PRIVATE matrix2 unit)
add_test(NAME matrix2_dense_test COMMAND matrix2_dense_test)
set_tests_properties(matrix2_dense_test PROPERTIES LABELS "matrix2;storage")

# Inline dense storage test
add_executable(matrix2_inline_dense_test storage/inline_dense_test.cpp)
target_link_libraries(matrix2_inline_dense_test PRIVATE matrix2 unit)
add_test(NAME matrix2_inline_dense_test COMMAND matrix2_inline_dense_test)
set_tests_properties(matrix2_inline_dense_test PROPERTIES LABELS "matrix2;storage")

# Banded storage test
add_executable(matrix2_banded_test storage/banded_test.cpp)
target_link_libraries(matrix2_banded_test PRIVATE matrix2 unit)
add_test(NAME matrix2_banded_test COMMAND matrix2_banded_test)
set_tests_properties(matrix2_banded_test PROPERTIES LABELS "matrix2;storage")

# Block storage test
add_executable(matrix2_block_test storage/block_test.cpp)
target_link_libraries(matrix2_block_test PRIVATE matrix2 unit)
add_test(NAME matrix2_block_test COMMAND matrix2_block_test)
set_tests_properties(matrix2_block_test PROPERTIES LABELS "matrix2;storage")

# Complex storage test
add_executable(matrix2_complex_test storage/complex_test.cpp)
target_link_libraries(matrix2_complex_test PRIVATE matrix2 unit)
add_test(NAME matrix2_complex_test COMMAND matrix2_complex_test)
set_tests_properties(matrix2_complex_test PROPERTIES LABELS "matrix2;storage")

# Identity storage test
add_executable(matrix2_identity_test storage/identity_test.cpp)
target_link_libraries(matrix2_identity_test PRIVATE matrix2 unit)
add_test(NAME matrix2_identity_test COMMAND matrix2_identity_test)
set_tests_properties(matrix2_identity_test PROPERTIES LABELS "matrix2;storage")

# Inline coordinate list storage test
add_executable(matrix2_inline_coordinate_list_test storage/inline_coordinate_list_test.cpp)
target_link_libraries(matrix2_inline_coordinate_list_test PRIVATE matrix2 unit)
add_test(NAME matrix2_inline_coordinate_list_test COMMAND matrix2_inline_coordinate_list_test)
set_tests_properties(matrix2_inline_coordinate_list_test PROPERTIES LABELS "matrix2;storage")

# Permutation storage test
add_executable(matrix2_permutation_test storage/permutation_test.cpp)
target_link_libraries(matrix2_permutation_test PRIVATE matrix2 unit)
add_test(NAME matrix2_permutation_test COMMAND matrix2_permutation_test)
set_tests_properties(matrix2_permutation_test PROPERTIES LABELS "matrix2;storage")

# Permuted storage test
add_executable(matrix2_permuted_test storage/permuted_test.cpp)
target_link_libraries(matrix2_permuted_test PRIVATE matrix2 unit)
add_test(NAME matrix2_permuted_test COMMAND matrix2_permuted_test)
set_tests_properties(matrix2_permuted_test PROPERTIES LABELS "matrix2;storage")

# ============================================================================
# Algorithm Tests
# ============================================================================

# LU decomposition test
add_executable(matrix2_lu_decomposition_test algorithms/lu_decomposition_test.cpp)
target_link_libraries(matrix2_lu_decomposition_test PRIVATE matrix2 unit)
add_test(NAME matrix2_lu_decomposition_test COMMAND matrix2_lu_decomposition_test)
set_tests_properties(matrix2_lu_decomposition_test PROPERTIES LABELS "matrix2;algorithms")

# Gauss-Jordan elimination test
add_executable(matrix2_gauss_jordan_test algorithms/gauss_jordan_test.cpp)
target_link_libraries(matrix2_gauss_jordan_test PRIVATE matrix2 unit)
add_test(NAME matrix2_gauss_jordan_test COMMAND matrix2_gauss_jordan_test)
set_tests_properties(matrix2_gauss_jordan_test PROPERTIES LABELS "matrix2;algorithms")
