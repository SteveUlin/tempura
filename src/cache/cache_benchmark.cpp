#include <algorithm>
#include <cmath>
#include <cstdint>
#include <print>
#include <random>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include "cache/clock_cache.h"
#include "cache/fifo_cache.h"
#include "cache/lfu_cache.h"
#include "cache/lifo_cache.h"
#include "cache/lru_cache.h"
#include "cache/lru_k_cache.h"
#include "cache/mfu_cache.h"
#include "cache/mru_cache.h"
#include "cache/random_cache.h"
#include "plot.h"

using namespace tempura;

// Generate Zipfian-distributed accesses (realistic cache workload)
// Lower alpha = more uniform, higher alpha = more skewed to hot keys
auto generateZipfianAccesses(std::size_t num_keys, std::size_t num_accesses,
                             double alpha, std::mt19937& rng)
    -> std::vector<int> {
  // Precompute CDF for Zipfian distribution
  std::vector<double> cdf(num_keys);
  double sum = 0.0;
  for (std::size_t i = 0; i < num_keys; ++i) {
    sum += 1.0 / std::pow(static_cast<double>(i + 1), alpha);
    cdf[i] = sum;
  }
  for (auto& v : cdf) {
    v /= sum;
  }

  std::uniform_real_distribution<double> dist{0.0, 1.0};
  std::vector<int> accesses;
  accesses.reserve(num_accesses);

  for (std::size_t i = 0; i < num_accesses; ++i) {
    double r = dist(rng);
    auto it = std::lower_bound(cdf.begin(), cdf.end(), r);
    int key = static_cast<int>(std::distance(cdf.begin(), it));
    accesses.push_back(key);
  }

  return accesses;
}

// Belady's optimal algorithm - requires knowing future accesses
auto computeOptimalHitRate(const std::vector<int>& accesses,
                           std::size_t cache_size) -> double {
  if (cache_size == 0) return 0.0;

  std::set<int> cache;
  std::size_t hits = 0;

  // Build next-use index for each position
  std::vector<std::size_t> next_use(accesses.size(),
                                    std::numeric_limits<std::size_t>::max());
  std::unordered_map<int, std::size_t> last_seen;

  // Scan backwards to find next use of each key
  for (std::size_t i = accesses.size(); i-- > 0;) {
    int key = accesses[i];
    if (last_seen.contains(key)) {
      next_use[i] = last_seen[key];
    }
    last_seen[key] = i;
  }

  for (std::size_t i = 0; i < accesses.size(); ++i) {
    int key = accesses[i];

    if (cache.contains(key)) {
      ++hits;
    } else {
      if (cache.size() >= cache_size) {
        // Evict the key used furthest in the future
        int victim = -1;
        std::size_t furthest = 0;

        for (int k : cache) {
          // Find next use of k after position i
          std::size_t next = std::numeric_limits<std::size_t>::max();
          for (std::size_t j = i + 1; j < accesses.size(); ++j) {
            if (accesses[j] == k) {
              next = j;
              break;
            }
          }
          if (next > furthest) {
            furthest = next;
            victim = k;
          }
        }
        cache.erase(victim);
      }
      cache.insert(key);
    }
  }

  return static_cast<double>(hits) / static_cast<double>(accesses.size());
}

// Generic cache benchmark
template <typename Cache>
auto benchmarkCache(const std::vector<int>& accesses, std::size_t cache_size)
    -> double {
  if (cache_size == 0) return 0.0;

  Cache cache{cache_size};
  std::size_t hits = 0;

  for (int key : accesses) {
    if (cache.get(key).has_value()) {
      ++hits;
    } else {
      cache.insert(key, key);  // Value doesn't matter for hit rate
    }
  }

  return static_cast<double>(hits) / static_cast<double>(accesses.size());
}

// Benchmark random cache with seeded RNG
auto benchmarkRandomCache(const std::vector<int>& accesses,
                          std::size_t cache_size, std::uint32_t seed) -> double {
  if (cache_size == 0) return 0.0;

  RandomCache<int, int, std::mt19937> cache{cache_size, std::mt19937{seed}};
  std::size_t hits = 0;

  for (int key : accesses) {
    if (cache.get(key).has_value()) {
      ++hits;
    } else {
      cache.insert(key, key);
    }
  }

  return static_cast<double>(hits) / static_cast<double>(accesses.size());
}

struct BenchmarkResult {
  std::string name;
  RGB color;
  std::vector<double> hit_rates;  // Indexed by cache size %
};

void runBenchmark(double zipf_alpha, std::size_t num_keys,
                  std::size_t num_accesses, std::uint32_t seed,
                  bool show_table = true, bool show_plots = true) {
  std::mt19937 rng{seed};
  auto accesses =
      generateZipfianAccesses(num_keys, num_accesses, zipf_alpha, rng);

  std::println("\n╔═══════════════════════════════════════════════════════════════╗");
  std::println("║  Zipf α = {:<6.2f}                                              ║", zipf_alpha);
  std::println("╚═══════════════════════════════════════════════════════════════╝");

  if (zipf_alpha < 0.5) {
    std::println("  → Nearly uniform - all keys accessed similarly");
  } else if (zipf_alpha < 1.0) {
    std::println("  → Mild skew - some keys more popular");
  } else if (zipf_alpha < 1.5) {
    std::println("  → Typical web workload - clear hot/cold separation");
  } else {
    std::println("  → Extreme skew - few keys dominate");
  }
  std::println("");

  // Cache sizes as percentage of key pool
  std::vector<double> cache_percentages = {1,  2,  5, 10, 20, 30, 50};

  std::vector<BenchmarkResult> results;

  // Optimal (Belady's algorithm)
  BenchmarkResult optimal{"Optimal", colors::kGreen, {}};
  for (double pct : cache_percentages) {
    std::size_t size = static_cast<std::size_t>(num_keys * pct / 100.0);
    if (size == 0) size = 1;
    optimal.hit_rates.push_back(computeOptimalHitRate(accesses, size) * 100.0);
  }
  results.push_back(optimal);

  // LRU
  BenchmarkResult lru{"LRU", colors::kCyan, {}};
  for (double pct : cache_percentages) {
    std::size_t size = static_cast<std::size_t>(num_keys * pct / 100.0);
    if (size == 0) size = 1;
    lru.hit_rates.push_back(
        benchmarkCache<LruCache<int, int>>(accesses, size) * 100.0);
  }
  results.push_back(lru);

  // LFU
  BenchmarkResult lfu{"LFU", colors::kBlue, {}};
  for (double pct : cache_percentages) {
    std::size_t size = static_cast<std::size_t>(num_keys * pct / 100.0);
    if (size == 0) size = 1;
    lfu.hit_rates.push_back(
        benchmarkCache<LfuCache<int, int>>(accesses, size) * 100.0);
  }
  results.push_back(lfu);

  // FIFO
  BenchmarkResult fifo{"FIFO", colors::kYellow, {}};
  for (double pct : cache_percentages) {
    std::size_t size = static_cast<std::size_t>(num_keys * pct / 100.0);
    if (size == 0) size = 1;
    fifo.hit_rates.push_back(
        benchmarkCache<FifoCache<int, int>>(accesses, size) * 100.0);
  }
  results.push_back(fifo);

  // Clock
  BenchmarkResult clock_cache{"Clock", colors::kMagenta, {}};
  for (double pct : cache_percentages) {
    std::size_t size = static_cast<std::size_t>(num_keys * pct / 100.0);
    if (size == 0) size = 1;
    clock_cache.hit_rates.push_back(
        benchmarkCache<ClockCache<int, int>>(accesses, size) * 100.0);
  }
  results.push_back(clock_cache);

  // Random
  BenchmarkResult random_cache{"Random", colors::kOrange, {}};
  for (double pct : cache_percentages) {
    std::size_t size = static_cast<std::size_t>(num_keys * pct / 100.0);
    if (size == 0) size = 1;
    random_cache.hit_rates.push_back(
        benchmarkRandomCache(accesses, size, seed) * 100.0);
  }
  results.push_back(random_cache);

  // LRU-2
  BenchmarkResult lru2{"LRU-2", RGB{150, 200, 255}, {}};
  for (double pct : cache_percentages) {
    std::size_t size = static_cast<std::size_t>(num_keys * pct / 100.0);
    if (size == 0) size = 1;
    lru2.hit_rates.push_back(
        benchmarkCache<LruKCache<int, int, 2>>(accesses, size) * 100.0);
  }
  results.push_back(lru2);

  // MRU (bad cache)
  BenchmarkResult mru{"MRU", colors::kRed, {}};
  for (double pct : cache_percentages) {
    std::size_t size = static_cast<std::size_t>(num_keys * pct / 100.0);
    if (size == 0) size = 1;
    mru.hit_rates.push_back(
        benchmarkCache<MruCache<int, int>>(accesses, size) * 100.0);
  }
  results.push_back(mru);

  // Print table
  if (show_table) {
    std::print("{:>8}", "Size %");
    for (const auto& r : results) {
      std::print("{:>9}", r.name);
    }
    std::println("");

    for (std::size_t i = 0; i < cache_percentages.size(); ++i) {
      std::print("{:>8.0f}", cache_percentages[i]);
      for (const auto& r : results) {
        std::print("{:>9.1f}", r.hit_rates[i]);
      }
      std::println("");
    }
  }

  // Build series for plotting using linear interpolation
  if (show_plots) {
    std::vector<Series> series;
    for (const auto& r : results) {
      Series s;
      s.label = r.name;
      s.color = r.color;
      s.fn = [&cache_percentages, hit_rates = r.hit_rates](double x) -> double {
        if (x <= cache_percentages.front()) return hit_rates.front();
        if (x >= cache_percentages.back()) return hit_rates.back();

        for (std::size_t i = 0; i < cache_percentages.size() - 1; ++i) {
          if (x >= cache_percentages[i] && x <= cache_percentages[i + 1]) {
            double t = (x - cache_percentages[i]) /
                       (cache_percentages[i + 1] - cache_percentages[i]);
            return hit_rates[i] + t * (hit_rates[i + 1] - hit_rates[i]);
          }
        }
        return hit_rates.back();
      };
      series.push_back(std::move(s));
    }

    std::println("");
    std::print("{}", linesPlot(series, 1.0, 50.0,
                               {.width = 70,
                                .height = 18,
                                .title = std::format("Hit Rate % (α={:.1f})", zipf_alpha),
                                .show_axes = false}));
  }
}

auto main() -> int {
  constexpr std::size_t kNumKeys = 1000;
  constexpr std::size_t kNumAccesses = 50000;
  constexpr std::uint32_t kSeed = 42;

  std::println("╔═══════════════════════════════════════════════════════════════╗");
  std::println("║           CACHE HIT RATE BENCHMARK                            ║");
  std::println("║  Keys: {:>5}  |  Accesses: {:>6}  |  Seed: {:>5}             ║",
               kNumKeys, kNumAccesses, kSeed);
  std::println("╚═══════════════════════════════════════════════════════════════╝");

  // Test different Zipf alpha values
  // α → 0: uniform distribution (all keys equally likely)
  // α = 1: classic Zipf (web traffic, word frequencies)
  // α > 1: increasingly skewed (few keys dominate)
  std::vector<double> alphas = {0.25, 0.75, 1.0, 1.5, 2.0};

  for (double alpha : alphas) {
    runBenchmark(alpha, kNumKeys, kNumAccesses, kSeed);
  }

  std::println("\n═══════════════════════════════════════════════════════════════════");
  std::println("OBSERVATIONS:");
  std::println("─────────────────────────────────────────────────────────────────");
  std::println("• α ≈ 0.25: Uniform access → all caches similar, ~cache_size% hit rate");
  std::println("• α ≈ 1.0:  Web workload → LFU shines (exploits frequency patterns)");
  std::println("• α ≈ 2.0:  Extreme skew → even small caches get high hit rates");
  std::println("• MRU is consistently worst (evicts what you just used!)");
  std::println("• LRU-2 helps with scan resistance at higher α values");
  std::println("═══════════════════════════════════════════════════════════════════\n");

  return 0;
}
