# ============================================================================
# Symbolic5: Info-native symbolic expressions (C++26 reflection)
# ============================================================================
#
# Dependencies:
#   - tempura_flags: Compiler settings (includes P2996 reflection support)
#
# Key Design: One type (Expr) holds two std::meta::info values — tree structure
# and annotations. All manipulation happens in the info domain at compile time.
# Types instantiated only at the splice boundary for zero-cost runtime eval.
# ============================================================================

add_library(symbolic5 INTERFACE)
target_include_directories(symbolic5 INTERFACE
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(symbolic5 INTERFACE tempura_flags)
add_library(tempura::symbolic5 ALIAS symbolic5)

# ============================================================================
# Tests
# ============================================================================

if(NOT TEMPURA_BUILD_TESTS)
  return()
endif()

# expr.h + eval.h tests
add_executable(symbolic5_expr_test expr_test.cc)
target_link_libraries(symbolic5_expr_test PRIVATE symbolic5 unit)
add_test(NAME symbolic5_expr_test COMMAND symbolic5_expr_test)
set_tests_properties(symbolic5_expr_test PROPERTIES LABELS "symbolic5")

# diff.h + simplify tests
add_executable(symbolic5_diff_test diff_test.cc)
target_link_libraries(symbolic5_diff_test PRIVATE symbolic5 unit)
add_test(NAME symbolic5_diff_test COMMAND symbolic5_diff_test)
set_tests_properties(symbolic5_diff_test PROPERTIES LABELS "symbolic5")

# to_string.h tests
add_executable(symbolic5_to_string_test to_string_test.cc)
target_link_libraries(symbolic5_to_string_test PRIVATE symbolic5 unit)
add_test(NAME symbolic5_to_string_test COMMAND symbolic5_to_string_test)
set_tests_properties(symbolic5_to_string_test PROPERTIES LABELS "symbolic5")

# simplify.h tests
add_executable(symbolic5_simplify_test simplify_test.cc)
target_link_libraries(symbolic5_simplify_test PRIVATE symbolic5 unit)
add_test(NAME symbolic5_simplify_test COMMAND symbolic5_simplify_test)
set_tests_properties(symbolic5_simplify_test PROPERTIES LABELS "symbolic5")
