# ============================================================================
# Hash Functions Library
# ============================================================================
#
# Non-cryptographic hash functions for hash tables and general hashing.
#
# Provides:
#   - FNV-1a: Simple, portable, good baseline
#   - MurmurHash3: Battle-tested, widely used
#   - wyhash: Fastest modern hash function
#
# All implementations are constexpr-capable and header-only.
#
# Dependencies: None (standalone library)
#
# ============================================================================

add_library(hash INTERFACE)
target_include_directories(hash INTERFACE
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
    $<INSTALL_INTERFACE:include>
)
target_link_libraries(hash INTERFACE tempura_flags)
add_library(tempura::hash ALIAS hash)

# ============================================================================
# Tests
# ============================================================================

if(NOT TEMPURA_BUILD_TESTS)
  return()
endif()

add_executable(hash_test hash_test.cpp)
target_link_libraries(hash_test PRIVATE hash unit)
add_test(NAME hash_test COMMAND hash_test)
set_tests_properties(hash_test PROPERTIES LABELS "hash")

add_executable(perfect_hash_test perfect_hash_test.cpp)
target_link_libraries(perfect_hash_test PRIVATE hash unit)
add_test(NAME perfect_hash_test COMMAND perfect_hash_test)
set_tests_properties(perfect_hash_test PROPERTIES LABELS "hash")

add_executable(hardware_hash_test hardware_hash_test.cpp)
target_link_libraries(hardware_hash_test PRIVATE hash unit)
add_test(NAME hardware_hash_test COMMAND hardware_hash_test)
set_tests_properties(hardware_hash_test PROPERTIES LABELS "hash")

# Benchmark (not a test, run manually)
add_executable(hash_benchmark hash_benchmark.cpp)
target_link_libraries(hash_benchmark PRIVATE hash)
