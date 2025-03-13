add_library(normal INTERFACE normal.h)
add_executable(normal_test normal_test.cpp)
target_link_libraries(normal_test normal unit)

add_library(beta INTERFACE beta.h)
add_executable(beta_test beta_test.cpp)
target_link_libraries(beta_test beta unit)

add_library(bernoulli INTERFACE bernoulli.h)
add_executable(bernoulli_test bernoulli_test.cpp)
target_link_libraries(bernoulli_test bernoulli unit)

add_library(bayes INTERFACE)
target_include_directories(bayes INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
