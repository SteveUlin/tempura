add_library(utility INTERFACE)
target_include_directories(utility INTERFACE
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(utility INTERFACE tempura_flags)
add_library(tempura::utility ALIAS utility)

# ============================================================================
# Tests
# ============================================================================

if(NOT TEMPURA_BUILD_TESTS)
  return()
endif()

# Overloaded visitor pattern test
add_executable(utility_overloaded_test overloaded_test.cpp)
target_link_libraries(utility_overloaded_test PRIVATE utility unit)
add_test(NAME utility_overloaded_test COMMAND utility_overloaded_test)
set_tests_properties(utility_overloaded_test PROPERTIES LABELS "utility;patterns")
