file(GLOB_RECURSE TEST_SOURCES "matrix2/*.cpp")

foreach(TEST_SOURCE ${TEST_SOURCES})
   # Get the base name of the test file
  get_filename_component(TEST_NAME ${TEST_SOURCE} NAME_WE)

  add_executable(matrix2_${TEST_NAME} ${TEST_SOURCE})
  target_link_libraries(matrix2_${TEST_NAME} unit matrix2)
  add_test(NAME matrix2_${TEST_NAME} COMMAND matrix2_${TEST_NAME})
  set_property(TEST matrix2_${TEST_NAME} PROPERTY LABELS matrix2)
endforeach()
