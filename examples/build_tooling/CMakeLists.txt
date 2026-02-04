# ============================================================================
# Build Tooling Examples
# ============================================================================
#
# Standalone demonstrations of compile-time tooling: timeout wrappers,
# profiling, and static assertion patterns.
#
# Targets:
#   build_tooling_profile       — Template-heavy symbolic4 code for -ftime-report
#   build_tooling_assert_test   — Verifies static_assert best practices
#
# Compile timeout usage (from project root):
#   cmake -B build -DTEMPURA_COMPILE_TIMEOUT=120
#   cmake --build build
#   # Files killed after 120s get a clear diagnostic.
#   # Files exceeding 60s (half) get a warning.
#
# Compile profiling usage:
#   cmake -B build -DTEMPURA_PROFILE_COMPILE=ON
#   cmake --build build --target build_tooling_profile 2>profile.log
#   grep "TOTAL" profile.log   # GCC phase timing summary
#
# ============================================================================

# --- Profiling example: uses symbolic4 headers ---
add_executable(build_tooling_profile profile_example.cpp)
target_link_libraries(build_tooling_profile PRIVATE symbolic4)

# --- LTO benchmark: for comparing compile time vs runtime tradeoff ---
add_executable(build_tooling_bench bench_lto.cpp)
target_link_libraries(build_tooling_bench PRIVATE symbolic4)

# --- Static assertion demo test ---
add_executable(build_tooling_assert_test static_assert_demo_test.cc)
target_link_libraries(build_tooling_assert_test PRIVATE unit)
target_include_directories(build_tooling_assert_test PRIVATE
  ${CMAKE_SOURCE_DIR}
)

if(TEMPURA_BUILD_TESTS)
  add_test(NAME build_tooling_assert_test COMMAND build_tooling_assert_test)
  set_tests_properties(build_tooling_assert_test PROPERTIES LABELS "build_tooling")
endif()
