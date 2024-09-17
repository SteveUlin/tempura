add_library(matrix INTERFACE matrix.h)

add_executable(slice_test slice_test.cpp)
target_link_libraries(slice_test matrix dense unit profiler)

add_library(addition INTERFACE addition.h)
add_executable(addition_test addition_test.cpp)
target_link_libraries(addition_test matrix dense unit)

add_library(multiplication INTERFACE multiplication.h)
add_executable(multiplication_test multiplication_test.cpp)
target_link_libraries(multiplication_test matrix dense unit profiler addition printer)

add_library(printer INTERFACE printer.h)
add_executable(printer_test printer_test.cpp)
target_link_libraries(printer_test matrix dense unit)

add_library(kronecker_product INTERFACE kronecker_product.h)
add_executable(kronecker_product_test kronecker_product_test.cpp)
target_link_libraries(kronecker_product_test kronecker_product unit identity)

add_subdirectory(storage)
add_subdirectory(algorithms)

