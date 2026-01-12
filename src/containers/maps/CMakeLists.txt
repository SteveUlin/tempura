add_library(linear_probing INTERFACE linear_probing.h)
target_include_directories(linear_probing INTERFACE
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(linear_probing INTERFACE tempura_flags)
add_library(tempura::linear_probing ALIAS linear_probing)

add_library(linear_probing_reuse INTERFACE linear_probing_reuse.h)
target_include_directories(linear_probing_reuse INTERFACE
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(linear_probing_reuse INTERFACE tempura_flags)
add_library(tempura::linear_probing_reuse ALIAS linear_probing_reuse)

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

add_library(coalesced INTERFACE coalesced.h)
target_include_directories(coalesced INTERFACE
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(coalesced INTERFACE tempura_flags)
add_library(tempura::coalesced ALIAS coalesced)

add_library(extendible INTERFACE extendible.h)
target_include_directories(extendible INTERFACE
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(extendible INTERFACE tempura_flags)
add_library(tempura::extendible ALIAS extendible)

add_library(swiss_table INTERFACE swiss_table.h)
target_include_directories(swiss_table INTERFACE
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(swiss_table INTERFACE tempura_flags)
add_library(tempura::swiss_table ALIAS swiss_table)

add_library(swiss_table_32 INTERFACE swiss_table_32.h)
target_include_directories(swiss_table_32 INTERFACE
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(swiss_table_32 INTERFACE tempura_flags)
add_library(tempura::swiss_table_32 ALIAS swiss_table_32)

add_library(swiss_table_64 INTERFACE swiss_table_64.h)
target_include_directories(swiss_table_64 INTERFACE
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(swiss_table_64 INTERFACE tempura_flags)
add_library(tempura::swiss_table_64 ALIAS swiss_table_64)

add_library(swiss_table_bloom INTERFACE swiss_table_bloom.h)
target_include_directories(swiss_table_bloom INTERFACE
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(swiss_table_bloom INTERFACE tempura_flags)
add_library(tempura::swiss_table_bloom ALIAS swiss_table_bloom)

add_library(f14 INTERFACE f14.h)
target_include_directories(f14 INTERFACE
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(f14 INTERFACE tempura_flags)
add_library(tempura::f14 ALIAS f14)

add_library(f14_prefetch INTERFACE f14_prefetch.h)
target_include_directories(f14_prefetch INTERFACE
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(f14_prefetch INTERFACE tempura_flags)
add_library(tempura::f14_prefetch ALIAS f14_prefetch)

add_library(swiss_table_reuse INTERFACE swiss_table_reuse.h)
target_include_directories(swiss_table_reuse INTERFACE
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(swiss_table_reuse INTERFACE tempura_flags)
add_library(tempura::swiss_table_reuse ALIAS swiss_table_reuse)

add_library(swiss_table_prefetch INTERFACE swiss_table_prefetch.h)
target_include_directories(swiss_table_prefetch INTERFACE
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(swiss_table_prefetch INTERFACE tempura_flags)
add_library(tempura::swiss_table_prefetch ALIAS swiss_table_prefetch)

add_library(graveyard INTERFACE graveyard.h)
target_include_directories(graveyard INTERFACE
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(graveyard INTERFACE tempura_flags)
add_library(tempura::graveyard ALIAS graveyard)

add_library(swiss_table_graveyard INTERFACE swiss_table_graveyard.h)
target_include_directories(swiss_table_graveyard INTERFACE
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(swiss_table_graveyard INTERFACE tempura_flags)
add_library(tempura::swiss_table_graveyard ALIAS swiss_table_graveyard)

add_library(swiss_table_32_prefetch INTERFACE swiss_table_32_prefetch.h)
target_include_directories(swiss_table_32_prefetch INTERFACE
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(swiss_table_32_prefetch INTERFACE tempura_flags)
add_library(tempura::swiss_table_32_prefetch ALIAS swiss_table_32_prefetch)

add_library(swiss_table_32_graveyard INTERFACE swiss_table_32_graveyard.h)
target_include_directories(swiss_table_32_graveyard INTERFACE
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(swiss_table_32_graveyard INTERFACE tempura_flags)
add_library(tempura::swiss_table_32_graveyard ALIAS swiss_table_32_graveyard)

add_library(swiss_table_32_prefetch_graveyard INTERFACE swiss_table_32_prefetch_graveyard.h)
target_include_directories(swiss_table_32_prefetch_graveyard INTERFACE
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(swiss_table_32_prefetch_graveyard INTERFACE tempura_flags)
add_library(tempura::swiss_table_32_prefetch_graveyard ALIAS swiss_table_32_prefetch_graveyard)

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
  # NOTE: coalesced/extendible excluded - O(n) operations make them too slow
  target_link_libraries(map_benchmark
    linear_probing linear_probing_reuse quadratic_probing double_hashing
    separate_chaining robin_hood anti_robin_hood cuckoo hopscotch two_choice
    swiss_table swiss_table_32 swiss_table_64
    swiss_table_reuse swiss_table_prefetch swiss_table_graveyard swiss_table_bloom
    swiss_table_32_prefetch swiss_table_32_graveyard swiss_table_32_prefetch_graveyard
    f14 f14_prefetch graveyard)

  add_executable(quick_benchmark quick_benchmark.cpp)
  # NOTE: coalesced/extendible excluded - O(n) operations make them too slow
  target_link_libraries(quick_benchmark
    linear_probing linear_probing_reuse quadratic_probing double_hashing
    separate_chaining robin_hood anti_robin_hood cuckoo hopscotch two_choice
    swiss_table swiss_table_32 swiss_table_64
    swiss_table_reuse swiss_table_prefetch swiss_table_graveyard
    swiss_table_32_prefetch swiss_table_32_graveyard swiss_table_32_prefetch_graveyard
    swiss_table_bloom f14 f14_prefetch graveyard)

  add_executable(comprehensive_benchmark comprehensive_benchmark.cpp)
  target_link_libraries(comprehensive_benchmark
    linear_probing linear_probing_reuse quadratic_probing double_hashing
    separate_chaining robin_hood anti_robin_hood cuckoo hopscotch two_choice
    swiss_table swiss_table_32 swiss_table_64
    swiss_table_reuse swiss_table_prefetch swiss_table_graveyard
    swiss_table_32_prefetch swiss_table_32_graveyard swiss_table_32_prefetch_graveyard
    swiss_table_bloom f14 f14_prefetch graveyard)

  add_executable(extendible_test extendible_test.cpp)
  target_link_libraries(extendible_test unit extendible)
  add_test(NAME extendible_test COMMAND extendible_test)
  set_property(TEST extendible_test PROPERTY LABELS containers)

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

  add_executable(coalesced_test coalesced_test.cpp)
  target_link_libraries(coalesced_test unit coalesced)
  add_test(NAME coalesced_test COMMAND coalesced_test)
  set_property(TEST coalesced_test PROPERTY LABELS containers)

  add_executable(swiss_table_test swiss_table_test.cpp)
  target_link_libraries(swiss_table_test unit swiss_table)
  add_test(NAME swiss_table_test COMMAND swiss_table_test)
  set_property(TEST swiss_table_test PROPERTY LABELS containers)

  add_executable(swiss_table_32_test swiss_table_32_test.cpp)
  target_link_libraries(swiss_table_32_test unit swiss_table_32)
  add_test(NAME swiss_table_32_test COMMAND swiss_table_32_test)
  set_property(TEST swiss_table_32_test PROPERTY LABELS containers)

  add_executable(swiss_table_64_test swiss_table_64_test.cpp)
  target_link_libraries(swiss_table_64_test unit swiss_table_64)
  add_test(NAME swiss_table_64_test COMMAND swiss_table_64_test)
  set_property(TEST swiss_table_64_test PROPERTY LABELS containers)

  add_executable(swiss_table_bloom_test swiss_table_bloom_test.cpp)
  target_link_libraries(swiss_table_bloom_test unit swiss_table_bloom)
  add_test(NAME swiss_table_bloom_test COMMAND swiss_table_bloom_test)
  set_property(TEST swiss_table_bloom_test PROPERTY LABELS containers)

  add_executable(swiss_table_reuse_test swiss_table_reuse_test.cpp)
  target_link_libraries(swiss_table_reuse_test unit swiss_table_reuse)
  add_test(NAME swiss_table_reuse_test COMMAND swiss_table_reuse_test)
  set_property(TEST swiss_table_reuse_test PROPERTY LABELS containers)

  add_executable(swiss_table_prefetch_test swiss_table_prefetch_test.cpp)
  target_link_libraries(swiss_table_prefetch_test unit swiss_table_prefetch)
  add_test(NAME swiss_table_prefetch_test COMMAND swiss_table_prefetch_test)
  set_property(TEST swiss_table_prefetch_test PROPERTY LABELS containers)

  add_executable(graveyard_test graveyard_test.cpp)
  target_link_libraries(graveyard_test unit graveyard)
  add_test(NAME graveyard_test COMMAND graveyard_test)
  set_property(TEST graveyard_test PROPERTY LABELS containers)

  add_executable(swiss_table_graveyard_test swiss_table_graveyard_test.cpp)
  target_link_libraries(swiss_table_graveyard_test unit swiss_table_graveyard)
  add_test(NAME swiss_table_graveyard_test COMMAND swiss_table_graveyard_test)
  set_property(TEST swiss_table_graveyard_test PROPERTY LABELS containers)

  add_executable(f14_test f14_test.cpp)
  target_link_libraries(f14_test unit f14)
  add_test(NAME f14_test COMMAND f14_test)
  set_property(TEST f14_test PROPERTY LABELS containers)

  add_executable(f14_prefetch_test f14_prefetch_test.cpp)
  target_link_libraries(f14_prefetch_test unit f14_prefetch)
  add_test(NAME f14_prefetch_test COMMAND f14_prefetch_test)
  set_property(TEST f14_prefetch_test PROPERTY LABELS containers)
endif()
