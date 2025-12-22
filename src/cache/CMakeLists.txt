add_library(cache INTERFACE)
target_sources(cache INTERFACE
  lru_cache.h
  lru_k_cache.h
  lfu_cache.h
  fifo_cache.h
  clock_cache.h
  random_cache.h
  mru_cache.h
  lifo_cache.h
  mfu_cache.h
)
target_include_directories(cache INTERFACE
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(cache INTERFACE tempura_flags)
add_library(tempura::cache ALIAS cache)

if(NOT TEMPURA_BUILD_TESTS)
  return()
endif()

add_executable(cache_test cache_test.cpp)
target_link_libraries(cache_test cache unit)

add_executable(cache_benchmark cache_benchmark.cpp)
target_link_libraries(cache_benchmark cache)
