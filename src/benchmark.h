#pragma once

#include <algorithm>
#include <atomic>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <optional>
#include <print>
#include <string>
#include <string_view>
#include <vector>

// Runtime-only by necessity: a benchmark's value (elapsed time) exists only at
// runtime, so this is the one brick exempt from the codebase's constexpr-by-default
// rule. Everything else holds — exception-free, no expression templates, terse.

namespace tempura {

// ── Optimizer barriers ──────────────────────────────────────────────────────
// Without these every number is fiction: the optimizer hoists loop-invariant
// inputs and deletes un-observed outputs, and a sub-µs kernel measures ~0.

// Read sink: force the compiler to treat v as observed so its producer can't be
// eliminated. By-reference with "r,m" keeps v in a register or in place with NO
// copy — sidesteps GCC's memcpy-into-the-loop bug for large objects (passing by
// value would re-copy v every iteration and time the copy, not the kernel).
template <typename T>
inline void doNotOptimize(const T& v) {
  asm volatile("" : : "r,m"(v) : "memory");
}
// Escape form: also tells the optimizer v may be written — for in-place results
// (accumulators, factorized workspaces). Same no-copy property.
template <typename T>
inline void doNotOptimize(T& v) {
  // GCC needs the memory alternative first ("+m,r"); "+r,m" makes it try register
  // allocation first and error with "impossible constraint" on non-register types.
  asm volatile("" : "+m,r"(v) : : "memory");
}
// Write barrier: all memory may have changed. Defeats elision of write-only
// fills and forces reloads of inputs across the barrier.
inline void clobberMemory() { asm volatile("" : : : "memory"); }

// Per-op latency in floating-point nanoseconds. Integer ns is right for a wall-clock
// budget, but once auto-batching divides one epoch by millions of reps, integer
// division truncates sub-ns work to 0 — so per-op times live in double.
using FpNanos = std::chrono::duration<double, std::nano>;

namespace internal {

static_assert(std::chrono::steady_clock::is_steady,
              "wall timing needs a monotonic clock; an NTP step on a non-steady "
              "clock yields negative durations that poison every statistic");

constexpr auto diff(timespec start, timespec end) -> timespec {
  return {.tv_sec = end.tv_sec - start.tv_sec, .tv_nsec = end.tv_nsec - start.tv_nsec};
}

constexpr auto toDuration(timespec t) -> std::chrono::nanoseconds {
  return std::chrono::seconds{t.tv_sec} + std::chrono::nanoseconds{t.tv_nsec};
}

auto toHumanReadable(FpNanos duration) -> std::string {
  const double ns = duration.count();
  if (ns < 1e3) return std::format("{:.2f} ns", ns);
  if (ns < 1e6) return std::format("{:.2f} μs", ns / 1e3);
  if (ns < 1e10) return std::format("{:.2f} ms", ns / 1e6);
  if (ns < 3e11) return std::format("{:.2f} s", ns / 1e9);
  return std::format("{:.2f} min", ns / 6e10);
}

// Median by partial sort on a copy (microbench noise is one-sided — interrupts and
// evictions only ever slow a sample — so a 50%-breakdown estimator beats mean±σ).
inline auto medianOf(std::vector<double> v) -> double {
  assert(!v.empty());
  auto mid = v.begin() + static_cast<std::ptrdiff_t>(v.size() / 2);
  std::nth_element(v.begin(), mid, v.end());
  return *mid;
}

// Median absolute deviation, reported raw (no ×1.4826): for one-sided noise the
// raw spread is the honest descriptor, not a normal-σ estimate.
inline auto madOf(const std::vector<double>& v, double med) -> double {
  std::vector<double> dev;
  dev.reserve(v.size());
  for (double x : v) dev.push_back(x > med ? x - med : med - x);
  return medianOf(std::move(dev));
}

// Best-effort sysfs read; absent files (VMs, containers) return nullopt, never throw.
inline auto readSysfs(const char* path) -> std::optional<std::string> {
  std::ifstream f{path};
  if (!f) return std::nullopt;
  std::string s;
  std::getline(f, s);
  if (s.empty()) return std::nullopt;
  return s;
}

// Refuse to silently lie: warn (to stderr, once per process) when the machine is
// in a frequency-varying state no statistic can survive.
inline void warnEnvironmentOnce() {
  static bool done = false;
  if (done) return;
  done = true;
  if (auto g = readSysfs("/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor");
      g && *g != "performance")
    std::println(stderr, "⚠️  CPU governor is '{}', not 'performance'", *g);
  // Two turbo drivers with opposite polarity: acpi/amd expose boost=1=on, intel_pstate
  // exposes no_turbo=0=on.
  if (auto b = readSysfs("/sys/devices/system/cpu/cpufreq/boost"); b && *b == "1")
    std::println(stderr, "⚠️  CPU turbo/boost is on — frequency may vary mid-sweep");
  if (auto t = readSysfs("/sys/devices/system/cpu/intel_pstate/no_turbo"); t && *t == "0")
    std::println(stderr, "⚠️  Intel turbo is on (no_turbo=0) — frequency may vary mid-sweep");
}

}  // namespace internal

using namespace std::chrono_literals;

// Captured measurement (the same numbers printed). Returned by run()/operator= so a
// sweep can build a GFLOP/s-vs-n table without re-parsing stdout.
struct BenchResult {
  FpNanos median_wall{};  // per-op latency; .count() is double ns (sub-ns preserved)
  FpNanos mad_wall{};
  FpNanos min_wall{};
  FpNanos median_cpu{};
  double ops_per_sec = 0.0;
  double gflops = 0.0;  // 0 unless flops() was set
  double gbytes = 0.0;  // 0 unless bytes() was set
  std::size_t samples = 0;
  std::size_t batch = 0;  // auto-chosen inner repeat per timed epoch
};

class Benchmark {
 public:
  // ── chainable configuration ──
  auto ops(std::size_t n) -> Benchmark& {            // logical ops per func() call (rate)
    op_scaling_factor_ = n;
    return *this;
  }
  auto warmup(std::size_t n) -> Benchmark& {         // untimed pre-runs (cache/page warm)
    warmup_cycles_ = n;
    return *this;
  }
  auto flops(double f) -> Benchmark& {               // FLOPs per func() call → GFLOP/s
    flops_per_call_ = f;
    return *this;
  }
  auto bytes(double b) -> Benchmark& {               // bytes touched per call → GB/s
    bytes_per_call_ = b;
    return *this;
  }
  auto minEpoch(std::chrono::nanoseconds t) -> Benchmark& {  // batch-calibration target
    min_epoch_ = t;
    return *this;
  }
  auto maxRuntime(std::chrono::nanoseconds t) -> Benchmark& {
    max_runtime_ = t;
    return *this;
  }
  auto minSamples(std::size_t n) -> Benchmark& {
    min_samples_ = n;
    return *this;
  }
  auto quiet() -> Benchmark& {  // measure without printing (oracle/scripted capture)
    quiet_ = true;
    return *this;
  }

  // Measure func and report. Templated (not std::function) so the body inlines into
  // the timing loop — no type-erased indirect call inside the measured region.
  template <typename F>
  auto run(F&& func) -> BenchResult {
    for (std::size_t w = 0; w < warmup_cycles_; ++w) {  // untimed warmup
      func();
      std::atomic_signal_fence(std::memory_order::seq_cst);
    }

    const std::size_t batch = calibrateBatch(func);

    std::vector<double> wall;  // per-op nanoseconds, in double (see FpNanos)
    std::vector<double> cpu;
    std::chrono::nanoseconds total{0};
    const double b = static_cast<double>(batch);
    while (wall.size() < max_samples_) {
      timespec c0{};
      timespec c1{};
      std::atomic_signal_fence(std::memory_order::seq_cst);
      const auto w0 = std::chrono::steady_clock::now();
      clock_gettime(CLOCK_THREAD_CPUTIME_ID, &c0);
      runBatch(func, batch);
      clock_gettime(CLOCK_THREAD_CPUTIME_ID, &c1);
      const auto w1 = std::chrono::steady_clock::now();
      std::atomic_signal_fence(std::memory_order::seq_cst);

      const auto wall_batch = std::chrono::duration_cast<std::chrono::nanoseconds>(w1 - w0);
      wall.push_back(static_cast<double>(wall_batch.count()) / b);
      cpu.push_back(static_cast<double>(internal::toDuration(internal::diff(c0, c1)).count()) / b);
      total += wall_batch;
      if (total > max_runtime_ && wall.size() >= min_samples_) break;
    }

    BenchResult r;
    r.batch = batch;
    r.samples = wall.size();
    const double med = internal::medianOf(wall);
    r.median_wall = FpNanos{med};
    r.mad_wall = FpNanos{internal::madOf(wall, med)};
    r.min_wall = FpNanos{*std::min_element(wall.begin(), wall.end())};
    r.median_cpu = FpNanos{internal::medianOf(cpu)};
    const double med_ns = med;
    if (med_ns > 0.0) {
      r.ops_per_sec = static_cast<double>(op_scaling_factor_) * 1e9 / med_ns;
      if (flops_per_call_ > 0.0) r.gflops = flops_per_call_ / med_ns;  // flop/ns ≡ GFLOP/s
      if (bytes_per_call_ > 0.0) r.gbytes = bytes_per_call_ / med_ns;  // byte/ns ≡ GB/s
    }

    if (!quiet_) report(r);
    return r;
  }

  // Sugar: `"name"_bench = []{ ... };` runs immediately. Returns the result (no
  // existing call site reads it, so the value is simply discarded there).
  template <typename F>
  auto operator=(F&& func) -> BenchResult {
    return run(std::forward<F>(func));
  }

 private:
  constexpr Benchmark(std::string_view name) : name_{name} {}

  friend constexpr auto operator""_bench(const char* /*unused*/, std::size_t /*unused*/)
      -> Benchmark;

  template <typename F>
  static void runBatch(F& func, std::size_t batch) {
    for (std::size_t i = 0; i < batch; ++i) {
      func();
      // Compiler fence between reps: a side-effect-free func() would otherwise be
      // CSE'd to a single call and the loop counted as one iteration.
      std::atomic_signal_fence(std::memory_order::seq_cst);
    }
  }

  // Grow the inner repeat until one batch lasts ≥ min_epoch_, so the two clock reads
  // amortize to ~0/op — the fix that lets sub-µs kernels be timed honestly.
  template <typename F>
  auto calibrateBatch(F& func) -> std::size_t {
    std::size_t batch = 1;
    while (batch < max_batch_) {
      const auto t0 = std::chrono::steady_clock::now();
      runBatch(func, batch);
      const auto dur = std::chrono::steady_clock::now() - t0;
      if (dur >= min_epoch_) break;
      if (dur <= std::chrono::nanoseconds{0}) {  // below clock granularity — double blind
        batch *= 2;
        continue;
      }
      const double grow = static_cast<double>(min_epoch_.count()) /
                          static_cast<double>(std::chrono::duration_cast<std::chrono::nanoseconds>(dur).count());
      const auto next = static_cast<std::size_t>(static_cast<double>(batch) * std::min(grow, 8.0));
      batch = std::max(batch + 1, next);
    }
    return batch;
  }

  void report(const BenchResult& r) const {
    std::println("Running... {}", name_);
    std::println("  wall median {} (best {}, MAD {})  [batch {}, {} samples]",
                 internal::toHumanReadable(r.median_wall),
                 internal::toHumanReadable(r.min_wall),
                 internal::toHumanReadable(r.mad_wall), r.batch, r.samples);
    std::println("  cpu  median {}", internal::toHumanReadable(r.median_cpu));
    if (r.gflops > 0.0) std::println("  {:.2f} GFLOP/s", r.gflops);
    if (r.gbytes > 0.0) std::println("  {:.2f} GB/s", r.gbytes);
    std::println("  ops/sec: {:L}", static_cast<std::size_t>(r.ops_per_sec));

    const double med_ns = static_cast<double>(r.median_wall.count());
    if (med_ns > 0.0 && static_cast<double>(r.mad_wall.count()) / med_ns > 0.05)
      std::println(stderr, "⚠️  {}: unstable, MAD/median = {:.1f}%", name_,
                   100.0 * static_cast<double>(r.mad_wall.count()) / med_ns);
    internal::warnEnvironmentOnce();
  }

  std::string_view name_;
  std::size_t op_scaling_factor_ = 1;
  std::size_t warmup_cycles_ = 100;
  std::size_t min_samples_ = 16;             // sample floor for a stable median
  std::size_t max_samples_ = 10'000'000;
  std::size_t max_batch_ = std::size_t{1} << 30;
  std::chrono::nanoseconds min_epoch_ = 1ms;       // batch-calibration target
  std::chrono::nanoseconds max_runtime_ = 1s;      // total wall budget; then stop
  double flops_per_call_ = 0.0;
  double bytes_per_call_ = 0.0;
  bool quiet_ = false;
};

[[nodiscard]] constexpr auto operator""_bench(const char* name, std::size_t size)
    -> Benchmark {
  return Benchmark{std::string_view{name, size}};
}

}  // namespace tempura
