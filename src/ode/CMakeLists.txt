add_library(ode INTERFACE)
target_sources(ode INTERFACE
  concepts.h
  euler.h
  runge_kutta.h
  integrator.h
  integrate.h
)
target_include_directories(ode INTERFACE
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(ode INTERFACE tempura_flags)
add_library(tempura::ode ALIAS ode)

if(TEMPURA_BUILD_TESTS)
  add_executable(ode_test ode_test.cpp)
  target_link_libraries(ode_test unit ode)
  add_test(NAME ode_test COMMAND ode_test)
  set_property(TEST ode_test PROPERTY LABELS ode)
endif()
