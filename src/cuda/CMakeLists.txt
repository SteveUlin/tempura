add_executable(hello_cuda hello_world.cu)
target_link_libraries(hello_cuda CUDA::cudart)

# add_executable(add_ten add_ten.cu)
# add_executable(pi pi.cu)

