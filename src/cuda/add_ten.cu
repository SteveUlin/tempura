#include <array>
#include <numeric>
#include <iostream>

__global__ auto VecAdd10(float* A, int N) -> void {
  const size_t i  = blockDim.x * blockIdx.x + threadIdx.x;
  if (i < N) {
    A[i] = A[i] + 10;
  }
}

auto main() -> int {
  constexpr size_t N = 10;
  std::array<float, N> arr;
  std::iota(arr.begin(), arr.end(), 0);

  float* d_arr;
  cudaMalloc(&d_arr, N * sizeof(float));
  cudaMemcpy(d_arr, arr.data(), N * sizeof(float), cudaMemcpyHostToDevice);

  VecAdd10<<<1, N>>>(d_arr, N);

  cudaMemcpy(arr.data(), d_arr, N * sizeof(float), cudaMemcpyDeviceToHost);

  for (const float x : arr) {
    std::cout << x << std::endl;
  }
  return 0;
}

