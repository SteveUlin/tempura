add_library(sin INTERFACE sin.h)
target_include_directories(sin INTERFACE
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(sin INTERFACE tempura_flags)
add_library(tempura::sin ALIAS sin)

add_library(cos INTERFACE cos.h)
target_include_directories(cos INTERFACE
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(cos INTERFACE tempura_flags)
add_library(tempura::cos ALIAS cos)

add_library(exp INTERFACE exp.h)
target_include_directories(exp INTERFACE
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(exp INTERFACE tempura_flags)
add_library(tempura::exp ALIAS exp)

if(NOT TEMPURA_BUILD_TESTS)
  return()
endif()

add_executable(sin_test sin_test.cpp)
target_link_libraries(sin_test sin unit)

add_executable(cos_test cos_test.cpp)
target_link_libraries(cos_test cos unit)

add_executable(exp_test exp_test.cpp)
target_link_libraries(exp_test exp unit)
