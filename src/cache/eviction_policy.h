#pragma once

#include <concepts>
#include <cstddef>
#include <optional>

namespace tempura {

// An EvictionPolicy determines which cache entries to remove when capacity is exceeded.
// Policies track access patterns and select victims for eviction.
//
// Standard policies: LRU, LFU, FIFO, TTL, ARC
// See: https://en.wikipedia.org/wiki/Cache_replacement_policies
template <typename P, typename Key>
concept EvictionPolicy = requires(P& policy, const Key& key) {
  // Called when an entry is accessed (cache hit)
  { policy.touch(key) } -> std::same_as<void>;

  // Called when a new entry is inserted
  { policy.insert(key) } -> std::same_as<void>;

  // Called when an entry is explicitly removed
  { policy.erase(key) } -> std::same_as<void>;

  // Select and remove the next victim for eviction
  // Returns the key to evict, or nullopt if cache is empty
  { policy.evict() } -> std::same_as<std::optional<Key>>;

  // Number of entries being tracked
  { policy.size() } -> std::convertible_to<std::size_t>;
};

// Capacity-aware policy that knows when eviction is needed
template <typename P, typename Key>
concept BoundedEvictionPolicy = EvictionPolicy<P, Key> && requires(P& policy) {
  { policy.capacity() } -> std::convertible_to<std::size_t>;
  { policy.full() } -> std::convertible_to<bool>;
};

}  // namespace tempura
