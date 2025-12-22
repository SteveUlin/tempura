#pragma once

#include <cassert>
#include <cstddef>
#include <list>
#include <optional>
#include <unordered_map>

namespace tempura {

// Most Frequently Used cache - evicts the most accessed entry.
// Counterintuitive, but useful when hot items become cold quickly
// (e.g., viral content that loses relevance).
// O(1) get/insert/erase.
template <typename K, typename V>
class MfuCache {
 public:
  explicit MfuCache(std::size_t capacity) : capacity_{capacity} {
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

    if (max_freq_ == 0) {
      max_freq_ = 1;
    }
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

    if (old_freq_it->second.empty()) {
      freq_lists_.erase(old_freq_it);
    }

    // Add to new frequency list
    auto& new_list = getOrCreateFreqList(new_freq);
    new_list.push_front(key);

    entry.freq = new_freq;
    entry.iter = new_list.begin();

    if (new_freq > max_freq_) {
      max_freq_ = new_freq;
    }
  }

  void evict() {
    assert(!data_.empty());

    // Find maximum frequency with entries
    while (max_freq_ > 0) {
      auto it = freq_lists_.find(max_freq_);
      if (it != freq_lists_.end() && !it->second.empty()) {
        break;
      }
      --max_freq_;
    }

    auto max_freq_it = freq_lists_.find(max_freq_);
    assert(max_freq_it != freq_lists_.end());
    assert(!max_freq_it->second.empty());

    // Evict MRU within maximum frequency (front of list)
    K victim = max_freq_it->second.front();
    max_freq_it->second.pop_front();

    if (max_freq_it->second.empty()) {
      freq_lists_.erase(max_freq_it);
    }

    data_.erase(victim);
  }

  std::size_t capacity_;
  std::size_t max_freq_ = 0;
  std::unordered_map<K, Entry> data_;
  std::unordered_map<std::size_t, std::list<K>> freq_lists_;
};

}  // namespace tempura
