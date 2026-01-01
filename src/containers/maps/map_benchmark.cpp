#include <chrono>
#include <cstdint>
#include <print>
#include <random>
#include <string>
#include <vector>

#include "containers/maps/double_hashing.h"
#include "containers/maps/linear_probing.h"
#include "containers/maps/quadratic_probing.h"
#include "containers/maps/separate_chaining.h"
#include "plot.h"

using namespace tempura;

// ============================================================================
// Timing Utilities
// ============================================================================

struct TimingResult {
  double ns_per_op;
  std::size_t ops;
};

template <typename Fn>
auto timeOps(Fn&& fn, std::size_t ops) -> TimingResult {
  auto start = std::chrono::high_resolution_clock::now();
  fn();
  auto end = std::chrono::high_resolution_clock::now();
  auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
  return {static_cast<double>(ns.count()) / static_cast<double>(ops), ops};
}

// ============================================================================
// Benchmark Functions
// ============================================================================

template <typename Map>
auto benchmarkInsert(std::size_t n, std::mt19937& rng) -> double {
  std::vector<int> keys(n);
  for (std::size_t i = 0; i < n; ++i) {
    keys[i] = static_cast<int>(rng());
  }

  auto result = timeOps(
      [&] {
        Map map;
        for (int key : keys) {
          map.insert(key, key);
        }
      },
      n);
  return result.ns_per_op;
}

template <typename Map>
auto benchmarkFind(std::size_t n, double hit_rate, std::mt19937& rng) -> double {
  Map map;
  std::vector<int> keys(n);
  for (std::size_t i = 0; i < n; ++i) {
    keys[i] = static_cast<int>(rng());
    map.insert(keys[i], keys[i]);
  }

  // Generate lookup keys - mix of hits and misses
  std::vector<int> lookups(n);
  std::uniform_real_distribution<double> dist{0.0, 1.0};
  for (std::size_t i = 0; i < n; ++i) {
    if (dist(rng) < hit_rate) {
      lookups[i] = keys[rng() % n];  // Hit
    } else {
      lookups[i] = static_cast<int>(rng()) | 0x80000000;  // Likely miss
    }
  }

  volatile int sink = 0;
  auto result = timeOps(
      [&] {
        for (int key : lookups) {
          auto* ptr = map.find(key);
          if (ptr) sink = *ptr;
        }
      },
      n);
  return result.ns_per_op;
}

template <typename Map>
auto benchmarkMixed(std::size_t n, std::mt19937& rng) -> double {
  // 50% insert, 30% find, 20% erase
  Map map;
  std::vector<int> existing_keys;
  existing_keys.reserve(n);

  std::uniform_real_distribution<double> op_dist{0.0, 1.0};
  volatile int sink = 0;

  auto result = timeOps(
      [&] {
        for (std::size_t i = 0; i < n; ++i) {
          double r = op_dist(rng);
          if (r < 0.5 || existing_keys.empty()) {
            // Insert
            int key = static_cast<int>(rng());
            map.insert(key, key);
            existing_keys.push_back(key);
          } else if (r < 0.8) {
            // Find
            int key = existing_keys[rng() % existing_keys.size()];
            auto* ptr = map.find(key);
            if (ptr) sink = *ptr;
          } else {
            // Erase
            std::size_t idx = rng() % existing_keys.size();
            map.erase(existing_keys[idx]);
            existing_keys[idx] = existing_keys.back();
            existing_keys.pop_back();
          }
        }
      },
      n);
  return result.ns_per_op;
}

// Benchmark clustering behavior with pathological hash
struct ClusteringHash {
  auto operator()(int key) const -> std::size_t {
    // Hash to only 16 buckets - forces lots of collisions
    return static_cast<std::size_t>(key & 0xF);
  }
};

template <typename Map>
auto benchmarkClustering(std::size_t n, std::mt19937& rng) -> double {
  std::vector<int> keys(n);
  for (std::size_t i = 0; i < n; ++i) {
    keys[i] = static_cast<int>(rng());
  }

  auto result = timeOps(
      [&] {
        Map map;
        for (int key : keys) {
          map.insert(key, key);
        }
        // Now find them all
        for (int key : keys) {
          volatile auto* ptr = map.find(key);
          (void)ptr;
        }
      },
      n * 2);
  return result.ns_per_op;
}

// ============================================================================
// Result Formatting
// ============================================================================

struct BenchResult {
  std::string name;
  RGB color;
  double linear_ns;
  double quadratic_ns;
  double double_hash_ns;
  double chaining_ns;
};

void printBarChart(const std::string& title,
                   const std::vector<BenchResult>& results) {
  std::println("\n{}", title);
  std::println("────────────────────────────────────────────────────────────────────");

  for (const auto& r : results) {
    // Find max for scaling
    double max_ns =
        std::max({r.linear_ns, r.quadratic_ns, r.double_hash_ns, r.chaining_ns});
    double scale = 40.0 / max_ns;

    std::println("{}:", r.name);

    auto bar = [scale](double ns, const char* label, RGB color) {
      int len = static_cast<int>(ns * scale);
      std::print("  {:>12} ", label);
      std::print("{}", color.ansiPrefix());
      for (int i = 0; i < len; ++i) std::print("█");
      std::println("{} {:.1f} ns", RGB::ansiSuffix(), ns);
    };

    bar(r.linear_ns, "Linear", colors::kCyan);
    bar(r.quadratic_ns, "Quadratic", colors::kGreen);
    bar(r.double_hash_ns, "Double", colors::kMagenta);
    bar(r.chaining_ns, "Chaining", colors::kYellow);
    std::println("");
  }
}

void printComparisonTable(const std::vector<BenchResult>& results) {
  std::println("\n╔════════════════════════════════════════════════════════════════════════════╗");
  std::println("║                       PERFORMANCE COMPARISON (ns/op)                       ║");
  std::println("╠══════════════════════╦════════╦════════╦════════╦════════╦═════════════════╣");
  std::println("║ Benchmark            ║ Linear ║  Quad  ║ Double ║ Chain  ║     Winner      ║");
  std::println("╠══════════════════════╬════════╬════════╬════════╬════════╬═════════════════╣");

  for (const auto& r : results) {
    double min_ns = std::min({r.linear_ns, r.quadratic_ns, r.double_hash_ns, r.chaining_ns});
    const char* winner = "?";
    RGB winner_color = colors::kDefault;

    if (min_ns == r.linear_ns) {
      winner = "Linear";
      winner_color = colors::kCyan;
    } else if (min_ns == r.quadratic_ns) {
      winner = "Quadratic";
      winner_color = colors::kGreen;
    } else if (min_ns == r.double_hash_ns) {
      winner = "Double";
      winner_color = colors::kMagenta;
    } else {
      winner = "Chaining";
      winner_color = colors::kYellow;
    }

    std::println("║ {:20} ║ {:>6.1f} ║ {:>6.1f} ║ {:>6.1f} ║ {:>6.1f} ║ {}{:^15}{}║",
                 r.name, r.linear_ns, r.quadratic_ns, r.double_hash_ns, r.chaining_ns,
                 winner_color.ansiPrefix(), winner, RGB::ansiSuffix());
  }

  std::println("╚══════════════════════╩════════╩════════╩════════╩════════╩═════════════════╝");
}

void printSizeScalingChart(std::mt19937& rng) {
  std::println("\n╔════════════════════════════════════════════════════════════════════════════╗");
  std::println("║                     INSERT PERFORMANCE vs SIZE (ns/op)                     ║");
  std::println("╚════════════════════════════════════════════════════════════════════════════╝");

  std::vector<std::size_t> sizes = {1000, 10000, 100000, 1000000};

  std::vector<Series> series;

  // Collect data points
  std::vector<double> linear_times, quad_times, double_times, chain_times;
  std::vector<double> x_values;

  for (std::size_t size : sizes) {
    x_values.push_back(static_cast<double>(size));

    std::mt19937 rng1{42}, rng2{42}, rng3{42}, rng4{42};
    linear_times.push_back(
        benchmarkInsert<LinearProbingMap<int, int>>(size, rng1));
    quad_times.push_back(
        benchmarkInsert<QuadraticProbingMap<int, int>>(size, rng2));
    double_times.push_back(
        benchmarkInsert<DoubleHashingMap<int, int>>(size, rng3));
    chain_times.push_back(
        benchmarkInsert<SeparateChainingMap<int, int>>(size, rng4));
  }

  // Print table
  std::println("{:>10} {:>10} {:>10} {:>10} {:>10}", "Size", "Linear", "Quad",
               "Double", "Chain");
  std::println("──────────────────────────────────────────────────────────");
  for (std::size_t i = 0; i < sizes.size(); ++i) {
    std::println("{:>10} {:>10.1f} {:>10.1f} {:>10.1f} {:>10.1f}", sizes[i],
                 linear_times[i], quad_times[i], double_times[i], chain_times[i]);
  }

  // ASCII bar comparison for largest size
  std::println("\nAt N = 1,000,000:");
  double max_time =
      std::max({linear_times.back(), quad_times.back(), double_times.back(), chain_times.back()});
  double scale = 50.0 / max_time;

  auto bar = [scale](double ns, const char* label, RGB color) {
    int len = static_cast<int>(ns * scale);
    std::print("  {:>10} ", label);
    std::print("{}", color.ansiPrefix());
    for (int i = 0; i < len; ++i) std::print("▓");
    std::println("{} {:.1f} ns/op", RGB::ansiSuffix(), ns);
  };

  bar(linear_times.back(), "Linear", colors::kCyan);
  bar(quad_times.back(), "Quadratic", colors::kGreen);
  bar(double_times.back(), "Double", colors::kMagenta);
  bar(chain_times.back(), "Chaining", colors::kYellow);
}

void printClusteringAnalysis(std::mt19937& rng) {
  std::println("\n╔════════════════════════════════════════════════════════════════════════════╗");
  std::println("║                CLUSTERING RESISTANCE (pathological hash)                   ║");
  std::println("╚════════════════════════════════════════════════════════════════════════════╝");

  std::println("Using hash that maps all keys to only 16 buckets...\n");

  constexpr std::size_t n = 100000;

  std::mt19937 rng1{42}, rng2{42}, rng3{42}, rng4{42};

  // Custom hash that causes clustering
  double linear_ns =
      benchmarkClustering<LinearProbingMap<int, int, ClusteringHash>>(n, rng1);
  double quad_ns =
      benchmarkClustering<QuadraticProbingMap<int, int, ClusteringHash>>(n, rng2);

  // For double hashing, we need both hashes to be bad
  struct ClusteringHash2 {
    auto operator()(int key) const -> std::size_t {
      return static_cast<std::size_t>((key >> 4) & 0x7) + 1;  // [1, 8]
    }
  };
  double double_ns = benchmarkClustering<
      DoubleHashingMap<int, int, ClusteringHash, ClusteringHash2>>(n, rng3);
  double chain_ns =
      benchmarkClustering<SeparateChainingMap<int, int, ClusteringHash>>(n, rng4);

  double max_ns = std::max({linear_ns, quad_ns, double_ns, chain_ns});
  double scale = 50.0 / max_ns;

  auto bar = [scale](double ns, const char* label, RGB color) {
    int len = static_cast<int>(ns * scale);
    std::print("  {:>10} ", label);
    std::print("{}", color.ansiPrefix());
    for (int i = 0; i < len; ++i) std::print("█");
    std::println("{} {:.1f} ns/op", RGB::ansiSuffix(), ns);
  };

  bar(linear_ns, "Linear", colors::kCyan);
  bar(quad_ns, "Quadratic", colors::kGreen);
  bar(double_ns, "Double", colors::kMagenta);
  bar(chain_ns, "Chaining", colors::kYellow);

  // Analysis
  std::println("\nAnalysis:");
  if (linear_ns > quad_ns && linear_ns > double_ns && linear_ns > chain_ns) {
    std::println("  → Linear probing suffers most from primary clustering");
  }
  if (quad_ns < linear_ns) {
    std::println("  → Quadratic probing reduces primary clustering");
  }
  if (double_ns < linear_ns) {
    std::println("  → Double hashing reduces clustering via key-dependent steps");
  }
  if (chain_ns < linear_ns && chain_ns < quad_ns && chain_ns < double_ns) {
    std::println("  → Separate chaining is immune to clustering (lists just get longer)");
  }
}

void printSummary() {
  std::println("\n");
  std::println("╔════════════════════════════════════════════════════════════════════════════╗");
  std::println("║                               SUMMARY                                      ║");
  std::println("╠════════════════════════════════════════════════════════════════════════════╣");
  std::println("║                                                                            ║");
  std::println("║  Linear Probing:                                                           ║");
  std::println("║    ✓ Best cache locality (sequential access)                               ║");
  std::println("║    ✓ Simplest implementation                                               ║");
  std::println("║    ✗ Suffers from primary clustering                                       ║");
  std::println("║                                                                            ║");
  std::println("║  Quadratic Probing:                                                        ║");
  std::println("║    ✓ Reduces primary clustering                                            ║");
  std::println("║    ✓ Good balance of simplicity and performance                            ║");
  std::println("║    ✗ Secondary clustering (same hash → same probe sequence)                ║");
  std::println("║                                                                            ║");
  std::println("║  Double Hashing:                                                           ║");
  std::println("║    ✓ Eliminates both clustering types                                      ║");
  std::println("║    ✓ Most uniform probe distribution                                       ║");
  std::println("║    ✗ Two hash computations per probe                                       ║");
  std::println("║    ✗ Requires prime table sizes                                            ║");
  std::println("║                                                                            ║");
  std::println("║  Separate Chaining:                                                        ║");
  std::println("║    ✓ No clustering (independent bucket lists)                              ║");
  std::println("║    ✓ Simple deletion (no tombstones)                                       ║");
  std::println("║    ✓ Load factor can exceed 1.0                                            ║");
  std::println("║    ✗ Extra memory for node pointers                                        ║");
  std::println("║    ✗ Cache-unfriendly (pointer chasing)                                    ║");
  std::println("║    ✗ More allocations (one per insert)                                     ║");
  std::println("║                                                                            ║");
  std::println("╚════════════════════════════════════════════════════════════════════════════╝");
}

auto main() -> int {
  std::println("╔════════════════════════════════════════════════════════════════════════════╗");
  std::println("║                      HASH MAP STRATEGY BENCHMARK                           ║");
  std::println("║                                                                            ║");
  std::println("║  Comparing: Linear │ Quadratic │ Double Hashing │ Separate Chaining        ║");
  std::println("╚════════════════════════════════════════════════════════════════════════════╝");

  std::mt19937 rng{42};

  // Collect benchmark results
  std::vector<BenchResult> results;

  // Insert benchmark
  {
    constexpr std::size_t n = 500000;
    std::mt19937 rng1{42}, rng2{42}, rng3{42}, rng4{42};
    results.push_back({"Insert (500k)",
                       colors::kYellow,
                       benchmarkInsert<LinearProbingMap<int, int>>(n, rng1),
                       benchmarkInsert<QuadraticProbingMap<int, int>>(n, rng2),
                       benchmarkInsert<DoubleHashingMap<int, int>>(n, rng3),
                       benchmarkInsert<SeparateChainingMap<int, int>>(n, rng4)});
  }

  // Find (100% hit rate)
  {
    constexpr std::size_t n = 500000;
    std::mt19937 rng1{42}, rng2{42}, rng3{42}, rng4{42};
    results.push_back(
        {"Find (100% hit)", colors::kGreen,
         benchmarkFind<LinearProbingMap<int, int>>(n, 1.0, rng1),
         benchmarkFind<QuadraticProbingMap<int, int>>(n, 1.0, rng2),
         benchmarkFind<DoubleHashingMap<int, int>>(n, 1.0, rng3),
         benchmarkFind<SeparateChainingMap<int, int>>(n, 1.0, rng4)});
  }

  // Find (50% hit rate)
  {
    constexpr std::size_t n = 500000;
    std::mt19937 rng1{42}, rng2{42}, rng3{42}, rng4{42};
    results.push_back(
        {"Find (50% hit)", colors::kBlue,
         benchmarkFind<LinearProbingMap<int, int>>(n, 0.5, rng1),
         benchmarkFind<QuadraticProbingMap<int, int>>(n, 0.5, rng2),
         benchmarkFind<DoubleHashingMap<int, int>>(n, 0.5, rng3),
         benchmarkFind<SeparateChainingMap<int, int>>(n, 0.5, rng4)});
  }

  // Mixed workload
  {
    constexpr std::size_t n = 500000;
    std::mt19937 rng1{42}, rng2{42}, rng3{42}, rng4{42};
    results.push_back({"Mixed (ins/find/del)",
                       colors::kMagenta,
                       benchmarkMixed<LinearProbingMap<int, int>>(n, rng1),
                       benchmarkMixed<QuadraticProbingMap<int, int>>(n, rng2),
                       benchmarkMixed<DoubleHashingMap<int, int>>(n, rng3),
                       benchmarkMixed<SeparateChainingMap<int, int>>(n, rng4)});
  }

  // Print results
  printComparisonTable(results);
  printBarChart("Performance by Operation Type", results);
  printSizeScalingChart(rng);
  printClusteringAnalysis(rng);
  printSummary();

  return 0;
}
