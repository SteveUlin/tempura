#pragma once

#include <cassert>
#include <cstddef>
#include <optional>
#include <random>
#include <unordered_map>
#include <vector>

namespace tempura {

// Cache with random eviction policy. O(1) get/insert, O(1) amortized erase.
// Simple baseline - surprisingly effective for many workloads.
template <typename K, typename V, typename Rng = std::mt19937>
class RandomCache {
 public:
  explicit RandomCache(std::size_t capacity, Rng rng = Rng{std::random_device{}()})
      : capacity_{capacity}, rng_{std::move(rng)} {
    assert(capacity > 0);
    keys_.reserve(capacity);
  }

  auto insert(const K& key, const V& value) -> bool {
    if (auto it = data_.find(key); it != data_.end()) {
      it->second.first = value;
      return false;
    }

    if (data_.size() >= capacity_) {
      evictRandom();
    }

    std::size_t idx = keys_.size();
    keys_.push_back(key);
    data_[key] = {value, idx};
    return true;
  }

  auto get(const K& key) -> std::optional<V> {
    auto it = data_.find(key);
    if (it == data_.end()) {
      return std::nullopt;
    }
    return it->second.first;
  }

  auto erase(const K& key) -> bool {
    auto it = data_.find(key);
    if (it == data_.end()) {
      return false;
    }
    removeAtIndex(it->second.second);
    data_.erase(it);
    return true;
  }

  auto size() const -> std::size_t { return data_.size(); }
  auto capacity() const -> std::size_t { return capacity_; }

 private:
  void evictRandom() {
    assert(!keys_.empty());
    std::uniform_int_distribution<std::size_t> dist{0, keys_.size() - 1};
    std::size_t idx = dist(rng_);
    data_.erase(keys_[idx]);
    removeAtIndex(idx);
  }

  void removeAtIndex(std::size_t idx) {
    // Swap with last element for O(1) removal
    if (idx != keys_.size() - 1) {
      K& last_key = keys_.back();
      data_[last_key].second = idx;
      keys_[idx] = std::move(last_key);
    }
    keys_.pop_back();
  }

  std::size_t capacity_;
  Rng rng_;
  std::vector<K> keys_;
  std::unordered_map<K, std::pair<V, std::size_t>> data_;  // value + index in keys_
};

}  // namespace tempura
