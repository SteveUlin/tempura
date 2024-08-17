#include <cstdio>
#include <cuda_runtime.h>
#include <cuda_runtime_api.h>

__global__ auto helloCUDA() -> void {
  printf("Hello CUDA from GPU!\n");
}

auto main() -> int {
  helloCUDA<<<1, 1>>>();
  cudaDeviceSynchronize();
  return 0;
}
