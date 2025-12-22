#pragma once

#include <cassert>
#include <cstddef>
#include <list>
#include <optional>
#include <unordered_map>

namespace tempura {

// First-In-First-Out cache. Evicts oldest inserted entry regardless of access.
// O(1) get/insert/erase. Simple baseline for comparison.
template <typename K, typename V>
class FifoCache {
 public:
  explicit FifoCache(std::size_t capacity) : capacity_{capacity} {
    assert(capacity > 0);
  }

  auto insert(const K& key, const V& value) -> bool {
    if (auto it = data_.find(key); it != data_.end()) {
      it->second.first = value;
      // Note: FIFO doesn't move to back on update - maintains original position
      return false;
    }

    if (data_.size() >= capacity_) {
      evict();
    }

    order_.push_back(key);
    data_[key] = {value, std::prev(order_.end())};
    return true;
  }

  auto get(const K& key) -> std::optional<V> {
    auto it = data_.find(key);
    if (it == data_.end()) {
      return std::nullopt;
    }
    // Note: FIFO doesn't update position on access
    return it->second.first;
  }

  auto erase(const K& key) -> bool {
    auto it = data_.find(key);
    if (it == data_.end()) {
      return false;
    }
    order_.erase(it->second.second);
    data_.erase(it);
    return true;
  }

  auto size() const -> std::size_t { return data_.size(); }
  auto capacity() const -> std::size_t { return capacity_; }

 private:
  void evict() {
    assert(!order_.empty());
    data_.erase(order_.front());
    order_.pop_front();
  }

  std::size_t capacity_;
  std::list<K> order_;  // Front = oldest, Back = newest
  std::unordered_map<K, std::pair<V, typename std::list<K>::iterator>> data_;
};

}  // namespace tempura
