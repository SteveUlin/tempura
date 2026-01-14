#include "probabilistic/simhash.h"

#include <cmath>
#include <functional>
#include <random>
#include <string>
#include <vector>

#include "unit.h"

using namespace tempura;

auto main() -> int {
  "identical features produce distance 0"_test = [] {
    using SH = SimHash<64>;

    std::vector<SH::WeightedFeature> features = {
        {std::hash<std::string>{}("apple"), 3.0},
        {std::hash<std::string>{}("banana"), 2.0},
        {std::hash<std::string>{}("cherry"), 1.0},
    };

    auto sig1 = SH::computeSignature(features);
    auto sig2 = SH::computeSignature(features);

    expectEq(SH::hammingDistance(sig1, sig2), 0uz);
    expectNear(SH::estimateSimilarity(sig1, sig2), 1.0, 0.0001);
  };

  "completely different features have high distance"_test = [] {
    using SH = SimHash<64>;

    // Two completely disjoint feature sets
    std::vector<SH::WeightedFeature> features_a = {
        {std::hash<std::string>{}("apple"), 1.0},
        {std::hash<std::string>{}("banana"), 1.0},
        {std::hash<std::string>{}("cherry"), 1.0},
        {std::hash<std::string>{}("date"), 1.0},
        {std::hash<std::string>{}("elderberry"), 1.0},
    };

    std::vector<SH::WeightedFeature> features_b = {
        {std::hash<std::string>{}("fig"), 1.0},
        {std::hash<std::string>{}("grape"), 1.0},
        {std::hash<std::string>{}("honeydew"), 1.0},
        {std::hash<std::string>{}("kiwi"), 1.0},
        {std::hash<std::string>{}("lemon"), 1.0},
    };

    auto sig_a = SH::computeSignature(features_a);
    auto sig_b = SH::computeSignature(features_b);

    auto dist = SH::hammingDistance(sig_a, sig_b);
    // For completely orthogonal vectors, expected distance is ~NumBits/2
    // But random features aren't perfectly orthogonal, so allow wide tolerance
    expectGT(dist, 10uz);  // Should differ significantly
    expectLT(dist, 54uz);  // But not all bits (they're not anti-correlated)
  };

  "similar features have low distance"_test = [] {
    using SH = SimHash<64>;

    // Two feature sets with significant overlap
    std::vector<SH::WeightedFeature> features_a = {
        {std::hash<std::string>{}("the"), 10.0},
        {std::hash<std::string>{}("quick"), 3.0},
        {std::hash<std::string>{}("brown"), 2.0},
        {std::hash<std::string>{}("fox"), 5.0},
        {std::hash<std::string>{}("jumps"), 1.0},
    };

    // Same dominant features, slightly different content
    std::vector<SH::WeightedFeature> features_b = {
        {std::hash<std::string>{}("the"), 10.0},
        {std::hash<std::string>{}("quick"), 3.0},
        {std::hash<std::string>{}("brown"), 2.0},
        {std::hash<std::string>{}("dog"), 5.0},  // Different from "fox"
        {std::hash<std::string>{}("runs"), 1.0},  // Different from "jumps"
    };

    auto sig_a = SH::computeSignature(features_a);
    auto sig_b = SH::computeSignature(features_b);

    auto dist = SH::hammingDistance(sig_a, sig_b);
    // Should be closer than completely different features
    expectLT(dist, 32uz);  // Less than half the bits differ
  };

  "similarity estimate is reasonable"_test = [] {
    using SH = SimHash<64>;

    // Distance 0 -> similarity 1.0
    expectNear(SH::estimateSimilarity(0uz), 1.0, 0.0001);

    // Distance NumBits/2 -> similarity ~0 (angle = π/2)
    expectNear(SH::estimateSimilarity(32uz), 0.0, 0.0001);

    // Distance NumBits -> similarity -1.0 (angle = π)
    expectNear(SH::estimateSimilarity(64uz), -1.0, 0.0001);

    // Distance 16 -> similarity ~0.707 (angle = π/4)
    expectNear(SH::estimateSimilarity(16uz), std::cos(std::numbers::pi / 4.0),
               0.0001);
  };

  "unweighted features work correctly"_test = [] {
    using SH = SimHash<64>;

    std::vector<std::size_t> hashes = {
        std::hash<std::string>{}("one"),
        std::hash<std::string>{}("two"),
        std::hash<std::string>{}("three"),
    };

    auto sig1 = SH::computeSignature(std::span{hashes});
    auto sig2 = SH::computeSignature(std::span{hashes});

    expectEq(SH::hammingDistance(sig1, sig2), 0uz);
  };

  "weights affect the signature"_test = [] {
    using SH = SimHash<64>;

    std::size_t hash_a = std::hash<std::string>{}("important");
    std::size_t hash_b = std::hash<std::string>{}("trivial");

    // Heavily weighted toward "important"
    std::vector<SH::WeightedFeature> features_heavy_a = {
        {hash_a, 100.0},
        {hash_b, 1.0},
    };

    // Heavily weighted toward "trivial"
    std::vector<SH::WeightedFeature> features_heavy_b = {
        {hash_a, 1.0},
        {hash_b, 100.0},
    };

    // Balanced weights
    std::vector<SH::WeightedFeature> features_balanced = {
        {hash_a, 50.0},
        {hash_b, 50.0},
    };

    auto sig_heavy_a = SH::computeSignature(features_heavy_a);
    auto sig_heavy_b = SH::computeSignature(features_heavy_b);
    auto sig_balanced = SH::computeSignature(features_balanced);

    // Heavy A should be more similar to balanced than to heavy B
    auto dist_heavy_a_b = SH::hammingDistance(sig_heavy_a, sig_heavy_b);
    auto dist_heavy_a_balanced = SH::hammingDistance(sig_heavy_a, sig_balanced);
    auto dist_heavy_b_balanced = SH::hammingDistance(sig_heavy_b, sig_balanced);

    // The balanced signature should be somewhere between
    expectLT(dist_heavy_a_balanced, dist_heavy_a_b);
    expectLT(dist_heavy_b_balanced, dist_heavy_a_b);
  };

  "128-bit signatures work"_test = [] {
    using SH = SimHash<128>;

    std::vector<SH::WeightedFeature> features = {
        {std::hash<std::string>{}("hello"), 1.0},
        {std::hash<std::string>{}("world"), 1.0},
    };

    auto sig = SH::computeSignature(features);
    expectEq(sig.size(), 2uz);  // 128 bits = 2 uint64_t

    auto dist = SH::hammingDistance(sig, sig);
    expectEq(dist, 0uz);

    expectEq(SH::numBits(), 128uz);
  };

  "256-bit signatures work"_test = [] {
    using SH = SimHash<256>;

    std::vector<SH::WeightedFeature> features = {
        {std::hash<std::string>{}("hello"), 1.0},
        {std::hash<std::string>{}("world"), 1.0},
    };

    auto sig1 = SH::computeSignature(features);
    expectEq(sig1.size(), 4uz);  // 256 bits = 4 uint64_t

    std::vector<SH::WeightedFeature> features2 = {
        {std::hash<std::string>{}("goodbye"), 1.0},
        {std::hash<std::string>{}("moon"), 1.0},
    };

    auto sig2 = SH::computeSignature(features2);
    auto dist = SH::hammingDistance(sig1, sig2);

    // Distance should be nonzero for different features
    expectGT(dist, 0uz);
    // And the similarity estimate should work
    auto sim = SH::estimateSimilarity(sig1, sig2);
    expectGE(sim, -1.0);
    expectLE(sim, 1.0);
  };

  "empty features produce zero signature"_test = [] {
    using SH = SimHash<64>;

    std::vector<SH::WeightedFeature> empty;
    auto sig = SH::computeSignature(empty);

    // All sums are 0, so all bits should be 0 (since >0 is required for bit=1)
    expectEq(sig, 0uz);
  };

  "numBits returns correct value"_test = [] {
    expectEq(SimHash<64>::numBits(), 64uz);
    expectEq(SimHash<128>::numBits(), 128uz);
    expectEq(SimHash<256>::numBits(), 256uz);
  };

  "statistical test: random vectors have ~half bits different"_test = [] {
    // For truly random (orthogonal) vectors, we expect Hamming distance ≈ NumBits/2
    // Generate random feature sets and verify the average distance is reasonable
    using SH = SimHash<64>;

    std::mt19937 rng{42};
    std::uniform_int_distribution<std::size_t> hash_dist;
    std::uniform_real_distribution<double> weight_dist{0.1, 10.0};

    constexpr int kNumTrials = 100;
    constexpr int kFeaturesPerSet = 20;

    double total_distance = 0.0;

    for (int trial = 0; trial < kNumTrials; ++trial) {
      std::vector<SH::WeightedFeature> features_a;
      std::vector<SH::WeightedFeature> features_b;

      for (int i = 0; i < kFeaturesPerSet; ++i) {
        features_a.push_back({hash_dist(rng), weight_dist(rng)});
        features_b.push_back({hash_dist(rng), weight_dist(rng)});
      }

      auto sig_a = SH::computeSignature(features_a);
      auto sig_b = SH::computeSignature(features_b);

      total_distance += static_cast<double>(SH::hammingDistance(sig_a, sig_b));
    }

    double avg_distance = total_distance / kNumTrials;
    // Expected ~32 for random orthogonal vectors
    // Allow range [20, 44] to account for statistical variance and non-orthogonality
    expectGT(avg_distance, 20.0);
    expectLT(avg_distance, 44.0);
  };

  return TestRegistry::result();
}
