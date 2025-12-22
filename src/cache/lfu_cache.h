#pragma once

#include <cassert>
#include <cstddef>
#include <list>
#include <optional>
#include <unordered_map>

namespace tempura {

// Least Frequently Used cache with O(1) operations.
// Evicts entry with lowest access count. Ties broken by LRU within frequency.
template <typename K, typename V>
class LfuCache {
 public:
  explicit LfuCache(std::size_t capacity) : capacity_{capacity} {
    assert(capacity > 0);
  }

  auto insert(const K& key, const V& value) -> bool {
    if (auto it = data_.find(key); it != data_.end()) {
      it->second.value = value;
      touch(it->second);
      return false;
    }

    if (data_.size() >= capacity_) {
      evict();
    }

    // New entries start at frequency 1
    auto& freq_list = getOrCreateFreqList(1);
    freq_list.push_front(key);

    auto& entry = data_[key];
    entry.value = value;
    entry.freq = 1;
    entry.iter = freq_list.begin();

    min_freq_ = 1;
    return true;
  }

  auto get(const K& key) -> std::optional<V> {
    auto it = data_.find(key);
    if (it == data_.end()) {
      return std::nullopt;
    }
    touch(it->second);
    return it->second.value;
  }

  auto erase(const K& key) -> bool {
    auto it = data_.find(key);
    if (it == data_.end()) {
      return false;
    }

    auto& entry = it->second;
    auto freq_it = freq_lists_.find(entry.freq);
    assert(freq_it != freq_lists_.end());

    freq_it->second.erase(entry.iter);
    if (freq_it->second.empty()) {
      freq_lists_.erase(freq_it);
    }

    data_.erase(it);
    return true;
  }

  auto size() const -> std::size_t { return data_.size(); }
  auto capacity() const -> std::size_t { return capacity_; }

 private:
  struct Entry {
    V value;
    std::size_t freq;
    typename std::list<K>::iterator iter;
  };

  auto getOrCreateFreqList(std::size_t freq) -> std::list<K>& {
    return freq_lists_[freq];
  }

  void touch(Entry& entry) {
    std::size_t old_freq = entry.freq;
    std::size_t new_freq = old_freq + 1;

    // Remove from old frequency list
    auto old_freq_it = freq_lists_.find(old_freq);
    assert(old_freq_it != freq_lists_.end());
    K key = *entry.iter;
    old_freq_it->second.erase(entry.iter);

    // Update min_freq if we emptied the min frequency list
    if (old_freq_it->second.empty()) {
      freq_lists_.erase(old_freq_it);
      if (old_freq == min_freq_) {
        min_freq_ = new_freq;
      }
    }

    // Add to new frequency list
    auto& new_list = getOrCreateFreqList(new_freq);
    new_list.push_front(key);

    entry.freq = new_freq;
    entry.iter = new_list.begin();
  }

  void evict() {
    assert(!data_.empty());

    auto min_freq_it = freq_lists_.find(min_freq_);
    assert(min_freq_it != freq_lists_.end());
    assert(!min_freq_it->second.empty());

    // Evict LRU within minimum frequency (back of list)
    K victim = min_freq_it->second.back();
    min_freq_it->second.pop_back();

    if (min_freq_it->second.empty()) {
      freq_lists_.erase(min_freq_it);
    }

    data_.erase(victim);
  }

  std::size_t capacity_;
  std::size_t min_freq_ = 0;
  std::unordered_map<K, Entry> data_;
  std::unordered_map<std::size_t, std::list<K>> freq_lists_;  // freq -> keys at that freq
};

}  // namespace tempura
