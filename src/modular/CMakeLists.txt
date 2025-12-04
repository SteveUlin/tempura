# Modular arithmetic library
#
# Provides:
# - ModInt<T, Mod>: Type-safe modular integers with compile-time modulus
# - Montgomery multiplication for efficient modular exponentiation
# - Number theory utilities (GCD, LCM, modular inverse, primality testing)

add_library(modular INTERFACE
  mod_int.h
  montgomery.h
  number_theory.h
)
target_include_directories(modular INTERFACE
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(modular INTERFACE tempura_flags meta bigint dynamic_bigint)
add_library(tempura::modular ALIAS modular)

if(TEMPURA_BUILD_TESTS)
  add_executable(modular_mod_int_test mod_int_test.cpp)
  target_link_libraries(modular_mod_int_test unit modular)
  add_test(NAME modular_mod_int_test COMMAND modular_mod_int_test)
  set_property(TEST modular_mod_int_test PROPERTY LABELS modular)

  add_executable(modular_number_theory_test number_theory_test.cpp)
  target_link_libraries(modular_number_theory_test unit modular)
  add_test(NAME modular_number_theory_test COMMAND modular_number_theory_test)
  set_property(TEST modular_number_theory_test PROPERTY LABELS modular)

  add_executable(modular_montgomery_test montgomery_test.cpp)
  target_link_libraries(modular_montgomery_test unit modular)
  add_test(NAME modular_montgomery_test COMMAND modular_montgomery_test)
  set_property(TEST modular_montgomery_test PROPERTY LABELS modular)
endif()
