
#pragma once

#include <atomic>
#include <chrono>
#include <cmath>
#include <functional>
#include <numeric>
#include <print>
#include <string_view>
#include <vector>

namespace tempura {

namespace internal {

constexpr auto diff(timespec start, timespec end) -> timespec {
  return {.tv_sec = end.tv_sec - start.tv_sec,
          .tv_nsec = end.tv_nsec - start.tv_nsec};
}

constexpr auto toDuration(timespec t) -> std::chrono::nanoseconds {
  return std::chrono::seconds{t.tv_sec} + std::chrono::nanoseconds{t.tv_nsec};
}

auto toHumanReadable(std::chrono::nanoseconds duration) -> std::string {
  using namespace std::chrono_literals;
  if (duration < 1us) {
    return std::format("{} ns", duration.count());
  }
  if (duration < 1ms) {
    return std::format(
        "{:.2f} μs",
        std::chrono::duration<double, std::chrono::microseconds::period>(
            duration)
            .count());
  }
  if (duration < 10s) {
    return std::format(
        "{:.2f} ms",
        std::chrono::duration<double, std::chrono::milliseconds::period>(
            duration)
            .count());
  }
  if (duration < 5min) {
    return std::format(
        "{:.2f} s",
        std::chrono::duration<double, std::chrono::seconds::period>(duration)
            .count());
  }
  if (duration < 120min) {
    return std::format(
        "{:.2f} min",
        std::chrono::duration<double, std::chrono::minutes::period>(duration)
            .count());
  }
  return std::format(
      "{:.2f} h",
      std::chrono::duration<double, std::chrono::hours::period>(duration)
          .count());
}
}  // namespace internal

using namespace std::chrono_literals;

class Benchmark {
 public:
  auto ops(std::size_t ops) -> Benchmark& {
    op_scaling_factor_ = ops;
    return *this;
  }

  auto operator=(const std::function<void()>& func) -> void {
    std::vector<std::chrono::nanoseconds> cpu_times;
    cpu_times.reserve(max_cycles_);
    std::vector<std::chrono::nanoseconds> wall_times;
    wall_times.reserve(max_cycles_);

    std::chrono::nanoseconds total_wall_time = 0ns;

    timespec cpu_start;
    std::chrono::time_point<std::chrono::high_resolution_clock> wall_start;

    timespec cpu_end;
    std::chrono::time_point<std::chrono::high_resolution_clock> wall_end;

    std::println("Running... {}", name_);

    std::size_t i = 0;
    for (; i < max_cycles_; ++i) {
      std::atomic_signal_fence(std::memory_order::seq_cst);
      wall_start = std::chrono::high_resolution_clock::now();
      clock_gettime(CLOCK_THREAD_CPUTIME_ID, &cpu_start);

      func();

      clock_gettime(CLOCK_THREAD_CPUTIME_ID, &cpu_end);
      wall_end = std::chrono::high_resolution_clock::now();
      std::atomic_signal_fence(std::memory_order::seq_cst);

      cpu_times.push_back(
          internal::toDuration(internal::diff(cpu_start, cpu_end)));
      wall_times.push_back(wall_end - wall_start);

      total_wall_time += wall_times.back();
      if (total_wall_time > max_runtime && i > min_cycles_) {
        break;
      }
    }
    auto avg_wall = std::chrono::nanoseconds(
        std::accumulate(wall_times.begin(), wall_times.end(), 0ns).count() / i);
    auto std_dev_wall = std::chrono::nanoseconds(
        static_cast<std::size_t>(std::sqrt(std::accumulate(
            wall_times.begin(), wall_times.end(), 0,
            [avg_wall](auto acc, auto val) {
              return acc + std::pow(val.count() - avg_wall.count(), 2);
            }))));

    std::println("Number of iterations: {}", i);
    std::println("Average wall time: {} ± {}",
                 internal::toHumanReadable(avg_wall),
                 internal::toHumanReadable(std_dev_wall));

    std::println(
        "Ops per sec: {:L}",
        static_cast<size_t>(static_cast<double>(op_scaling_factor_) * 1e9 /
                            static_cast<double>(avg_wall.count())));
  }

 private:
  constexpr Benchmark(std::string_view name) : name_(name) {}

  friend constexpr auto operator""_bench(const char* /*unused*/,
                                         std::size_t /*unused*/) -> Benchmark;

  std::string_view name_;
  std::size_t min_cycles_ = 10'000;
  std::size_t max_cycles_ = 10'000'000;
  std::size_t op_scaling_factor_ = 1;
  std::chrono::nanoseconds max_runtime = 10s;
};

[[nodiscard]] constexpr auto operator""_bench(const char* name,
                                              std::size_t size) -> Benchmark {
  return Benchmark{std::string_view{name, size}};
}
}  // namespace tempura
