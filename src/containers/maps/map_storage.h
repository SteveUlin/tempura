#pragma once

// ============================================================================
// MapStorage - Union for Const Key Handling
// ============================================================================
//
// Maps need to expose pair<const Key, Value> to prevent users from mutating
// keys (which would corrupt the hash table). But we also want to move keys
// efficiently during resize. This union aliases both pair types to solve this.
//
// USAGE:
//   MapStorage<K, V> storage;
//   construct_at(&storage.mutable_pair, key, value);  // Construct via mutable
//   auto& [k, v] = storage.const_pair;                // Access via const
//   storage.mutable_pair.second = std::move(new_val); // Mutate value
//
// WHY THIS IS WELL-DEFINED:
//   1. pair<K, V> and pair<const K, V> have identical memory layout
//      (const is a compile-time qualifier, not a runtime difference)
//   2. Union type punning between layout-compatible types is allowed
//   3. We construct through mutable_pair, access through const_pair
//
// This is the same technique used by Abseil's Swiss Tables:
//   https://abseil.io/about/design/swisstables
//   https://github.com/abseil/abseil-cpp/blob/master/absl/container/internal/container_memory.h
//
// ============================================================================

#include <utility>

namespace tempura {

template <typename Key, typename Value>
union MapStorage {
  std::pair<Key, Value> mutable_pair;
  std::pair<const Key, Value> const_pair;

  constexpr MapStorage() noexcept {}
  constexpr ~MapStorage() noexcept {}

  // Non-copyable/moveable at the union level - managed by container
  MapStorage(const MapStorage&) = delete;
  auto operator=(const MapStorage&) -> MapStorage& = delete;
  MapStorage(MapStorage&&) = delete;
  auto operator=(MapStorage&&) -> MapStorage& = delete;
};

}  // namespace tempura
