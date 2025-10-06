# add_subdirectory(autodiff)
# add_subdirectory(cuda)
# add_subdirectory(matrix)
# add_subdirectory(symbolic)  # Removed - replaced by symbolic2
add_subdirectory(symbolic2)
add_subdirectory(symbolic3)  # Combinator-based symbolic system
add_subdirectory(bayes)
add_subdirectory(matrix2)
add_subdirectory(matrix3)
add_subdirectory(meta)
add_subdirectory(math)
add_subdirectory(optimization)
add_subdirectory(quadature)
add_subdirectory(simd)
add_subdirectory(special)
add_subdirectory(synchronization)
add_subdirectory(utility)

add_library(benchmark INTERFACE benchmark.h)

add_library(chebyshev INTERFACE chebyshev.h)

add_executable(chebyshev_test chebyshev_test.cpp)
target_link_libraries(chebyshev_test unit chebyshev)
add_test(NAME chebyshev_test COMMAND chebyshev_test)
set_property(TEST chebyshev_test PROPERTY LABELS base)

add_library(interpolate INTERFACE interpolate.h)
add_library(json INTERFACE json.h)
add_library(polynomial INTERFACE polynomial.h)
add_library(sequence INTERFACE sequence.h)
add_library(unit INTERFACE unit.h)
add_library(root_finding INTERFACE root_finding.h)

# ---

add_library(broadcast_array INTERFACE broadcast_array.h)
add_executable(broadcast_array_test broadcast_array_test.cpp)
target_link_libraries(broadcast_array_test broadcast_array unit)

add_library(profiler SHARED)
target_sources(profiler PUBLIC profiler.h profiler.cpp)


add_library(function_traits INTERFACE function_traits.h)

add_library(image INTERFACE image.h)
add_executable(image_test image_test.cpp)
target_link_libraries(image_test image unit)


