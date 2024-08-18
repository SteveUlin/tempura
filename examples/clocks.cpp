#include <chrono>
#include <functional>
#include <iostream>

// Clocks:
//
// The goal of this file is to measure the fidelity of different assessors
// for cpu clocks.

// Modern C++ Approach:
uint64_t getCppDelta() {
  std::chrono::time_point<std::chrono::high_resolution_clock> a;
  std::chrono::time_point<std::chrono::high_resolution_clock> b;
  a = std::chrono::high_resolution_clock::now();
  b = std::chrono::high_resolution_clock::now();
  return std::chrono::duration_cast<std::chrono::nanoseconds>(b - a).count();
}

// Clang's built-in for reading the cpu clock.
uint64_t getCpuDelta() {
  unsigned long long a;
  unsigned long long b;
  a = __rdtsc();
  b = __rdtsc();
  return b - a;
}

uint64_t getSysCallDelta() {
  struct timespec a;
  struct timespec b;
  clock_gettime(CLOCK_MONOTONIC, &a);
  clock_gettime(CLOCK_MONOTONIC, &b);
  return (b.tv_sec - a.tv_sec) * 1'000'000'000 + (b.tv_nsec - a.tv_nsec);
}

auto measureFidelity(std::function<int64_t()> func) {
  constexpr uint64_t N = 10'000'000;
  uint64_t total = 0;
  double variance = 0;
  for (uint64_t i = 1; i <= N; i++) {
    uint64_t x = func();
    double prev = static_cast<double>(total) / i;
    total += x;
    double curr = static_cast<double>(total) / i;
    variance += (x - prev) * (x - curr);
  }
  return std::tuple{static_cast<double>(total) / N, std::sqrt(variance / N)};
}

int main() {
  // ≈ 12.4 ± 4.1 ns
  auto [cpp, cppStd] = measureFidelity(getCppDelta);
  std::cout << std::format("C++ clock: {} ns, std: {}", cpp, cppStd);

  // ≈ 21.1 ± 2.8 ns
  auto [cpu, cpuStd] = measureFidelity(getCpuDelta);
  std::cout << std::format("CPU clock: {} cycles, std: {}", cpu, cpuStd);

  // ≈ 11.8 ± 3.7 ns
  auto [sys, sysStd] = measureFidelity(getSysCallDelta);
  std::cout << std::format("Sys call clock: {} ns, std: {}", sys, sysStd);
  return 0;
}
