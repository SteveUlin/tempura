add_executable(interpolate_test interpolate_test.cpp)
target_link_libraries(interpolate_test unit interpolate)
add_test(NAME interpolate_test COMMAND interpolate_test)
set_property(TEST interpolate_test PROPERTY LABELS base)

add_executable(polynomial_test polynomial_test.cpp)
target_link_libraries(polynomial_test unit polynomial)
add_test(NAME polynomial_test COMMAND polynomial_test)
set_property(TEST polynomial_test PROPERTY LABELS base)

add_executable(json_test json_test.cpp)
target_link_libraries(json_test unit json)
add_test(NAME json_test COMMAND json_test)
set_property(TEST json_test PROPERTY LABELS base)

file(GLOB_RECURSE MATRIX_TEST_SOURCES "matrix2/*.cpp")
foreach(TEST_SOURCE ${MATRIX_TEST_SOURCES})
   # Get the base name of the test file
  get_filename_component(TEST_NAME ${TEST_SOURCE} NAME_WE)

  add_executable(matrix2_${TEST_NAME} ${TEST_SOURCE})
  target_link_libraries(matrix2_${TEST_NAME} unit matrix2)
  add_test(NAME matrix2_${TEST_NAME} COMMAND matrix2_${TEST_NAME})
  set_property(TEST matrix2_${TEST_NAME} PROPERTY LABELS matrix2)
endforeach()

file(GLOB_RECURSE QUADATURE_TEST_SOURCES "quadature/*.cpp")
foreach(TEST_SOURCE ${QUADATURE_TEST_SOURCES})
   # Get the base name of the test file
  get_filename_component(TEST_NAME ${TEST_SOURCE} NAME_WE)

  add_executable(quadature_${TEST_NAME} ${TEST_SOURCE})
  target_link_libraries(quadature_${TEST_NAME} unit matrix2)
  add_test(NAME quadature_${TEST_NAME} COMMAND quadature_${TEST_NAME})
  set_property(TEST quadature_${TEST_NAME} PROPERTY LABELS quadature)
endforeach()
