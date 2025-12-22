#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <queue>
#include <unordered_map>
#include <vector>

namespace tempura {

// LRU-K cache: evicts entry with oldest Kth-most-recent access.
// Scan-resistant - entries accessed < K times are evicted first.
// Common choice: K=2 for database buffer pools.
template <typename K, typename V, std::size_t KAccesses = 2>
class LruKCache {
  static_assert(KAccesses >= 1, "K must be at least 1");

 public:
  explicit LruKCache(std::size_t capacity) : capacity_{capacity} {
    assert(capacity > 0);
  }

  auto insert(const K& key, const V& value) -> bool {
    ++timestamp_;

    if (auto it = data_.find(key); it != data_.end()) {
      it->second.value = value;
      recordAccess(it->second);
      return false;
    }

    if (data_.size() >= capacity_) {
      evict();
    }

    auto& entry = data_[key];
    entry.value = value;
    entry.key = key;
    recordAccess(entry);
    return true;
  }

  auto get(const K& key) -> std::optional<V> {
    auto it = data_.find(key);
    if (it == data_.end()) {
      return std::nullopt;
    }
    ++timestamp_;
    recordAccess(it->second);
    return it->second.value;
  }

  auto erase(const K& key) -> bool {
    auto it = data_.find(key);
    if (it == data_.end()) {
      return false;
    }
    it->second.deleted = true;  // Lazy deletion
    data_.erase(it);
    return true;
  }

  [[nodiscard]] auto size() const -> std::size_t { return data_.size(); }
  [[nodiscard]] auto capacity() const -> std::size_t { return capacity_; }

 private:
  struct Entry {
    V value;
    K key;
    std::uint64_t access_times[KAccesses] = {};  // Circular buffer of last K accesses
    std::size_t access_count = 0;
    bool deleted = false;
  };

  // Priority based on Kth-most-recent access (or earliest access if < K accesses)
  struct EvictionCandidate {
    std::uint64_t priority;  // Lower = evict first
    K key;

    auto operator<(const EvictionCandidate& other) const -> bool {
      return priority > other.priority;  // Min-heap
    }
  };

  void recordAccess(Entry& entry) {
    std::size_t idx = entry.access_count % KAccesses;
    entry.access_times[idx] = timestamp_;
    ++entry.access_count;
  }

  auto getEvictionPriority(const Entry& entry) const -> std::uint64_t {
    if (entry.access_count < KAccesses) {
      // Fewer than K accesses - prioritize eviction (use 0 as earliest time)
      return 0;
    }
    // Return Kth-most-recent access time
    std::size_t oldest_idx = entry.access_count % KAccesses;
    return entry.access_times[oldest_idx];
  }

  void evict() {
    assert(!data_.empty());

    // Find entry with lowest eviction priority
    const K* victim = nullptr;
    std::uint64_t min_priority = std::numeric_limits<std::uint64_t>::max();

    for (const auto& [key, entry] : data_) {
      std::uint64_t priority = getEvictionPriority(entry);
      if (priority < min_priority) {
        min_priority = priority;
        victim = &key;
      }
    }

    assert(victim != nullptr);
    data_.erase(*victim);
  }

  std::size_t capacity_;
  std::uint64_t timestamp_ = 0;
  std::unordered_map<K, Entry> data_;
};

}  // namespace tempura
