add_library(linear_probing INTERFACE linear_probing.h)
target_include_directories(linear_probing INTERFACE
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(linear_probing INTERFACE tempura_flags)
add_library(tempura::linear_probing ALIAS linear_probing)

add_library(quadratic_probing INTERFACE quadratic_probing.h)
target_include_directories(quadratic_probing INTERFACE
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(quadratic_probing INTERFACE tempura_flags)
add_library(tempura::quadratic_probing ALIAS quadratic_probing)

add_library(double_hashing INTERFACE double_hashing.h)
target_include_directories(double_hashing INTERFACE
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(double_hashing INTERFACE tempura_flags)
add_library(tempura::double_hashing ALIAS double_hashing)

if(TEMPURA_BUILD_TESTS)
  add_executable(linear_probing_test linear_probing_test.cpp)
  target_link_libraries(linear_probing_test unit linear_probing)
  add_test(NAME linear_probing_test COMMAND linear_probing_test)
  set_property(TEST linear_probing_test PROPERTY LABELS containers)

  add_executable(quadratic_probing_test quadratic_probing_test.cpp)
  target_link_libraries(quadratic_probing_test unit quadratic_probing)
  add_test(NAME quadratic_probing_test COMMAND quadratic_probing_test)
  set_property(TEST quadratic_probing_test PROPERTY LABELS containers)

  add_executable(probe_analysis probe_analysis.cpp)
  target_link_libraries(probe_analysis linear_probing)

  add_executable(double_hashing_test double_hashing_test.cpp)
  target_link_libraries(double_hashing_test unit double_hashing)
  add_test(NAME double_hashing_test COMMAND double_hashing_test)
  set_property(TEST double_hashing_test PROPERTY LABELS containers)

  add_executable(map_benchmark map_benchmark.cpp)
  target_link_libraries(map_benchmark linear_probing quadratic_probing double_hashing)
endif()
