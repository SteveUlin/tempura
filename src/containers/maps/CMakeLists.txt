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

add_library(separate_chaining INTERFACE separate_chaining.h)
target_include_directories(separate_chaining INTERFACE
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(separate_chaining INTERFACE tempura_flags)
add_library(tempura::separate_chaining ALIAS separate_chaining)

add_library(robin_hood INTERFACE robin_hood.h)
target_include_directories(robin_hood INTERFACE
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(robin_hood INTERFACE tempura_flags)
add_library(tempura::robin_hood ALIAS robin_hood)

add_library(anti_robin_hood INTERFACE anti_robin_hood.h)
target_include_directories(anti_robin_hood INTERFACE
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(anti_robin_hood INTERFACE tempura_flags)
add_library(tempura::anti_robin_hood ALIAS anti_robin_hood)

add_library(cuckoo INTERFACE cuckoo.h)
target_include_directories(cuckoo INTERFACE
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(cuckoo INTERFACE tempura_flags)
add_library(tempura::cuckoo ALIAS cuckoo)

add_library(hopscotch INTERFACE hopscotch.h)
target_include_directories(hopscotch INTERFACE
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(hopscotch INTERFACE tempura_flags)
add_library(tempura::hopscotch ALIAS hopscotch)

add_library(two_choice INTERFACE two_choice.h)
target_include_directories(two_choice INTERFACE
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(two_choice INTERFACE tempura_flags)
add_library(tempura::two_choice ALIAS two_choice)

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

  add_executable(separate_chaining_test separate_chaining_test.cpp)
  target_link_libraries(separate_chaining_test unit separate_chaining)
  add_test(NAME separate_chaining_test COMMAND separate_chaining_test)
  set_property(TEST separate_chaining_test PROPERTY LABELS containers)

  add_executable(robin_hood_test robin_hood_test.cpp)
  target_link_libraries(robin_hood_test unit robin_hood)
  add_test(NAME robin_hood_test COMMAND robin_hood_test)
  set_property(TEST robin_hood_test PROPERTY LABELS containers)

  add_executable(anti_robin_hood_test anti_robin_hood_test.cpp)
  target_link_libraries(anti_robin_hood_test unit anti_robin_hood)
  add_test(NAME anti_robin_hood_test COMMAND anti_robin_hood_test)
  set_property(TEST anti_robin_hood_test PROPERTY LABELS containers)

  add_executable(map_benchmark map_benchmark.cpp)
  target_link_libraries(map_benchmark linear_probing quadratic_probing double_hashing separate_chaining robin_hood anti_robin_hood cuckoo hopscotch two_choice)

  add_executable(cuckoo_test cuckoo_test.cpp)
  target_link_libraries(cuckoo_test unit cuckoo)
  add_test(NAME cuckoo_test COMMAND cuckoo_test)
  set_property(TEST cuckoo_test PROPERTY LABELS containers)

  add_executable(hopscotch_test hopscotch_test.cpp)
  target_link_libraries(hopscotch_test unit hopscotch)
  add_test(NAME hopscotch_test COMMAND hopscotch_test)
  set_property(TEST hopscotch_test PROPERTY LABELS containers)

  add_executable(two_choice_test two_choice_test.cpp)
  target_link_libraries(two_choice_test unit two_choice)
  add_test(NAME two_choice_test COMMAND two_choice_test)
  set_property(TEST two_choice_test PROPERTY LABELS containers)
endif()
