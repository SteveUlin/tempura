# Hash Map Implementations

This directory contains multiple hash map implementations for **exploration and comparison purposes**. The goal is to understand the tradeoffs between different collision resolution strategies, not to provide a single "best" implementation.

## Implementations

| File | Strategy | Key Characteristics |
|------|----------|---------------------|
| `linear_probing.h` | Linear probing | Cache-friendly, simple, prone to clustering |
| `quadratic_probing.h` | Triangular probing | Reduces clustering, requires power-of-2 capacity |
| `double_hashing.h` | Double hashing | Best distribution, requires prime capacity |
| `separate_chaining.h` | Linked lists | Stable pointers, simpler deletion, more allocations |

## Why Multiple Implementations?

Each strategy has different performance characteristics:

- **Linear probing**: Best cache locality due to sequential memory access. Degrades with clustering at high load factors.

- **Quadratic/triangular probing**: Reduces primary clustering while maintaining reasonable cache behavior. Power-of-2 capacity enables fast modulo via bitmasking.

- **Double hashing**: Near-optimal probe distribution. Requires prime capacity for guaranteed coverage, making modulo slower.

- **Separate chaining**: No clustering, O(1) deletion without tombstones, stable element addresses. Trades cache locality for simplicity.

## Benchmarks

Run `map_benchmark` to compare performance:
```bash
cmake --build build --target map_benchmark
./build/src/containers/maps/map_benchmark
```

## Implementation Notes

### The Const Key Problem

Map semantics require exposing `pair<const Key, Value>` to prevent users from mutating keys (which would corrupt the hash table). But we also want to move keys efficiently during resize.

### Solution: Union Type Aliasing (Abseil's Approach)

The open addressing implementations use `MapStorage<K,V>` (from `map_storage.h`) - a union that aliases both pair types:

```cpp
template <typename Key, typename Value>
union MapStorage {
  std::pair<Key, Value> mutable_pair;        // For internal moves during resize
  std::pair<const Key, Value> const_pair;    // For user-facing access
};
```

Each slot uses `ManualLifetime<MapStorage<K,V>>` for deferred construction.

This is well-defined because:
1. Both types have identical layout (const is compile-time only)
2. Union type punning between layout-compatible types is allowed
3. We construct through `mutable_pair`, access through `const_pair`

See:
- https://abseil.io/about/design/swisstables
- https://github.com/abseil/abseil-cpp/blob/master/absl/container/internal/container_memory.h

Separate chaining stores `pair<const Key, Value>` directly since nodes aren't moved during resize.
