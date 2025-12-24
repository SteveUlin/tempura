#pragma once

#include <cassert>
#include <cstddef>
#include <list>
#include <optional>
#include <unordered_map>

namespace tempura {

// Least Recently Used cache with O(1) get/insert/erase.
template <typename K, typename V>
class LruCache {
 public:
  explicit LruCache(std::size_t capacity) : capacity_{capacity} {
    assert(capacity > 0);
  }

  // Insert or update a key-value pair.
  // Returns true if new element inserted, false if existing element updated.
  auto insert(const K& key, const V& value) -> bool {
    if (auto it = data_.find(key); it != data_.end()) {
      it->second.first = value;
      touch(it);
      return false;
    }

    if (data_.size() >= capacity_) {
      evictLru();
    }

    order_.push_front(key);
    data_[key] = {value, order_.begin()};
    return true;
  }

  auto get(const K& key) -> std::optional<V> {
    auto it = data_.find(key);
    if (it == data_.end()) {
      return std::nullopt;
    }
    touch(it);
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
  using DataIter = typename std::unordered_map<K, std::pair<V, typename std::list<K>::iterator>>::iterator;

  void touch(DataIter it) {
    order_.splice(order_.begin(), order_, it->second.second);
  }

  void evictLru() {
    assert(!order_.empty());
    data_.erase(order_.back());
    order_.pop_back();
  }

  std::size_t capacity_;
  std::list<K> order_;  // Front = MRU, Back = LRU
  std::unordered_map<K, std::pair<V, typename std::list<K>::iterator>> data_;
};

}  // namespace tempura
