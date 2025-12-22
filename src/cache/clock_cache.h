#pragma once

#include <cassert>
#include <cstddef>
#include <optional>
#include <unordered_map>
#include <vector>

namespace tempura {

// Clock cache (second-chance algorithm). Memory-efficient LRU approximation.
// Used in operating systems for page replacement.
// O(1) get/insert amortized, O(capacity) worst case eviction.
template <typename K, typename V>
class ClockCache {
 public:
  explicit ClockCache(std::size_t capacity) : capacity_{capacity} {
    assert(capacity > 0);
    slots_.reserve(capacity);
  }

  auto insert(const K& key, const V& value) -> bool {
    if (auto it = index_.find(key); it != index_.end()) {
      auto& slot = slots_[it->second];
      slot.value = value;
      slot.referenced = true;
      return false;
    }

    if (index_.size() >= capacity_) {
      evict();
    }

    std::size_t idx = slots_.size();
    if (idx < capacity_) {
      slots_.push_back({key, value, true});
    } else {
      // Reuse evicted slot (hand_ points to it after eviction)
      idx = hand_;
      slots_[idx] = {key, value, true};
    }
    index_[key] = idx;
    return true;
  }

  auto get(const K& key) -> std::optional<V> {
    auto it = index_.find(key);
    if (it == index_.end()) {
      return std::nullopt;
    }
    auto& slot = slots_[it->second];
    slot.referenced = true;
    return slot.value;
  }

  auto erase(const K& key) -> bool {
    auto it = index_.find(key);
    if (it == index_.end()) {
      return false;
    }
    slots_[it->second].deleted = true;
    index_.erase(it);
    return true;
  }

  auto size() const -> std::size_t { return index_.size(); }
  auto capacity() const -> std::size_t { return capacity_; }

 private:
  struct Slot {
    K key;
    V value;
    bool referenced = false;
    bool deleted = false;
  };

  void evict() {
    assert(!index_.empty());

    // Sweep until finding unreferenced (or deleted) entry
    while (true) {
      auto& slot = slots_[hand_];

      if (slot.deleted) {
        // Skip deleted slots
        advanceHand();
        continue;
      }

      if (!slot.referenced) {
        // Found victim
        index_.erase(slot.key);
        slot.deleted = true;
        return;
      }

      // Give second chance
      slot.referenced = false;
      advanceHand();
    }
  }

  void advanceHand() {
    hand_ = (hand_ + 1) % slots_.size();
  }

  std::size_t capacity_;
  std::size_t hand_ = 0;
  std::vector<Slot> slots_;
  std::unordered_map<K, std::size_t> index_;  // key -> slot index
};

}  // namespace tempura
