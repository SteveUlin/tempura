#include <cstdio>
#include <cuda_runtime.h>

__global__ auto helloCUDA() -> void {
  printf("Hello CUDA from GPU!\n");
}

auto main() -> int {
  auto a = [] {
    helloCUDA<<<1, 1>>>();
  };
  cudaDeviceSynchronize();
  return 0;
}
