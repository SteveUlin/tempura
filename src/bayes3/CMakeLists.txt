add_executable(bayes3_test core_test.cpp)
target_include_directories(bayes3_test PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(bayes3_test PRIVATE unit)
add_test(NAME bayes3_test COMMAND bayes3_test)
