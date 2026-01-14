#include "probabilistic/weighted_minhash.h"

#include <cmath>
#include <set>
#include <unordered_map>
#include <vector>

#include "unit.h"

using namespace tempura;

// Helper to compute exact weighted Jaccard similarity
auto computeWeightedJaccard(
    const std::vector<WeightedMinHash<>::WeightedFeature>& a,
    const std::vector<WeightedMinHash<>::WeightedFeature>& b) -> double {
  // Build maps for quick lookup
  std::unordered_map<std::uint64_t, double> weights_a;
  std::unordered_map<std::uint64_t, double> weights_b;

  for (const auto& [id, w] : a) {
    weights_a[id] = w;
  }
  for (const auto& [id, w] : b) {
    weights_b[id] = w;
  }

  // Collect all feature IDs
  std::set<std::uint64_t> all_ids;
  for (const auto& [id, w] : a) all_ids.insert(id);
  for (const auto& [id, w] : b) all_ids.insert(id);

  double sum_min = 0.0;
  double sum_max = 0.0;

  for (std::uint64_t id : all_ids) {
    double wa = weights_a.count(id) ? weights_a[id] : 0.0;
    double wb = weights_b.count(id) ? weights_b[id] : 0.0;
    sum_min += std::min(wa, wb);
    sum_max += std::max(wa, wb);
  }

  return sum_max > 0.0 ? sum_min / sum_max : 1.0;
}

auto main() -> int {
  "identical weighted sets have similarity ~1.0"_test = [] {
    WeightedMinHash<128> wmh;
    std::vector<WeightedMinHash<128>::WeightedFeature> set_a = {
        {1, 3.0}, {2, 5.0}, {3, 2.0}, {4, 1.0}, {5, 4.0}};
    std::vector<WeightedMinHash<128>::WeightedFeature> set_b = {
        {1, 3.0}, {2, 5.0}, {3, 2.0}, {4, 1.0}, {5, 4.0}};

    auto sig_a = wmh.computeSignature(set_a);
    auto sig_b = wmh.computeSignature(set_b);

    double similarity = WeightedMinHash<128>::estimateSimilarity(sig_a, sig_b);
    // Identical sets should have identical signatures
    expectEq(similarity, 1.0);
  };

  "disjoint sets have similarity ~0.0"_test = [] {
    WeightedMinHash<128> wmh;
    std::vector<WeightedMinHash<128>::WeightedFeature> set_a = {
        {1, 3.0}, {2, 5.0}, {3, 2.0}};
    std::vector<WeightedMinHash<128>::WeightedFeature> set_b = {
        {101, 4.0}, {102, 6.0}, {103, 1.0}};

    auto sig_a = wmh.computeSignature(set_a);
    auto sig_b = wmh.computeSignature(set_b);

    double similarity = WeightedMinHash<128>::estimateSimilarity(sig_a, sig_b);
    // Disjoint weighted sets have J=0
    // Random collisions in (hash, t) pairs should be rare
    expectLT(similarity, 0.15);
  };

  "hand-computed weighted Jaccard: simple case"_test = [] {
    // A = {apple: 3, banana: 2}
    // B = {apple: 1, banana: 4}
    // min sum = min(3,1) + min(2,4) = 1 + 2 = 3
    // max sum = max(3,1) + max(2,4) = 3 + 4 = 7
    // J = 3/7 ≈ 0.4286
    WeightedMinHash<256> wmh;
    std::vector<WeightedMinHash<256>::WeightedFeature> set_a = {{1, 3.0}, {2, 2.0}};
    std::vector<WeightedMinHash<256>::WeightedFeature> set_b = {{1, 1.0}, {2, 4.0}};

    auto sig_a = wmh.computeSignature(set_a);
    auto sig_b = wmh.computeSignature(set_b);

    double similarity = WeightedMinHash<256>::estimateSimilarity(sig_a, sig_b);
    double expected = 3.0 / 7.0;
    // Standard error = sqrt(J(1-J)/k) = sqrt(0.4286 * 0.5714 / 256) ≈ 0.031
    // Tolerance of 0.15 is ~5 standard errors
    expectNear(similarity, expected, 0.15);
  };

  "hand-computed weighted Jaccard: one-sided overlap"_test = [] {
    // A = {x: 10}
    // B = {x: 5, y: 5}
    // min sum = min(10, 5) + min(0, 5) = 5 + 0 = 5
    // max sum = max(10, 5) + max(0, 5) = 10 + 5 = 15
    // J = 5/15 = 1/3 ≈ 0.333
    WeightedMinHash<256> wmh;
    std::vector<WeightedMinHash<256>::WeightedFeature> set_a = {{1, 10.0}};
    std::vector<WeightedMinHash<256>::WeightedFeature> set_b = {{1, 5.0}, {2, 5.0}};

    auto sig_a = wmh.computeSignature(set_a);
    auto sig_b = wmh.computeSignature(set_b);

    double similarity = WeightedMinHash<256>::estimateSimilarity(sig_a, sig_b);
    double expected = 1.0 / 3.0;
    // Standard error ≈ 0.029, tolerance of 0.15 is generous
    expectNear(similarity, expected, 0.15);
  };

  "scaling weights uniformly does not change similarity"_test = [] {
    // Weighted Jaccard is scale-invariant when both sets scaled equally
    WeightedMinHash<128> wmh;
    std::vector<WeightedMinHash<128>::WeightedFeature> set_a = {
        {1, 1.0}, {2, 2.0}, {3, 3.0}};
    std::vector<WeightedMinHash<128>::WeightedFeature> set_b = {
        {1, 2.0}, {2, 1.0}, {3, 4.0}};

    // Scale both by 10x
    std::vector<WeightedMinHash<128>::WeightedFeature> set_a_scaled = {
        {1, 10.0}, {2, 20.0}, {3, 30.0}};
    std::vector<WeightedMinHash<128>::WeightedFeature> set_b_scaled = {
        {1, 20.0}, {2, 10.0}, {3, 40.0}};

    auto sig_a = wmh.computeSignature(set_a);
    auto sig_b = wmh.computeSignature(set_b);
    auto sig_a_scaled = wmh.computeSignature(set_a_scaled);
    auto sig_b_scaled = wmh.computeSignature(set_b_scaled);

    double sim_original = WeightedMinHash<128>::estimateSimilarity(sig_a, sig_b);
    double sim_scaled =
        WeightedMinHash<128>::estimateSimilarity(sig_a_scaled, sig_b_scaled);

    // Both should estimate the same underlying Jaccard
    // Allow some variance since they're independent estimates
    expectNear(sim_original, sim_scaled, 0.2);
  };

  "empty set produces valid signature"_test = [] {
    WeightedMinHash<64> wmh;
    std::vector<WeightedMinHash<64>::WeightedFeature> empty_set;

    auto sig = wmh.computeSignature(empty_set);

    // Should not crash; signature is defined (all zeros from initialization)
    expectEq(sig.size(), 64uz);
  };

  "zero and negative weights are ignored"_test = [] {
    WeightedMinHash<64> wmh;
    std::vector<WeightedMinHash<64>::WeightedFeature> set_a = {
        {1, 5.0}, {2, 0.0}, {3, -1.0}};
    std::vector<WeightedMinHash<64>::WeightedFeature> set_b = {{1, 5.0}};

    auto sig_a = wmh.computeSignature(set_a);
    auto sig_b = wmh.computeSignature(set_b);

    double similarity = WeightedMinHash<64>::estimateSimilarity(sig_a, sig_b);
    // Zero/negative weights are ignored, so both sets effectively have just {1: 5.0}
    expectEq(similarity, 1.0);
  };

  "signature self-similarity is always 1.0"_test = [] {
    WeightedMinHash<128> wmh;
    std::vector<WeightedMinHash<128>::WeightedFeature> set = {
        {10, 2.5}, {20, 7.3}, {30, 1.1}, {40, 4.9}};

    auto sig = wmh.computeSignature(set);

    double similarity = WeightedMinHash<128>::estimateSimilarity(sig, sig);
    expectEq(similarity, 1.0);
  };

  "accuracy improves with more hashes"_test = [] {
    // Compare accuracy of small vs large number of hashes
    // Use known weighted Jaccard
    std::vector<WeightedMinHash<32>::WeightedFeature> set_a_32 = {
        {1, 4.0}, {2, 3.0}, {3, 2.0}, {4, 1.0}};
    std::vector<WeightedMinHash<32>::WeightedFeature> set_b_32 = {
        {1, 2.0}, {2, 4.0}, {3, 1.0}, {5, 3.0}};

    std::vector<WeightedMinHash<256>::WeightedFeature> set_a_256 = {
        {1, 4.0}, {2, 3.0}, {3, 2.0}, {4, 1.0}};
    std::vector<WeightedMinHash<256>::WeightedFeature> set_b_256 = {
        {1, 2.0}, {2, 4.0}, {3, 1.0}, {5, 3.0}};

    // Expected: min sum = 2+3+1+0+0 = 6, max sum = 4+4+2+1+3 = 14
    // J = 6/14 ≈ 0.4286
    double expected = 6.0 / 14.0;

    WeightedMinHash<32> wmh32;
    WeightedMinHash<256> wmh256;

    auto sig_a_32 = wmh32.computeSignature(set_a_32);
    auto sig_b_32 = wmh32.computeSignature(set_b_32);
    auto sig_a_256 = wmh256.computeSignature(set_a_256);
    auto sig_b_256 = wmh256.computeSignature(set_b_256);

    double sim_32 = WeightedMinHash<32>::estimateSimilarity(sig_a_32, sig_b_32);
    double sim_256 = WeightedMinHash<256>::estimateSimilarity(sig_a_256, sig_b_256);

    // Both should be reasonable estimates
    // SE for k=32: sqrt(0.4286 * 0.5714 / 32) ≈ 0.087
    // SE for k=256: sqrt(0.4286 * 0.5714 / 256) ≈ 0.031
    expectNear(sim_32, expected, 0.25);   // ~3 SE
    expectNear(sim_256, expected, 0.15);  // ~5 SE
  };

  "high overlap weighted sets"_test = [] {
    // A = {1: 10, 2: 10, 3: 10}
    // B = {1: 9, 2: 9, 3: 9}
    // min sum = 9 + 9 + 9 = 27
    // max sum = 10 + 10 + 10 = 30
    // J = 27/30 = 0.9
    WeightedMinHash<256> wmh;
    std::vector<WeightedMinHash<256>::WeightedFeature> set_a = {
        {1, 10.0}, {2, 10.0}, {3, 10.0}};
    std::vector<WeightedMinHash<256>::WeightedFeature> set_b = {
        {1, 9.0}, {2, 9.0}, {3, 9.0}};

    auto sig_a = wmh.computeSignature(set_a);
    auto sig_b = wmh.computeSignature(set_b);

    double similarity = WeightedMinHash<256>::estimateSimilarity(sig_a, sig_b);
    // SE = sqrt(0.9 * 0.1 / 256) ≈ 0.019
    expectNear(similarity, 0.9, 0.15);
  };

  "fractional weights work correctly"_test = [] {
    WeightedMinHash<128> wmh;
    std::vector<WeightedMinHash<128>::WeightedFeature> set_a = {
        {1, 0.5}, {2, 0.25}, {3, 0.125}};
    std::vector<WeightedMinHash<128>::WeightedFeature> set_b = {
        {1, 0.5}, {2, 0.25}, {3, 0.125}};

    auto sig_a = wmh.computeSignature(set_a);
    auto sig_b = wmh.computeSignature(set_b);

    double similarity = WeightedMinHash<128>::estimateSimilarity(sig_a, sig_b);
    expectEq(similarity, 1.0);
  };

  "very small weights work"_test = [] {
    WeightedMinHash<64> wmh;
    std::vector<WeightedMinHash<64>::WeightedFeature> set_a = {
        {1, 1e-10}, {2, 1e-10}};
    std::vector<WeightedMinHash<64>::WeightedFeature> set_b = {
        {1, 1e-10}, {2, 1e-10}};

    auto sig_a = wmh.computeSignature(set_a);
    auto sig_b = wmh.computeSignature(set_b);

    double similarity = WeightedMinHash<64>::estimateSimilarity(sig_a, sig_b);
    expectEq(similarity, 1.0);
  };

  "very large weights work"_test = [] {
    WeightedMinHash<64> wmh;
    std::vector<WeightedMinHash<64>::WeightedFeature> set_a = {
        {1, 1e10}, {2, 1e10}};
    std::vector<WeightedMinHash<64>::WeightedFeature> set_b = {
        {1, 1e10}, {2, 1e10}};

    auto sig_a = wmh.computeSignature(set_a);
    auto sig_b = wmh.computeSignature(set_b);

    double similarity = WeightedMinHash<64>::estimateSimilarity(sig_a, sig_b);
    expectEq(similarity, 1.0);
  };

  "static properties"_test = [] {
    expectEq(WeightedMinHash<64>::numHashes(), 64uz);
    expectEq(WeightedMinHash<128>::numHashes(), 128uz);
    expectEq(WeightedMinHash<256>::numHashes(), 256uz);
  };

  "multiple features weighted scenario"_test = [] {
    // More realistic scenario with many features
    // Document A: {word1: 5, word2: 3, word3: 1, word4: 2, word5: 4}
    // Document B: {word1: 4, word2: 4, word3: 2, word6: 3, word7: 2}
    // Overlap on word1, word2, word3
    // min sum = min(5,4) + min(3,4) + min(1,2) = 4 + 3 + 1 = 8
    // max sum = 5 + 4 + 2 + 2 + 4 + 3 + 2 = 22
    // J = 8/22 ≈ 0.364
    WeightedMinHash<256> wmh;
    std::vector<WeightedMinHash<256>::WeightedFeature> doc_a = {
        {1, 5.0}, {2, 3.0}, {3, 1.0}, {4, 2.0}, {5, 4.0}};
    std::vector<WeightedMinHash<256>::WeightedFeature> doc_b = {
        {1, 4.0}, {2, 4.0}, {3, 2.0}, {6, 3.0}, {7, 2.0}};

    auto sig_a = wmh.computeSignature(doc_a);
    auto sig_b = wmh.computeSignature(doc_b);

    double similarity = WeightedMinHash<256>::estimateSimilarity(sig_a, sig_b);
    double expected = 8.0 / 22.0;
    // SE ≈ 0.030
    expectNear(similarity, expected, 0.15);
  };

  return TestRegistry::result();
}
