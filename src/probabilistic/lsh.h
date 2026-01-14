#pragma once

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <functional>
#include <span>
#include <unordered_map>
#include <vector>

namespace tempura {

// ============================================================================
// Locality-Sensitive Hashing (LSH) for MinHash Signatures
// ============================================================================
//
// LSH is a technique for finding approximately similar items in sublinear time.
// Instead of comparing every pair (O(n^2)), LSH hashes similar items to the
// same buckets with high probability, reducing candidate pairs dramatically.
//
// This implementation works with MinHash signatures for Jaccard similarity.
// MinHash converts sets into fixed-length signatures where:
//   P(signature[i] matches) = Jaccard(A, B)
//
// THE BANDING TECHNIQUE
// ---------------------
// A signature of k hash values is divided into b bands of r rows each (k = b*r).
// For each band, we hash the r values together. Two items are CANDIDATES if
// they hash to the same bucket in at least one band.
//
// For two items with Jaccard similarity s:
//   - P(match in all r rows of one band) = s^r
//   - P(mismatch in one band) = 1 - s^r
//   - P(mismatch in all b bands) = (1 - s^r)^b
//   - P(candidate | similarity s) = 1 - (1 - s^r)^b
//
// This creates an S-curve threshold behavior:
//
//     P(candidate)
//     1.0 |           ______
//         |          /
//         |         |
//         |        /
//     0.5 |-------*--------   <-- threshold t ≈ (1/b)^(1/r)
//         |      /
//         |     |
//         |    /
//     0.0 |___/
//         +-------------------> Similarity s
//         0                1
//
// TUNING PARAMETERS
// -----------------
// Choose b and r based on your desired similarity threshold t:
//   - threshold t ≈ (1/b)^(1/r)
//   - Higher r = steeper curve, fewer false positives at low similarity
//   - Higher b = more chances to match, fewer false negatives at high similarity
//
// Example configurations (k=100 hash signature):
//   b=20, r=5:  t ≈ 0.55, good for moderate similarity
//   b=10, r=10: t ≈ 0.79, strict matching for high similarity
//   b=50, r=2:  t ≈ 0.14, permissive matching for low similarity
//
// COMPLEXITY
// ----------
//   insert:  O(b) hash computations, O(1) amortized per bucket
//   query:   O(b) hash computations + O(candidates found)
//   Space:   O(b * n) where n = number of items
//
// TEMPLATE PARAMETERS
// -------------------
//   Id          - Identifier type for items (e.g., int, string)
//   NumBands    - Number of bands (b), more bands = higher recall
//   RowsPerBand - Rows per band (r), more rows = higher precision
//
// USAGE
// -----
//   LSHIndex<int, 20, 5> index;  // 20 bands, 5 rows each = 100 hash signature
//
//   // Insert items with their MinHash signatures
//   index.insert(item_id_1, signature_1);
//   index.insert(item_id_2, signature_2);
//
//   // Query for candidates similar to a signature
//   auto candidates = index.query(query_signature);
//   // candidates contains all items that match in at least one band
//
template <typename Id, std::size_t NumBands, std::size_t RowsPerBand>
class LSHIndex {
  static_assert(NumBands > 0, "NumBands must be positive");
  static_assert(RowsPerBand > 0, "RowsPerBand must be positive");

public:
  static constexpr std::size_t kSignatureSize = NumBands * RowsPerBand;
  static constexpr std::size_t kNumBands = NumBands;
  static constexpr std::size_t kRowsPerBand = RowsPerBand;

  using Signature = std::array<std::size_t, kSignatureSize>;

  LSHIndex() = default;

  // Insert an item with its MinHash signature into the index.
  // The signature must have exactly NumBands * RowsPerBand elements.
  void insert(const Id& id, const Signature& signature) {
    for (std::size_t band = 0; band < NumBands; ++band) {
      std::size_t band_hash = hashBand(signature, band);
      band_tables_[band][band_hash].push_back(id);
    }
  }

  // Insert using a span (for runtime-sized signatures that match kSignatureSize)
  void insert(const Id& id, std::span<const std::size_t> signature) {
    assert(signature.size() == kSignatureSize);
    Signature sig;
    std::copy(signature.begin(), signature.end(), sig.begin());
    insert(id, sig);
  }

  // Query for candidate items that might be similar to the given signature.
  // Returns items that match in at least one band (share all r rows in any band).
  // The result may contain duplicates if items match in multiple bands.
  auto query(const Signature& signature) const -> std::vector<Id> {
    std::vector<Id> candidates;

    for (std::size_t band = 0; band < NumBands; ++band) {
      std::size_t band_hash = hashBand(signature, band);
      auto it = band_tables_[band].find(band_hash);
      if (it != band_tables_[band].end()) {
        for (const auto& id : it->second) {
          candidates.push_back(id);
        }
      }
    }

    return candidates;
  }

  // Query using a span
  auto query(std::span<const std::size_t> signature) const -> std::vector<Id> {
    assert(signature.size() == kSignatureSize);
    Signature sig;
    std::copy(signature.begin(), signature.end(), sig.begin());
    return query(sig);
  }

  // Query and return unique candidates (deduplicated).
  auto queryUnique(const Signature& signature) const -> std::vector<Id> {
    auto candidates = query(signature);
    std::sort(candidates.begin(), candidates.end());
    candidates.erase(std::unique(candidates.begin(), candidates.end()),
                     candidates.end());
    return candidates;
  }

  // Query unique using a span
  auto queryUnique(std::span<const std::size_t> signature) const
      -> std::vector<Id> {
    assert(signature.size() == kSignatureSize);
    Signature sig;
    std::copy(signature.begin(), signature.end(), sig.begin());
    return queryUnique(sig);
  }

  // Clear all items from the index.
  void clear() {
    for (auto& table : band_tables_) {
      table.clear();
    }
  }

  // Get the number of buckets used across all bands (for diagnostics).
  auto totalBuckets() const -> std::size_t {
    std::size_t total = 0;
    for (const auto& table : band_tables_) {
      total += table.size();
    }
    return total;
  }

  // Calculate the theoretical probability of two items being candidates
  // given their Jaccard similarity. Uses the formula: 1 - (1 - s^r)^b
  static constexpr auto candidateProbability(double similarity) -> double {
    double p_band_match = 1.0;
    for (std::size_t i = 0; i < RowsPerBand; ++i) {
      p_band_match *= similarity;
    }
    double p_no_match = 1.0;
    for (std::size_t i = 0; i < NumBands; ++i) {
      p_no_match *= (1.0 - p_band_match);
    }
    return 1.0 - p_no_match;
  }

  // Approximate similarity threshold where P(candidate) = 0.5
  // t ≈ (1/b)^(1/r)
  static constexpr auto threshold() -> double {
    // (1/NumBands)^(1/RowsPerBand)
    double base = 1.0 / static_cast<double>(NumBands);
    double exp = 1.0 / static_cast<double>(RowsPerBand);
    // Use exp(log(base) * exp) = base^exp
    // Approximation for constexpr context
    return std::pow(base, exp);
  }

private:
  // Hash a single band of the signature.
  // Combines r consecutive hash values starting at band * RowsPerBand.
  auto hashBand(const Signature& signature, std::size_t band) const
      -> std::size_t {
    std::size_t start = band * RowsPerBand;
    std::size_t hash = 0;

    // Combine hash values using a simple but effective scheme
    for (std::size_t i = 0; i < RowsPerBand; ++i) {
      // Mix using a prime multiplier and XOR
      hash ^= signature[start + i] + 0x9e3779b9 + (hash << 6) + (hash >> 2);
    }

    return hash;
  }

  // One hash table per band: bucket_hash -> list of item ids
  std::array<std::unordered_map<std::size_t, std::vector<Id>>, NumBands>
      band_tables_;
};

}  // namespace tempura
