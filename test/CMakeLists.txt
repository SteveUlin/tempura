add_executable(chebyshev_test chebyshev_test.cpp)
target_link_libraries(chebyshev_test unit chebyshev)
add_test(NAME chebyshev_test COMMAND chebyshev_test)
set_property(TEST chebyshev_test PROPERTY LABELS base)

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

file(GLOB_RECURSE MATRIX_TEST_SOURCES "matrix2/*.cpp")
foreach(TEST_SOURCE ${MATRIX_TEST_SOURCES})
   # Get the base name of the test file
  get_filename_component(TEST_NAME ${TEST_SOURCE} NAME_WE)

  add_executable(matrix2_${TEST_NAME} ${TEST_SOURCE})
  target_link_libraries(matrix2_${TEST_NAME} unit matrix2)
  add_test(NAME matrix2_${TEST_NAME} COMMAND matrix2_${TEST_NAME})
  set_property(TEST matrix2_${TEST_NAME} PROPERTY LABELS matrix2)
endforeach()

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

  add_executable(special_${TEST_NAME} ${TEST_SOURCE})
  target_link_libraries(special_${TEST_NAME} unit special)
  add_test(NAME special_${TEST_NAME} COMMAND special_${TEST_NAME})
  set_property(TEST special_${TEST_NAME} PROPERTY LABELS special)
endforeach()

#file(GLOB_RECURSE BAYES_TEST_SOURCES "bayes/*.cpp")
#foreach(TEST_SOURCE ${BAYES_TEST_SOURCES})
#   # Get the base name of the test file
#  get_filename_component(TEST_NAME ${TEST_SOURCE} NAME_WE)
#
#  add_executable(bayes_${TEST_NAME} ${TEST_SOURCE})
#  target_link_libraries(bayes_${TEST_NAME} unit bayes)
#  add_test(NAME bayes_${TEST_NAME} COMMAND bayes_${TEST_NAME})
#  set_property(TEST bayes_${TEST_NAME} PROPERTY LABELS bayes)
#endforeach()

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
  add_test(NAME utility_${TEST_NAME} COMMAND utilty_${TEST_NAME})
  set_property(TEST utility_${TEST_NAME} PROPERTY LABELS utility)
endforeach()

file(GLOB_RECURSE TEST_SOURCES "meta/*.cpp")
foreach(TEST_SOURCE ${TEST_SOURCES})
   # Get the base name of the test file
  get_filename_component(TEST_NAME ${TEST_SOURCE} NAME_WE)

  add_executable(meta_${TEST_NAME} ${TEST_SOURCE})
  target_link_libraries(meta_${TEST_NAME} unit meta)
  add_test(NAME meta_${TEST_NAME} COMMAND meta_${TEST_NAME})
  set_property(TEST meta_${TEST_NAME} PROPERTY LABELS meta)
endforeach()
