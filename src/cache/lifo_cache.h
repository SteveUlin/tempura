#pragma once

#include <cassert>
#include <cstddef>
#include <list>
#include <optional>
#include <unordered_map>

namespace tempura {

// Last-In-First-Out cache - evicts the most recently inserted entry.
// Stack-like behavior. Useful when newer entries are less valuable.
// O(1) get/insert/erase.
template <typename K, typename V>
class LifoCache {
 public:
  explicit LifoCache(std::size_t capacity) : capacity_{capacity} {
    assert(capacity > 0);
  }

  auto insert(const K& key, const V& value) -> bool {
    if (auto it = data_.find(key); it != data_.end()) {
      it->second.first = value;
      // Note: LIFO doesn't change position on update
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
    // LIFO doesn't update position on access
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
    data_.erase(order_.back());  // Back = newest (opposite of FIFO)
    order_.pop_back();
  }

  std::size_t capacity_;
  std::list<K> order_;  // Front = oldest, Back = newest
  std::unordered_map<K, std::pair<V, typename std::list<K>::iterator>> data_;
};

}  // namespace tempura
