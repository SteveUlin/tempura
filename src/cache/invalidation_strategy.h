#pragma once

#include <concepts>
#include <cstddef>

namespace tempura::cache {

// An InvalidationStrategy determines when cached entries should be evicted.
// Strategies are notified of cache operations and decide what to invalidate.
template <typename S, typename Key>
concept InvalidationStrategy = requires(S& strategy, const Key& key) {
  // Called when an entry is accessed (read hit)
  { strategy.onAccess(key) } -> std::same_as<void>;

  // Called when a new entry is inserted
  { strategy.onInsert(key) } -> std::same_as<void>;

  // Called when an entry is removed
  { strategy.onRemove(key) } -> std::same_as<void>;

  // Returns the next key to evict, or indicates no eviction needed
  { strategy.selectVictim() } -> std::convertible_to<Key>;

  // Returns true if a key should be evicted (key is set), false otherwise
  { strategy.shouldEvict() } -> std::convertible_to<bool>;
};

}  // namespace tempura::cache
