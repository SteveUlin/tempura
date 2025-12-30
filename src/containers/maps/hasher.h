#pragma once

// Hash function concept for map containers
//
// A hash function takes a key and returns a size_t. Good hash functions:
//   - Distribute keys uniformly across the output range
//   - Are deterministic (same key always gives same hash)
//   - Are fast to compute

#include <concepts>
#include <cstddef>

namespace tempura {

template <typename H, typename K>
concept Hasher = requires(H h, K k) {
  { h(k) } -> std::convertible_to<std::size_t>;
};

}  // namespace tempura
