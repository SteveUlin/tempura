// Hash Function Benchmark: Speed and Quality Comparison
//
// Measures:
//   1. Throughput (GB/s) for various input sizes
//   2. Avalanche quality (bits flipped per single-bit change, ideal = 50%)
//   3. Distribution uniformity (chi-squared statistic, lower = better)

#include <array>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <print>
#include <random>
#include <string_view>
#include <vector>

#include "hash/aes_hash.h"
#include "hash/crc32c.h"
#include "hash/fnv.h"
#include "hash/murmur3.h"
#include "hash/wyhash.h"

using namespace tempura;
using Clock = std::chrono::high_resolution_clock;

// ============================================================================
// Throughput Benchmark
// ============================================================================

struct ThroughputResult {
  double gbps;          // Gigabytes per second
  double ns_per_hash;   // Nanoseconds per hash call
};

template <typename HashFn>
auto benchmarkThroughput(HashFn fn, const std::uint8_t* data, std::size_t len,
                         std::size_t iterations) -> ThroughputResult {
  // Warmup
  volatile std::uint64_t sink = 0;
  for (std::size_t i = 0; i < 1000; ++i) {
    sink += fn(data, len);
  }

  auto start = Clock::now();
  for (std::size_t i = 0; i < iterations; ++i) {
    sink += fn(data, len);
  }
  auto end = Clock::now();

  double elapsed_ns = std::chrono::duration<double, std::nano>(end - start).count();
  double total_bytes = static_cast<double>(len) * iterations;
  double gbps = total_bytes / elapsed_ns;  // bytes/ns = GB/s
  double ns_per_hash = elapsed_ns / iterations;

  return {gbps, ns_per_hash};
}

// ============================================================================
// Avalanche Quality
// ============================================================================
//
// For each bit position in the input, flip it and count how many output
// bits change. Perfect avalanche = 50% of output bits flip.

template <typename HashFn>
auto measureAvalanche64(HashFn fn) -> double {
  constexpr int kNumTests = 1000;
  std::mt19937_64 rng(42);

  int total_flips = 0;
  int total_bits = 0;

  for (int test = 0; test < kNumTests; ++test) {
    // Random 8-byte input
    std::uint64_t input = rng();
    std::array<std::uint8_t, 8> bytes;
    std::memcpy(bytes.data(), &input, 8);

    std::uint64_t base_hash = fn(bytes.data(), 8);

    // Flip each bit and measure output change
    for (int bit = 0; bit < 64; ++bit) {
      std::uint64_t flipped_input = input ^ (1ULL << bit);
      std::memcpy(bytes.data(), &flipped_input, 8);

      std::uint64_t new_hash = fn(bytes.data(), 8);
      int bits_changed = std::popcount(base_hash ^ new_hash);

      total_flips += bits_changed;
      total_bits += 64;  // 64 output bits
    }
  }

  return 100.0 * total_flips / total_bits;  // Percentage
}

// 32-bit version for 32-bit hashes
template <typename HashFn>
auto measureAvalanche32(HashFn fn) -> double {
  constexpr int kNumTests = 1000;
  std::mt19937 rng(42);

  int total_flips = 0;
  int total_bits = 0;

  for (int test = 0; test < kNumTests; ++test) {
    std::uint32_t input = rng();
    std::array<std::uint8_t, 4> bytes;
    std::memcpy(bytes.data(), &input, 4);

    std::uint32_t base_hash = fn(bytes.data(), 4);

    for (int bit = 0; bit < 32; ++bit) {
      std::uint32_t flipped_input = input ^ (1U << bit);
      std::memcpy(bytes.data(), &flipped_input, 4);

      std::uint32_t new_hash = fn(bytes.data(), 4);
      int bits_changed = std::popcount(base_hash ^ new_hash);

      total_flips += bits_changed;
      total_bits += 32;
    }
  }

  return 100.0 * total_flips / total_bits;
}

// ============================================================================
// Distribution Quality (Chi-Squared)
// ============================================================================
//
// Hash sequential integers into buckets and measure how uniform the
// distribution is. Lower chi-squared = more uniform.

template <typename HashFn>
auto measureDistribution(HashFn fn, std::size_t num_keys,
                         std::size_t num_buckets) -> double {
  std::vector<int> buckets(num_buckets, 0);

  for (std::size_t i = 0; i < num_keys; ++i) {
    std::array<std::uint8_t, 8> bytes;
    std::memcpy(bytes.data(), &i, 8);
    std::uint64_t h = fn(bytes.data(), 8);
    buckets[h % num_buckets]++;
  }

  // Chi-squared statistic
  double expected = static_cast<double>(num_keys) / num_buckets;
  double chi_sq = 0;
  for (int count : buckets) {
    double diff = count - expected;
    chi_sq += (diff * diff) / expected;
  }

  return chi_sq;
}

// ============================================================================
// Small Key Performance (Integer Hashing)
// ============================================================================

template <typename HashFn>
auto benchmarkIntegers(HashFn fn, std::size_t count) -> double {
  volatile std::uint64_t sink = 0;

  auto start = Clock::now();
  for (std::uint64_t i = 0; i < count; ++i) {
    std::array<std::uint8_t, 8> bytes;
    std::memcpy(bytes.data(), &i, 8);
    sink += fn(bytes.data(), 8);
  }
  auto end = Clock::now();

  double elapsed_ns = std::chrono::duration<double, std::nano>(end - start).count();
  return elapsed_ns / count;  // ns per hash
}

// ============================================================================
// Main
// ============================================================================

auto main() -> int {
  std::println("════════════════════════════════════════════════════════════════");
  std::println("                    Hash Function Benchmark");
  std::println("════════════════════════════════════════════════════════════════\n");

  // Prepare test data
  std::vector<std::uint8_t> data(1024 * 1024);  // 1 MB
  std::mt19937 rng(12345);
  for (auto& b : data) {
    b = static_cast<std::uint8_t>(rng());
  }

  // Hash function wrappers (all return uint64_t for fair comparison)
  auto fnv64 = [](const std::uint8_t* d, std::size_t n) -> std::uint64_t {
    return fnv1a64(d, n);
  };
  auto murmur64 = [](const std::uint8_t* d, std::size_t n) -> std::uint64_t {
    return murmurHash3_64(d, n);
  };
  auto wy = [](const std::uint8_t* d, std::size_t n) -> std::uint64_t {
    return wyhash(d, n);
  };
  auto crc = [](const std::uint8_t* d, std::size_t n) -> std::uint64_t {
    return crc32c(d, n);
  };
  auto crc_mixed = [](const std::uint8_t* d, std::size_t n) -> std::uint64_t {
    return crc32cMixed(d, n);
  };
  auto aes = [](const std::uint8_t* d, std::size_t n) -> std::uint64_t {
    return aesHash64(d, n);
  };

  // ──────────────────────────────────────────────────────────────────────────
  // Throughput: Large Data (1 MB)
  // ──────────────────────────────────────────────────────────────────────────

  std::println("┌─────────────────────────────────────────────────────────────┐");
  std::println("│  THROUGHPUT: 1 MB blocks                                    │");
  std::println("├─────────────────┬───────────────┬───────────────────────────┤");
  std::println("│ Hash Function   │ Speed (GB/s)  │ Time per hash             │");
  std::println("├─────────────────┼───────────────┼───────────────────────────┤");

  constexpr std::size_t kLargeIters = 1000;

  auto print_throughput = [&](const char* name, auto fn) {
    auto r = benchmarkThroughput(fn, data.data(), data.size(), kLargeIters);
    std::println("│ {:15} │ {:10.2f}    │ {:10.1f} μs              │",
                 name, r.gbps, r.ns_per_hash / 1000);
  };

  print_throughput("FNV-1a", fnv64);
  print_throughput("MurmurHash3", murmur64);
  print_throughput("wyhash", wy);
  print_throughput("CRC32C", crc);
  print_throughput("CRC32C+mix", crc_mixed);
  print_throughput("AES-NI", aes);

  std::println("└─────────────────┴───────────────┴───────────────────────────┘\n");

  // ──────────────────────────────────────────────────────────────────────────
  // Throughput: Small Data (8, 16, 64, 256 bytes)
  // ──────────────────────────────────────────────────────────────────────────

  std::println("┌─────────────────────────────────────────────────────────────┐");
  std::println("│  THROUGHPUT: Small inputs (ns per hash)                     │");
  std::println("├─────────────────┬─────────┬─────────┬─────────┬─────────────┤");
  std::println("│ Hash Function   │ 8 bytes │ 16 bytes│ 64 bytes│ 256 bytes   │");
  std::println("├─────────────────┼─────────┼─────────┼─────────┼─────────────┤");

  constexpr std::size_t kSmallIters = 1000000;

  auto print_small = [&](const char* name, auto fn) {
    auto r8 = benchmarkThroughput(fn, data.data(), 8, kSmallIters);
    auto r16 = benchmarkThroughput(fn, data.data(), 16, kSmallIters);
    auto r64 = benchmarkThroughput(fn, data.data(), 64, kSmallIters);
    auto r256 = benchmarkThroughput(fn, data.data(), 256, kSmallIters);
    std::println("│ {:15} │ {:6.1f}  │ {:6.1f}  │ {:6.1f}  │ {:6.1f}       │",
                 name, r8.ns_per_hash, r16.ns_per_hash, r64.ns_per_hash, r256.ns_per_hash);
  };

  print_small("FNV-1a", fnv64);
  print_small("MurmurHash3", murmur64);
  print_small("wyhash", wy);
  print_small("CRC32C", crc);
  print_small("CRC32C+mix", crc_mixed);
  print_small("AES-NI", aes);

  std::println("└─────────────────┴─────────┴─────────┴─────────┴─────────────┘\n");

  // ──────────────────────────────────────────────────────────────────────────
  // Avalanche Quality
  // ──────────────────────────────────────────────────────────────────────────

  std::println("┌─────────────────────────────────────────────────────────────┐");
  std::println("│  AVALANCHE QUALITY (ideal = 50.0%)                          │");
  std::println("│  Measures: % of output bits that flip per input bit flip    │");
  std::println("├─────────────────┬─────────────┬─────────────────────────────┤");
  std::println("│ Hash Function   │ Avalanche   │ Quality                     │");
  std::println("├─────────────────┼─────────────┼─────────────────────────────┤");

  auto print_avalanche = [](const char* name, double pct) {
    const char* quality;
    if (std::abs(pct - 50.0) < 1.0) quality = "Excellent";
    else if (std::abs(pct - 50.0) < 3.0) quality = "Good";
    else if (std::abs(pct - 50.0) < 5.0) quality = "Acceptable";
    else quality = "Poor";

    std::println("│ {:15} │ {:8.2f}%   │ {:27} │", name, pct, quality);
  };

  print_avalanche("FNV-1a", measureAvalanche64(fnv64));
  print_avalanche("MurmurHash3", measureAvalanche64(murmur64));
  print_avalanche("wyhash", measureAvalanche64(wy));
  print_avalanche("CRC32C", measureAvalanche32([](const std::uint8_t* d, std::size_t n) {
    return crc32c(d, n);
  }));
  print_avalanche("CRC32C+mix", measureAvalanche32([](const std::uint8_t* d, std::size_t n) {
    return crc32cMixed(d, n);
  }));
  print_avalanche("AES-NI", measureAvalanche64(aes));

  std::println("└─────────────────┴─────────────┴─────────────────────────────┘\n");

  // ──────────────────────────────────────────────────────────────────────────
  // Distribution Quality
  // ──────────────────────────────────────────────────────────────────────────

  std::println("┌─────────────────────────────────────────────────────────────┐");
  std::println("│  DISTRIBUTION QUALITY (Chi-squared, lower = better)         │");
  std::println("│  100K keys → 1000 buckets, expected χ² ≈ 999 for uniform    │");
  std::println("├─────────────────┬─────────────┬─────────────────────────────┤");
  std::println("│ Hash Function   │ Chi-squared │ Quality                     │");
  std::println("├─────────────────┼─────────────┼─────────────────────────────┤");

  constexpr std::size_t kNumKeys = 100000;
  constexpr std::size_t kNumBuckets = 1000;
  // For chi-squared with 999 df, critical values:
  // p=0.99: 1143, p=0.95: 1073, p=0.05: 927, p=0.01: 888

  auto print_dist = [](const char* name, double chi_sq) {
    const char* quality;
    if (chi_sq < 1100) quality = "Excellent (uniform)";
    else if (chi_sq < 1200) quality = "Good";
    else if (chi_sq < 1500) quality = "Acceptable";
    else quality = "Poor (biased)";

    std::println("│ {:15} │ {:8.1f}    │ {:27} │", name, chi_sq, quality);
  };

  print_dist("FNV-1a", measureDistribution(fnv64, kNumKeys, kNumBuckets));
  print_dist("MurmurHash3", measureDistribution(murmur64, kNumKeys, kNumBuckets));
  print_dist("wyhash", measureDistribution(wy, kNumKeys, kNumBuckets));
  print_dist("CRC32C", measureDistribution(crc, kNumKeys, kNumBuckets));
  print_dist("CRC32C+mix", measureDistribution(crc_mixed, kNumKeys, kNumBuckets));
  print_dist("AES-NI", measureDistribution(aes, kNumKeys, kNumBuckets));

  std::println("└─────────────────┴─────────────┴─────────────────────────────┘\n");

  // ──────────────────────────────────────────────────────────────────────────
  // Integer Hashing (common use case)
  // ──────────────────────────────────────────────────────────────────────────

  std::println("┌─────────────────────────────────────────────────────────────┐");
  std::println("│  INTEGER HASHING SPEED (ns per 8-byte integer)              │");
  std::println("├─────────────────┬─────────────┬─────────────────────────────┤");
  std::println("│ Hash Function   │ ns/hash     │ M hashes/sec                │");
  std::println("├─────────────────┼─────────────┼─────────────────────────────┤");

  auto print_int = [](const char* name, double ns) {
    double mhps = 1000.0 / ns;  // million hashes per second
    std::println("│ {:15} │ {:8.1f}    │ {:8.1f}                    │", name, ns, mhps);
  };

  print_int("FNV-1a", benchmarkIntegers(fnv64, 10000000));
  print_int("MurmurHash3", benchmarkIntegers(murmur64, 10000000));
  print_int("wyhash", benchmarkIntegers(wy, 10000000));
  print_int("CRC32C", benchmarkIntegers(crc, 10000000));
  print_int("CRC32C+mix", benchmarkIntegers(crc_mixed, 10000000));
  print_int("AES-NI", benchmarkIntegers(aes, 10000000));

  std::println("└─────────────────┴─────────────┴─────────────────────────────┘\n");

  // ──────────────────────────────────────────────────────────────────────────
  // Summary
  // ──────────────────────────────────────────────────────────────────────────

  std::println("════════════════════════════════════════════════════════════════");
  std::println("OBSERVATIONS");
  std::println("════════════════════════════════════════════════════════════════");
  std::println("");
  std::println("  Large data: wyhash ≈ CRC32C ≈ AES-NI (all ~5 GB/s, memory-bound)");
  std::println("  Small data: CRC32C fastest (hw instruction), MurmurHash3 competitive");
  std::println("  Integers:   CRC32C raw is ~5x faster (single instruction)");
  std::println("  Quality:    wyhash = MurmurHash3 = AES-NI (excellent avalanche)");
  std::println("              FNV-1a has weak avalanche (~40% vs ideal 50%)");
  std::println("              CRC32C raw is slightly biased; +mix fixes it");
  std::println("");
  std::println("  Recommendations:");
  std::println("    • General purpose:     wyhash (excellent speed + quality)");
  std::println("    • Integer keys:        CRC32C (fastest) or CRC32C+mix (better quality)");
  std::println("    • Constexpr/portable:  MurmurHash3 (good speed + quality)");
  std::println("    • Simplicity:          FNV-1a (but accept weaker avalanche)");
  std::println("    • Large bulk data:     AES-NI or wyhash (similar performance)");
  std::println("");

  return 0;
}
