# Tests in this directory use target-based includes via target_link_libraries()
# (Include paths come from the libraries they link to)

# Apply compiler flags to all tests
link_libraries(tempura_flags)

# add_executable(interpolate_test interpolate_test.cpp)
# target_link_libraries(interpolate_test unit interpolate)
# add_test(NAME interpolate_test COMMAND interpolate_test)
# set_property(TEST interpolate_test PROPERTY LABELS base)

add_executable(polynomial_test polynomial_test.cpp)
target_link_libraries(polynomial_test unit polynomial)
add_test(NAME polynomial_test COMMAND polynomial_test)
set_property(TEST polynomial_test PROPERTY LABELS base)

add_executable(json_test json_test.cpp)
target_link_libraries(json_test unit json)
add_test(NAME json_test COMMAND json_test)
set_property(TEST json_test PROPERTY LABELS base)

add_executable(sequence_test sequence_test.cpp)
target_link_libraries(sequence_test unit sequence)
add_test(NAME sequence_test COMMAND sequence_test)
set_property(TEST sequence_test PROPERTY LABELS base)

add_executable(root_finding_test root_finding_test.cpp)
target_link_libraries(root_finding_test unit root_finding)
add_test(NAME root_finding_test COMMAND root_finding_test)
set_property(TEST root_finding_test PROPERTY LABELS base)

# Component tests now live in src/*/ next to their implementation:
# - matrix2 tests → src/matrix2/
# - bayes tests → src/bayes/
# - quadature tests → src/quadature/
# - special tests → src/special/
# - optimization tests → src/optimization/
# - utility tests → src/utility/
# - meta tests → src/meta/test/

file(GLOB_RECURSE MATRIX3_TEST_SOURCES "matrix3/*.cpp")
foreach(TEST_SOURCE ${MATRIX3_TEST_SOURCES})
   # Get the base name of the test file
  get_filename_component(TEST_NAME ${TEST_SOURCE} NAME_WE)

  add_executable(matrix3_${TEST_NAME} ${TEST_SOURCE})
  target_link_libraries(matrix3_${TEST_NAME} unit matrix3)
  add_test(NAME matrix3_${TEST_NAME} COMMAND matrix3_${TEST_NAME})
  set_property(TEST matrix3_${TEST_NAME} PROPERTY LABELS matrix3)
endforeach()
