add_library(special INTERFACE)
target_include_directories(special INTERFACE
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(special INTERFACE tempura_flags)
add_library(tempura::special ALIAS special)

# ============================================================================
# Tests
# ============================================================================

if(NOT TEMPURA_BUILD_TESTS)
  return()
endif()

# Gamma function test (currently broken - namespace ambiguity with unit.h::detail)
# TODO: Fix namespace collision and re-enable
# add_executable(special_gamma_test gamma_test.cpp)
# target_link_libraries(special_gamma_test PRIVATE special unit)
# add_test(NAME special_gamma_test COMMAND special_gamma_test)
# set_tests_properties(special_gamma_test PROPERTIES LABELS "special;functions")
