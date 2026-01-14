#include "probabilistic/xor_filter.h"

#include <random>
#include <set>
#include <vector>

#include "probabilistic/bloom_filter.h"
#include "unit.h"

using namespace tempura;

auto main() -> int {
  "empty filter returns false for contains"_test = [] {
    XorFilter<8> filter;
    expectFalse(filter.contains(42));
    expectFalse(filter.contains(0));
    expectFalse(filter.contains(12345));
    expectEq(filter.size(), 0uz);
    expectFalse(filter.isConstructed());
  };

  "construction from vector"_test = [] {
    std::vector<std::uint64_t> items = {10, 20, 30, 40, 50};
    XorFilter<8> filter{items};

    expectTrue(filter.isConstructed());
    expectEq(filter.size(), 5uz);

    // All inserted items must be found (no false negatives)
    for (auto item : items) {
      expectTrue(filter.contains(item));
    }
  };

  "all items found after construction"_test = [] {
    // Test with a larger set
    std::vector<std::uint64_t> items;
    for (std::uint64_t i = 0; i < 1000; ++i) {
      items.push_back(i * 17 + 31);  // Arbitrary transformation
    }

    XorFilter<8> filter{items};
    expectTrue(filter.isConstructed());
    expectEq(filter.size(), 1000uz);

    // No false negatives: all inserted items must be found
    for (auto item : items) {
      expectTrue(filter.contains(item));
    }
  };

  "false positive rate approximately matches theory"_test = [] {
    // For 8-bit fingerprints, theoretical FP rate is ~1/256 ≈ 0.39%
    // We test with 10k non-members and expect FP rate < 2% (generous tolerance)

    std::vector<std::uint64_t> items;
    for (std::uint64_t i = 0; i < 1000; ++i) {
      items.push_back(i);
    }

    XorFilter<8> filter{items};
    expectTrue(filter.isConstructed());

    // Count false positives among items NOT in the set
    std::mt19937_64 rng{42};
    std::uniform_int_distribution<std::uint64_t> dist{100000, 1000000};

    int false_positives = 0;
    constexpr int kNumTests = 10000;
    for (int i = 0; i < kNumTests; ++i) {
      std::uint64_t val = dist(rng);
      if (filter.contains(val)) {
        ++false_positives;
      }
    }

    double fp_rate = static_cast<double>(false_positives) / kNumTests;

    // Theoretical rate is ~0.39%, allow up to 2% for statistical variance
    // With 10k samples and true rate 0.4%, std error ≈ 0.006
    // 2% threshold is ~25 standard errors away - effectively never fails
    expectLT(fp_rate, 0.02);

    // Also verify it's not absurdly low (filter is working)
    // With 10k tests at 0.4% rate, we expect ~40 FPs
    // Less than 5 would be very suspicious (p < 0.0001)
    expectGT(false_positives, 5);
  };

  "16-bit fingerprints have lower FP rate"_test = [] {
    // 16-bit fingerprints should have FP rate ~1/65536 ≈ 0.0015%

    std::vector<std::uint64_t> items;
    for (std::uint64_t i = 0; i < 1000; ++i) {
      items.push_back(i);
    }

    XorFilter<16> filter{items};
    expectTrue(filter.isConstructed());

    // Count false positives
    std::mt19937_64 rng{123};
    std::uniform_int_distribution<std::uint64_t> dist{100000, 1000000};

    int false_positives = 0;
    constexpr int kNumTests = 10000;
    for (int i = 0; i < kNumTests; ++i) {
      std::uint64_t val = dist(rng);
      if (filter.contains(val)) {
        ++false_positives;
      }
    }

    double fp_rate = static_cast<double>(false_positives) / kNumTests;

    // With 16-bit fingerprints, theoretical rate is ~0.0015%
    // In 10k tests, we expect ~0.15 FPs on average
    // Allow up to 0.5% for statistical variance
    expectLT(fp_rate, 0.005);
  };

  "size in bytes is reasonable"_test = [] {
    std::vector<std::uint64_t> items;
    for (std::uint64_t i = 0; i < 1000; ++i) {
      items.push_back(i);
    }

    XorFilter<8> filter8{items};
    XorFilter<16> filter16{items};

    // 8-bit filter: ~1.23 * 1000 * 1 byte = ~1230 bytes + overhead
    expectGT(filter8.sizeInBytes(), 1000uz);
    expectLT(filter8.sizeInBytes(), 2000uz);

    // 16-bit filter: ~1.23 * 1000 * 2 bytes = ~2460 bytes + overhead
    expectGT(filter16.sizeInBytes(), 2000uz);
    expectLT(filter16.sizeInBytes(), 4000uz);

    // 16-bit should be roughly twice the size of 8-bit
    expectGT(filter16.sizeInBytes(), filter8.sizeInBytes());
  };

  "bits per element is approximately 1.23 * fingerprint_bits"_test = [] {
    std::vector<std::uint64_t> items;
    for (std::uint64_t i = 0; i < 10000; ++i) {
      items.push_back(i);
    }

    XorFilter<8> filter8{items};
    XorFilter<16> filter16{items};

    // Expected: ~1.23 * 8 ≈ 9.8 bits/element for 8-bit
    // Expected: ~1.23 * 16 ≈ 19.7 bits/element for 16-bit
    // Allow some slack for the +32 padding in array size

    double bpe8 = filter8.bitsPerElement();
    double bpe16 = filter16.bitsPerElement();

    expectGT(bpe8, 9.0);
    expectLT(bpe8, 11.0);

    expectGT(bpe16, 18.0);
    expectLT(bpe16, 22.0);
  };

  "comparison with bloom filter size"_test = [] {
    // XOR filter should be more space-efficient than Bloom for same FP rate

    // 1000 items, ~1% false positive rate
    // Bloom filter: optimal is m/n * ln(2) ≈ 9.6 bits/element for k=7
    // We use 1024 * 10 = 10240 bits = 10.24 bits/element

    // XOR filter with 8-bit fingerprints: ~9.8 bits/element, FP rate ~0.39%
    // (better FP rate at similar space!)

    std::vector<std::uint64_t> items;
    for (std::uint64_t i = 0; i < 1000; ++i) {
      items.push_back(i);
    }

    XorFilter<8> xor_filter{items};

    // XOR filter uses ~1.23 * 1000 * 8 bits ≈ 9840 bits
    // Bloom filter with 10 bits/element uses 10000 bits
    // XOR should be competitive or better

    double xor_bits = xor_filter.bitsPerElement();

    // XOR filter should use less than 11 bits per element
    expectLT(xor_bits, 11.0);

    // And have better FP rate than Bloom at similar size
    // Bloom with k=7, m=10000, n=1000 has FP ≈ 0.82%
    // XOR with 8-bit fingerprints has FP ≈ 0.39%
    double xor_fp = XorFilter<8>::falsePositiveRate();
    double bloom_fp = BloomFilter<10000, 7>::estimateFalsePositiveRate(1000);

    expectLT(xor_fp, bloom_fp);
  };

  "construction from range view"_test = [] {
    // Test that we can construct from arbitrary ranges, not just vectors
    auto items = std::views::iota(0ULL, 100ULL);

    XorFilter<8> filter{items};
    expectTrue(filter.isConstructed());
    expectEq(filter.size(), 100uz);

    for (std::uint64_t i = 0; i < 100; ++i) {
      expectTrue(filter.contains(i));
    }
  };

  "empty range creates empty filter"_test = [] {
    std::vector<std::uint64_t> empty;
    XorFilter<8> filter{empty};

    // Empty filter should be "constructed" but contain nothing
    expectEq(filter.size(), 0uz);
    expectFalse(filter.contains(0));
    expectFalse(filter.contains(42));
  };

  "single item filter"_test = [] {
    std::vector<std::uint64_t> items = {42};
    XorFilter<8> filter{items};

    expectTrue(filter.isConstructed());
    expectEq(filter.size(), 1uz);
    expectTrue(filter.contains(42));
  };

  "duplicate items handled correctly"_test = [] {
    // Duplicates are automatically deduplicated
    std::vector<std::uint64_t> items = {1, 2, 3, 1, 2, 3, 1, 2, 3};
    XorFilter<8> filter{items};

    // Filter should be constructed with unique items
    expectTrue(filter.isConstructed());
    expectEq(filter.size(), 3uz);  // Only 3 unique items
    expectTrue(filter.contains(1));
    expectTrue(filter.contains(2));
    expectTrue(filter.contains(3));
  };

  "static properties"_test = [] {
    // 8-bit fingerprints: FP rate = 1/256 ≈ 0.0039
    double fp8 = XorFilter<8>::falsePositiveRate();
    expectNear(fp8, 1.0 / 256.0, 0.0001);

    // 16-bit fingerprints: FP rate = 1/65536 ≈ 0.000015
    double fp16 = XorFilter<16>::falsePositiveRate();
    expectNear(fp16, 1.0 / 65536.0, 0.000001);
  };

  "large set construction"_test = [] {
    // Test with 10k items to ensure peeling works at scale
    std::vector<std::uint64_t> items;
    items.reserve(10000);
    for (std::uint64_t i = 0; i < 10000; ++i) {
      items.push_back(i * 31337);  // Arbitrary multiplier
    }

    XorFilter<8> filter{items};
    expectTrue(filter.isConstructed());
    expectEq(filter.size(), 10000uz);

    // Spot check some items
    expectTrue(filter.contains(0));
    expectTrue(filter.contains(31337));
    expectTrue(filter.contains(9999 * 31337));
  };

  return TestRegistry::result();
}
