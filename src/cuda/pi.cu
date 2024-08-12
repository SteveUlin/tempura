#include <curand.h>
#include <curand_kernel.h>

#include <array>
#include <iostream>

inline constexpr size_t kWarpSize = 32;
inline constexpr size_t kCudaCores = 2048;
inline constexpr size_t kIterations = 1000000;

__global__ auto piCount(uint64_t* totals) -> void {
  size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
  totals[idx] = 0;
  curandState state;
  curand_init(1234, idx, 0, &state);
  for (size_t i = 0; i < kIterations; i++) {
    float x = curand_uniform(&state);
    float y = curand_uniform(&state);
    totals[idx] += 1 - static_cast<uint64_t>(x * x + y * y);
  }
}

auto main() -> int {
  constexpr size_t N = kCudaCores * kWarpSize;
  uint64_t* counts;
  cudaMalloc(&counts, N * sizeof(uint64_t));
  piCount<<<kCudaCores, kWarpSize>>>(counts);

  std::array<uint64_t, N> hostCounts;
  cudaMemcpy(hostCounts.data(), counts, N * sizeof(uint64_t), cudaMemcpyDeviceToHost);
  cudaFree(counts);
  
  uint64_t total = 0;
  for (const uint64_t count : hostCounts) {
    total += count;
  }
  const double pi = 4.0 * static_cast<double>(total) / static_cast<double>(kIterations * kCudaCores * kWarpSize);
  std::cout << "Pi: " << pi << std::endl;
  return 0;
}
