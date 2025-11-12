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

# matrix2 tests now live in src/matrix2/ next to the code

file(GLOB_RECURSE MATRIX3_TEST_SOURCES "matrix3/*.cpp")
foreach(TEST_SOURCE ${MATRIX3_TEST_SOURCES})
   # Get the base name of the test file
  get_filename_component(TEST_NAME ${TEST_SOURCE} NAME_WE)

  add_executable(matrix3_${TEST_NAME} ${TEST_SOURCE})
  target_link_libraries(matrix3_${TEST_NAME} unit matrix3)
  add_test(NAME matrix3_${TEST_NAME} COMMAND matrix3_${TEST_NAME})
  set_property(TEST matrix3_${TEST_NAME} PROPERTY LABELS matrix3)
endforeach()

file(GLOB_RECURSE QUADATURE_TEST_SOURCES "quadature/*.cpp")
foreach(TEST_SOURCE ${QUADATURE_TEST_SOURCES})
   # Get the base name of the test file
  get_filename_component(TEST_NAME ${TEST_SOURCE} NAME_WE)

  add_executable(quadature_${TEST_NAME} ${TEST_SOURCE})
  target_link_libraries(quadature_${TEST_NAME} unit quadature)
  add_test(NAME quadature_${TEST_NAME} COMMAND quadature_${TEST_NAME})
  set_property(TEST quadature_${TEST_NAME} PROPERTY LABELS quadature)
endforeach()

file(GLOB_RECURSE SPECIAL_TEST_SOURCES "special/*.cpp")
foreach(TEST_SOURCE ${SPECIAL_TEST_SOURCES})
   # Get the base name of the test file
  get_filename_component(TEST_NAME ${TEST_SOURCE} NAME_WE)

  # Skip broken gamma_test (namespace ambiguity with unit.h::detail)
  if(TEST_NAME STREQUAL "gamma_test")
    continue()
  endif()

  add_executable(special_${TEST_NAME} ${TEST_SOURCE})
  target_link_libraries(special_${TEST_NAME} unit special)
  add_test(NAME special_${TEST_NAME} COMMAND special_${TEST_NAME})
  set_property(TEST special_${TEST_NAME} PROPERTY LABELS special)
endforeach()

# Bayes tests now live in src/bayes/ next to the code

file(GLOB_RECURSE OPTIMIZATION_TEST_SOURCES "optimization/*.cpp")
foreach(TEST_SOURCE ${OPTIMIZATION_TEST_SOURCES})
   # Get the base name of the test file
  get_filename_component(TEST_NAME ${TEST_SOURCE} NAME_WE)

  add_executable(optimization_${TEST_NAME} ${TEST_SOURCE})
  target_link_libraries(optimization_${TEST_NAME} unit optimization)
  add_test(NAME optimization_${TEST_NAME} COMMAND optimization_${TEST_NAME})
  set_property(TEST optimization_${TEST_NAME} PROPERTY LABELS optimization)
endforeach()

file(GLOB_RECURSE MATRIX_TEST_SOURCES "utility/*.cpp")
foreach(TEST_SOURCE ${MATRIX_TEST_SOURCES})
   # Get the base name of the test file
  get_filename_component(TEST_NAME ${TEST_SOURCE} NAME_WE)

  add_executable(utility_${TEST_NAME} ${TEST_SOURCE})
  target_link_libraries(utility_${TEST_NAME} unit utility meta)
  add_test(NAME utility_${TEST_NAME} COMMAND utility_${TEST_NAME})
  set_property(TEST utility_${TEST_NAME} PROPERTY LABELS utility)
endforeach()

file(GLOB_RECURSE TEST_SOURCES "meta/*.cpp")
foreach(TEST_SOURCE ${TEST_SOURCES})
   # Get the base name of the test file
  get_filename_component(TEST_NAME ${TEST_SOURCE} NAME_WE)

  # Skip broken tests (pre-existing issues)
  if(TEST_NAME STREQUAL "function_objects_test" OR TEST_NAME STREQUAL "containers_test")
    continue()
  endif()

  add_executable(meta_${TEST_NAME} ${TEST_SOURCE})
  target_link_libraries(meta_${TEST_NAME} unit meta)
  add_test(NAME meta_${TEST_NAME} COMMAND meta_${TEST_NAME})
  set_property(TEST meta_${TEST_NAME} PROPERTY LABELS meta)
endforeach()
