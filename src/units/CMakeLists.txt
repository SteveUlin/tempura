add_library(magnitude INTERFACE magnitude.h)
target_include_directories(magnitude INTERFACE
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(magnitude INTERFACE tempura_flags meta ratio)
add_library(tempura::magnitude ALIAS magnitude)

add_library(dimension INTERFACE dimension.h)
target_include_directories(dimension INTERFACE
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(dimension INTERFACE tempura_flags meta)
add_library(tempura::dimension ALIAS dimension)

add_library(quantity_spec INTERFACE quantity_spec.h)
target_include_directories(quantity_spec INTERFACE
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(quantity_spec INTERFACE dimension)
add_library(tempura::quantity_spec ALIAS quantity_spec)

add_library(units_unit INTERFACE unit_type.h)
target_include_directories(units_unit INTERFACE
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(units_unit INTERFACE quantity_spec)
add_library(tempura::units_unit ALIAS units_unit)

add_library(derived_unit INTERFACE derived_unit.h)
target_include_directories(derived_unit INTERFACE
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(derived_unit INTERFACE units_unit)
add_library(tempura::derived_unit ALIAS derived_unit)

add_library(reference INTERFACE reference.h)
target_include_directories(reference INTERFACE
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(reference INTERFACE units_unit)
add_library(tempura::reference ALIAS reference)

add_library(quantity INTERFACE quantity.h)
target_include_directories(quantity INTERFACE
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(quantity INTERFACE reference)
add_library(tempura::quantity ALIAS quantity)

add_library(literals INTERFACE literals.h)
target_include_directories(literals INTERFACE
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(literals INTERFACE quantity)
add_library(tempura::literals ALIAS literals)

add_library(units INTERFACE units.h)
target_include_directories(units INTERFACE
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(units INTERFACE literals)
add_library(tempura::units ALIAS units)

if(NOT TEMPURA_BUILD_TESTS)
  return()
endif()

add_executable(units_dimension_test dimension_test.cpp)
target_link_libraries(units_dimension_test dimension unit)
add_test(NAME units_dimension_test COMMAND units_dimension_test)
set_property(TEST units_dimension_test PROPERTY LABELS units)

add_executable(units_quantity_spec_test quantity_spec_test.cpp)
target_link_libraries(units_quantity_spec_test quantity_spec unit)
add_test(NAME units_quantity_spec_test COMMAND units_quantity_spec_test)
set_property(TEST units_quantity_spec_test PROPERTY LABELS units)

add_executable(units_unit_test unit_test.cpp)
target_link_libraries(units_unit_test units_unit unit)
add_test(NAME units_unit_test COMMAND units_unit_test)
set_property(TEST units_unit_test PROPERTY LABELS units)

add_executable(units_reference_test reference_test.cpp)
target_link_libraries(units_reference_test reference unit)
add_test(NAME units_reference_test COMMAND units_reference_test)
set_property(TEST units_reference_test PROPERTY LABELS units)

add_executable(units_quantity_test quantity_test.cpp)
target_link_libraries(units_quantity_test quantity unit)
add_test(NAME units_quantity_test COMMAND units_quantity_test)
set_property(TEST units_quantity_test PROPERTY LABELS units)

add_executable(units_literals_test literals_test.cpp)
target_link_libraries(units_literals_test units unit)
add_test(NAME units_literals_test COMMAND units_literals_test)
set_property(TEST units_literals_test PROPERTY LABELS units)

add_executable(units_derived_unit_test derived_unit_test.cpp)
target_link_libraries(units_derived_unit_test derived_unit unit)
add_test(NAME units_derived_unit_test COMMAND units_derived_unit_test)
set_property(TEST units_derived_unit_test PROPERTY LABELS units)

add_executable(units_magnitude_test magnitude_test.cpp)
target_link_libraries(units_magnitude_test magnitude unit)
add_test(NAME units_magnitude_test COMMAND units_magnitude_test)
set_property(TEST units_magnitude_test PROPERTY LABELS units)

add_executable(units_us_test us_test.cpp)
target_link_libraries(units_us_test units unit)
add_test(NAME units_us_test COMMAND units_us_test)
set_property(TEST units_us_test PROPERTY LABELS units)

add_executable(units_math_test math_test.cpp)
target_link_libraries(units_math_test units unit)
add_test(NAME units_math_test COMMAND units_math_test)
set_property(TEST units_math_test PROPERTY LABELS units)

add_executable(units_examples examples.cpp)
target_link_libraries(units_examples units derived_unit)
