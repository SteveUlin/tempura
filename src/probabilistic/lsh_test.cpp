#include "probabilistic/lsh.h"

#include <algorithm>
#include <random>
#include <set>
#include <vector>

#include "unit.h"

using namespace tempura;

auto main() -> int {
  "identical signatures are candidates"_test = [] {
    LSHIndex<int, 10, 5> index;  // 10 bands, 5 rows = 50 hash signature

    LSHIndex<int, 10, 5>::Signature sig;
    for (std::size_t i = 0; i < sig.size(); ++i) {
      sig[i] = i * 42;
    }

    index.insert(1, sig);
    index.insert(2, sig);  // Same signature as item 1

    auto candidates = index.queryUnique(sig);

    // Both items should be candidates (they match in ALL bands)
    expectTrue(std::find(candidates.begin(), candidates.end(), 1) !=
               candidates.end());
    expectTrue(std::find(candidates.begin(), candidates.end(), 2) !=
               candidates.end());
  };

  "completely different signatures are not candidates"_test = [] {
    LSHIndex<int, 10, 5> index;  // 10 bands, 5 rows = 50 hash signature

    LSHIndex<int, 10, 5>::Signature sig1;
    LSHIndex<int, 10, 5>::Signature sig2;

    // Create maximally different signatures
    for (std::size_t i = 0; i < sig1.size(); ++i) {
      sig1[i] = i;
      sig2[i] = i + 1000000;  // Completely different values
    }

    index.insert(1, sig1);

    auto candidates = index.queryUnique(sig2);

    // Item 1 should NOT be a candidate for sig2 (no bands match)
    expectTrue(std::find(candidates.begin(), candidates.end(), 1) ==
               candidates.end());
  };

  "high similarity signatures are likely candidates"_test = [] {
    // Use more bands for higher recall at high similarity
    LSHIndex<int, 20, 5> index;  // 20 bands, 5 rows = 100 hash signature
    constexpr std::size_t kSigSize = 100;

    std::mt19937 rng{42};
    std::uniform_int_distribution<std::size_t> dist{0, 1000};

    // Create a base signature
    std::array<std::size_t, kSigSize> base_sig;
    for (std::size_t i = 0; i < kSigSize; ++i) {
      base_sig[i] = dist(rng);
    }

    // Create a very similar signature (~90% matching)
    std::array<std::size_t, kSigSize> similar_sig = base_sig;
    for (std::size_t i = 0; i < 10; ++i) {  // Change 10%
      similar_sig[i] = dist(rng) + 2000;
    }

    index.insert(1, base_sig);

    auto candidates = index.queryUnique(similar_sig);

    // With 90% similarity and 20 bands of 5 rows:
    // P(candidate) = 1 - (1 - 0.9^5)^20 ≈ 1 - (1 - 0.59)^20 ≈ 1.0
    // Should almost always be a candidate
    expectTrue(std::find(candidates.begin(), candidates.end(), 1) !=
               candidates.end());
  };

  "query returns correct candidate set"_test = [] {
    LSHIndex<int, 5, 4> index;  // 5 bands, 4 rows = 20 hash signature

    LSHIndex<int, 5, 4>::Signature query_sig;
    for (std::size_t i = 0; i < query_sig.size(); ++i) {
      query_sig[i] = i;
    }

    // Insert the query signature itself
    index.insert(100, query_sig);

    // Insert a signature that matches in first band only
    LSHIndex<int, 5, 4>::Signature partial_match = query_sig;
    for (std::size_t i = 4; i < partial_match.size(); ++i) {
      partial_match[i] = i + 5000;  // Change all bands except first
    }
    index.insert(200, partial_match);

    // Insert a completely different signature
    LSHIndex<int, 5, 4>::Signature different;
    for (std::size_t i = 0; i < different.size(); ++i) {
      different[i] = i + 10000;
    }
    index.insert(300, different);

    auto candidates = index.queryUnique(query_sig);

    // Should find item 100 (exact match) and 200 (first band matches)
    expectTrue(std::find(candidates.begin(), candidates.end(), 100) !=
               candidates.end());
    expectTrue(std::find(candidates.begin(), candidates.end(), 200) !=
               candidates.end());
    // Should NOT find item 300 (no bands match)
    expectTrue(std::find(candidates.begin(), candidates.end(), 300) ==
               candidates.end());
  };

  "clear removes all items"_test = [] {
    LSHIndex<int, 4, 3> index;

    LSHIndex<int, 4, 3>::Signature sig;
    for (std::size_t i = 0; i < sig.size(); ++i) {
      sig[i] = i;
    }

    index.insert(1, sig);
    index.insert(2, sig);

    auto before_clear = index.queryUnique(sig);
    expectEq(before_clear.size(), 2uz);

    index.clear();

    auto after_clear = index.queryUnique(sig);
    expectEq(after_clear.size(), 0uz);
    expectEq(index.totalBuckets(), 0uz);
  };

  "query returns duplicates across bands"_test = [] {
    LSHIndex<int, 4, 2> index;  // 4 bands, 2 rows = 8 hash signature

    LSHIndex<int, 4, 2>::Signature sig;
    for (std::size_t i = 0; i < sig.size(); ++i) {
      sig[i] = 42;  // All same value
    }

    index.insert(1, sig);

    // Non-unique query should return duplicates (one per matching band)
    auto candidates = index.query(sig);

    // Should have 4 entries (one per band)
    expectEq(candidates.size(), 4uz);

    // All should be item 1
    for (const auto& id : candidates) {
      expectEq(id, 1);
    }

    // Unique query should deduplicate
    auto unique_candidates = index.queryUnique(sig);
    expectEq(unique_candidates.size(), 1uz);
  };

  "candidate probability formula"_test = [] {
    // Test the theoretical probability calculation
    // For b=10, r=5: P(candidate | s) = 1 - (1 - s^5)^10

    // At s=0 (no similarity), probability should be 0
    expectNear(LSHIndex<int, 10, 5>::candidateProbability(0.0), 0.0, 1e-10);

    // At s=1 (identical), probability should be 1
    expectNear(LSHIndex<int, 10, 5>::candidateProbability(1.0), 1.0, 1e-10);

    // At threshold t ≈ (1/10)^(1/5) ≈ 0.631, probability should be ~0.5
    // The actual formula gives P ≈ 0.65 at this threshold (due to discrete bands)
    double threshold = LSHIndex<int, 10, 5>::threshold();
    double p_at_threshold = LSHIndex<int, 10, 5>::candidateProbability(threshold);
    // Threshold approximation has some error; verify it's in reasonable range
    expectGT(p_at_threshold, 0.4);
    expectLT(p_at_threshold, 0.7);

    // Low similarity (s=0.2): probability should be very low
    double p_low = LSHIndex<int, 10, 5>::candidateProbability(0.2);
    // 1 - (1 - 0.2^5)^10 = 1 - (1 - 0.00032)^10 ≈ 0.0032
    expectLT(p_low, 0.01);

    // High similarity (s=0.9): probability should be very high
    double p_high = LSHIndex<int, 10, 5>::candidateProbability(0.9);
    // 1 - (1 - 0.9^5)^10 = 1 - (1 - 0.59)^10 ≈ 0.9999
    expectGT(p_high, 0.99);
  };

  "different band configurations"_test = [] {
    // Test that different configurations work correctly

    // Many bands, few rows: lower threshold, more false positives
    {
      LSHIndex<int, 20, 2> index;  // t ≈ 0.22
      LSHIndex<int, 20, 2>::Signature sig;
      for (std::size_t i = 0; i < sig.size(); ++i) {
        sig[i] = i;
      }
      index.insert(1, sig);
      auto candidates = index.queryUnique(sig);
      expectEq(candidates.size(), 1uz);
    }

    // Few bands, many rows: higher threshold, fewer false positives
    {
      LSHIndex<int, 2, 20> index;  // t ≈ 0.71
      LSHIndex<int, 2, 20>::Signature sig;
      for (std::size_t i = 0; i < sig.size(); ++i) {
        sig[i] = i;
      }
      index.insert(1, sig);
      auto candidates = index.queryUnique(sig);
      expectEq(candidates.size(), 1uz);
    }
  };

  "string ids"_test = [] {
    LSHIndex<std::string, 5, 3> index;

    LSHIndex<std::string, 5, 3>::Signature sig;
    for (std::size_t i = 0; i < sig.size(); ++i) {
      sig[i] = i * 7;
    }

    index.insert("document_a", sig);
    index.insert("document_b", sig);

    auto candidates = index.queryUnique(sig);
    expectEq(candidates.size(), 2uz);

    std::set<std::string> candidate_set(candidates.begin(), candidates.end());
    expectTrue(candidate_set.count("document_a") == 1);
    expectTrue(candidate_set.count("document_b") == 1);
  };

  "span interface"_test = [] {
    LSHIndex<int, 4, 4> index;

    std::vector<std::size_t> sig_vec(16);
    for (std::size_t i = 0; i < sig_vec.size(); ++i) {
      sig_vec[i] = i * 11;
    }

    // Insert using span
    index.insert(1, std::span<const std::size_t>{sig_vec});

    // Query using span
    auto candidates = index.queryUnique(std::span<const std::size_t>{sig_vec});
    expectEq(candidates.size(), 1uz);
    expectEq(candidates[0], 1);
  };

  return TestRegistry::result();
}
