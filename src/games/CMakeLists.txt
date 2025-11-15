add_library(sudoku INTERFACE sudoku.h)
target_include_directories(sudoku INTERFACE
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(sudoku INTERFACE tempura_flags)
add_library(tempura::sudoku ALIAS sudoku)

if(NOT TEMPURA_BUILD_TESTS)
  return()
endif()

# ============================================================================
# Tests
# ============================================================================

add_executable(sudoku_test sudoku_test.cpp)
target_link_libraries(sudoku_test sudoku unit)
add_test(NAME sudoku_test COMMAND sudoku_test)
set_tests_properties(sudoku_test PROPERTIES LABELS "games;sudoku")
