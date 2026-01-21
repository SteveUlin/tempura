# ============================================================================
# Symbolic4: AST-as-types with pluggable interpreters
# ============================================================================
#
# Dependencies:
#   - meta: Type metaprogramming for type lists and utilities
#   - tempura_flags: Compiler settings
#
# Key Design: Everything is an Atom<Id, Strategy>. Types ARE the representation.
# Recursion schemes (fold, para) provide pluggable tree traversal.
# ============================================================================

add_library(symbolic4 INTERFACE)
target_include_directories(symbolic4 INTERFACE
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(symbolic4 INTERFACE tempura::meta tempura_flags)
add_library(tempura::symbolic4 ALIAS symbolic4)

# ============================================================================
# Tests
# ============================================================================

if(NOT TEMPURA_BUILD_TESTS)
  return()
endif()

# compressed.h tests
add_executable(symbolic4_compressed_test compressed_test.cc)
target_link_libraries(symbolic4_compressed_test PRIVATE symbolic4 unit)
add_test(NAME symbolic4_compressed_test COMMAND symbolic4_compressed_test)
set_tests_properties(symbolic4_compressed_test PROPERTIES LABELS "symbolic4;compressed")

# core.h tests
add_executable(symbolic4_core_test core_test.cc)
target_link_libraries(symbolic4_core_test PRIVATE symbolic4 unit)
add_test(NAME symbolic4_core_test COMMAND symbolic4_core_test)
set_tests_properties(symbolic4_core_test PROPERTIES LABELS "symbolic4;core")

# operators.h tests
add_executable(symbolic4_operators_test operators_test.cc)
target_link_libraries(symbolic4_operators_test PRIVATE symbolic4 unit)
add_test(NAME symbolic4_operators_test COMMAND symbolic4_operators_test)
set_tests_properties(symbolic4_operators_test PROPERTIES LABELS "symbolic4;operators")

# scheme/fold.h tests
add_executable(symbolic4_fold_test scheme/fold_test.cc)
target_link_libraries(symbolic4_fold_test PRIVATE symbolic4 unit)
add_test(NAME symbolic4_fold_test COMMAND symbolic4_fold_test)
set_tests_properties(symbolic4_fold_test PROPERTIES LABELS "symbolic4;scheme;fold")

# scheme/para.h tests
add_executable(symbolic4_para_test scheme/para_test.cc)
target_link_libraries(symbolic4_para_test PRIVATE symbolic4 unit)
add_test(NAME symbolic4_para_test COMMAND symbolic4_para_test)
set_tests_properties(symbolic4_para_test PROPERTIES LABELS "symbolic4;scheme;para")

# interpreter/eval.h tests
add_executable(symbolic4_eval_test interpreter/eval_test.cc)
target_link_libraries(symbolic4_eval_test PRIVATE symbolic4 unit)
add_test(NAME symbolic4_eval_test COMMAND symbolic4_eval_test)
set_tests_properties(symbolic4_eval_test PROPERTIES LABELS "symbolic4;interpreter;eval")

# interpreter/diff.h tests
add_executable(symbolic4_diff_test interpreter/diff_test.cc)
target_link_libraries(symbolic4_diff_test PRIVATE symbolic4 unit)
add_test(NAME symbolic4_diff_test COMMAND symbolic4_diff_test)
set_tests_properties(symbolic4_diff_test PROPERTIES LABELS "symbolic4;interpreter;diff")

# interpreter/to_string.h tests
add_executable(symbolic4_to_string_test interpreter/to_string_test.cc)
target_link_libraries(symbolic4_to_string_test PRIVATE symbolic4 unit)
add_test(NAME symbolic4_to_string_test COMMAND symbolic4_to_string_test)
set_tests_properties(symbolic4_to_string_test PROPERTIES LABELS "symbolic4;interpreter;to_string")

# let.h tests
add_executable(symbolic4_let_test let_test.cc)
target_link_libraries(symbolic4_let_test PRIVATE symbolic4 unit)
add_test(NAME symbolic4_let_test COMMAND symbolic4_let_test)
set_tests_properties(symbolic4_let_test PROPERTIES LABELS "symbolic4;let")

# Demo executable to see toString output
add_executable(symbolic4_to_string_demo interpreter/to_string_demo.cc)
target_link_libraries(symbolic4_to_string_demo PRIVATE symbolic4)
