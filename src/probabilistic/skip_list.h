#pragma once

#include <cassert>
#include <cstddef>
#include <initializer_list>
#include <iterator>
#include <random>
#include <utility>
#include <vector>

namespace tempura {

// ============================================================================
// Skip List
// ============================================================================
//
// A skip list is a probabilistic alternative to balanced trees. It maintains
// sorted elements in O(log n) average time for search, insert, and delete,
// but with a simpler implementation than red-black or AVL trees.
//
// STRUCTURE
// ---------
// Imagine a sorted linked list. Searching is O(n) because you traverse every
// node. A skip list adds "express lanes" - higher levels that skip over nodes:
//
//   Level 3:  HEAD ─────────────────────────────────────> 50 ───────────> NIL
//   Level 2:  HEAD ──────────> 20 ──────────────────────> 50 ───────────> NIL
//   Level 1:  HEAD ───> 10 ──> 20 ──────────> 40 ──────> 50 ──> 60 ────> NIL
//   Level 0:  HEAD -> 5 -> 10 -> 20 -> 30 -> 40 -> 50 -> 60 -> 70 -> 80 -> NIL
//
// To find 40: start at HEAD level 3, go right until you'd overshoot, drop down,
// repeat. You skip most nodes, giving O(log n) expected time.
//
// Each node's height is chosen randomly at insertion: flip a coin repeatedly,
// and the number of heads (+1) is your level. This means ~50% of nodes are
// level 1, ~25% level 2, ~12.5% level 3, etc. - a geometric distribution that
// naturally creates the sparse upper levels needed for fast traversal.
//
// WHY USE A SKIP LIST?
// --------------------
// vs Balanced Trees (std::set, std::map):
//   ✓ Simpler to implement correctly - no rotations or color flipping
//   ✓ Lock-free versions are easier to build (each level is independent)
//   ✗ Slightly more memory per node (multiple pointers vs 2-3)
//   ✗ Worst case is O(n) not O(log n) - though astronomically unlikely
//
// vs Hash Tables (std::unordered_set):
//   ✓ Maintains sorted order - can iterate min-to-max, do range queries
//   ✓ Supports lowerBound/upperBound operations
//   ✓ No rehashing pauses, memory usage grows smoothly
//   ✗ O(log n) vs O(1) average lookup - hash tables win for pure key lookup
//
// WHEN TO CHOOSE A SKIP LIST
// --------------------------
// • You need sorted order AND fast insert/delete (not just lookup)
// • You want range queries: "all elements between A and B"
// • You're building concurrent data structures (lock-free skip lists are practical)
// • You want simpler code than a balanced tree without sacrificing performance
// • You can tolerate probabilistic (not worst-case) guarantees
//
// TEMPLATE PARAMETERS
// -------------------
//   Key         - Element type, must support operator<
//   MaxLevel    - Maximum height (default 16 handles ~65k elements optimally)
//   Probability - Promotion probability as percentage (default 50 = p=0.5)
//                 Higher p = taller nodes = faster search but more memory
//
// COMPLEXITY
// ----------
//   insert, erase, find, contains:  O(log n) average, O(n) worst
//   lowerBound, upperBound:         O(log n) average
//   iteration (begin to end):       O(n)
//   Space:                          O(n) average (each node has ~2 pointers on average with p=0.5)
//
template <typename Key, std::size_t MaxLevel = 16, std::size_t Probability = 50>
class SkipList {
  static_assert(MaxLevel > 0, "MaxLevel must be positive");
  static_assert(Probability > 0 && Probability < 100,
                "Probability must be in (0, 100)");

public:
  struct Node {
    Key key;
    std::vector<Node*> forward;  // forward[i] = next node at level i

    explicit Node(Key k, std::size_t level)
        : key{std::move(k)}, forward(level, nullptr) {}

    // Sentinel node constructor
    explicit Node(std::size_t level) : key{}, forward(level, nullptr) {}
  };

  class Iterator {
  public:
    using difference_type = std::ptrdiff_t;
    using value_type = const Key;
    using pointer = const Key*;
    using reference = const Key&;
    using iterator_category = std::forward_iterator_tag;

    Iterator() : node_{nullptr} {}
    explicit Iterator(Node* node) : node_{node} {}

    auto operator*() const -> reference { return node_->key; }
    auto operator->() const -> pointer { return &node_->key; }

    auto operator++() -> Iterator& {
      node_ = node_->forward[0];
      return *this;
    }

    auto operator++(int) -> Iterator {
      Iterator tmp = *this;
      ++*this;
      return tmp;
    }

    auto operator==(const Iterator& other) const -> bool {
      return node_ == other.node_;
    }

  private:
    Node* node_;
  };
  static_assert(std::forward_iterator<Iterator>);

  SkipList() : head_{new Node{MaxLevel}}, level_{1}, size_{0} {}

  SkipList(std::initializer_list<Key> init) : SkipList{} {
    for (auto&& key : init) {
      insert(key);
    }
  }

  ~SkipList() {
    Node* current = head_;
    while (current != nullptr) {
      Node* next = current->forward[0];
      delete current;
      current = next;
    }
  }

  // No copy (would need to deep-copy the structure)
  SkipList(const SkipList&) = delete;
  auto operator=(const SkipList&) -> SkipList& = delete;

  SkipList(SkipList&& other) noexcept
      : head_{other.head_},
        level_{other.level_},
        size_{other.size_},
        rng_{other.rng_} {
    other.head_ = new Node{MaxLevel};
    other.level_ = 1;
    other.size_ = 0;
  }

  auto operator=(SkipList&& other) noexcept -> SkipList& {
    if (this != &other) {
      // Clean up current
      Node* current = head_;
      while (current != nullptr) {
        Node* next = current->forward[0];
        delete current;
        current = next;
      }

      // Take ownership
      head_ = other.head_;
      level_ = other.level_;
      size_ = other.size_;
      rng_ = other.rng_;

      // Reset other
      other.head_ = new Node{MaxLevel};
      other.level_ = 1;
      other.size_ = 0;
    }
    return *this;
  }

  // Insert a key. Returns true if inserted, false if key already exists.
  auto insert(const Key& key) -> bool {
    std::vector<Node*> update(MaxLevel);
    Node* current = head_;

    // Find position at each level
    for (std::size_t i = level_; i-- > 0;) {
      while (current->forward[i] != nullptr &&
             current->forward[i]->key < key) {
        current = current->forward[i];
      }
      update[i] = current;
    }

    current = current->forward[0];

    // Key already exists
    if (current != nullptr && current->key == key) {
      return false;
    }

    // Generate random level for new node
    std::size_t new_level = randomLevel();

    // If new level exceeds current list level, update head pointers
    if (new_level > level_) {
      for (std::size_t i = level_; i < new_level; ++i) {
        update[i] = head_;
      }
      level_ = new_level;
    }

    // Create and insert new node
    Node* new_node = new Node{key, new_level};
    for (std::size_t i = 0; i < new_level; ++i) {
      new_node->forward[i] = update[i]->forward[i];
      update[i]->forward[i] = new_node;
    }

    ++size_;
    return true;
  }

  // Remove a key. Returns true if found and removed.
  auto erase(const Key& key) -> bool {
    std::vector<Node*> update(MaxLevel);
    Node* current = head_;

    // Find position at each level
    for (std::size_t i = level_; i-- > 0;) {
      while (current->forward[i] != nullptr &&
             current->forward[i]->key < key) {
        current = current->forward[i];
      }
      update[i] = current;
    }

    current = current->forward[0];

    // Key not found
    if (current == nullptr || current->key != key) {
      return false;
    }

    // Remove node from each level
    for (std::size_t i = 0; i < level_; ++i) {
      if (update[i]->forward[i] != current) {
        break;
      }
      update[i]->forward[i] = current->forward[i];
    }

    delete current;

    // Reduce level if necessary
    while (level_ > 1 && head_->forward[level_ - 1] == nullptr) {
      --level_;
    }

    --size_;
    return true;
  }

  // Check if key exists
  auto contains(const Key& key) const -> bool {
    return find(key) != end();
  }

  // Find a key. Returns end() if not found.
  auto find(const Key& key) const -> Iterator {
    Node* current = head_;

    for (std::size_t i = level_; i-- > 0;) {
      while (current->forward[i] != nullptr &&
             current->forward[i]->key < key) {
        current = current->forward[i];
      }
    }

    current = current->forward[0];

    if (current != nullptr && current->key == key) {
      return Iterator{current};
    }
    return end();
  }

  // Return iterator to first element >= key
  auto lowerBound(const Key& key) const -> Iterator {
    Node* current = head_;

    for (std::size_t i = level_; i-- > 0;) {
      while (current->forward[i] != nullptr &&
             current->forward[i]->key < key) {
        current = current->forward[i];
      }
    }

    return Iterator{current->forward[0]};
  }

  // Return iterator to first element > key
  auto upperBound(const Key& key) const -> Iterator {
    Node* current = head_;

    for (std::size_t i = level_; i-- > 0;) {
      while (current->forward[i] != nullptr &&
             current->forward[i]->key <= key) {
        current = current->forward[i];
      }
    }

    return Iterator{current->forward[0]};
  }

  auto begin() const -> Iterator { return Iterator{head_->forward[0]}; }
  auto end() const -> Iterator { return Iterator{nullptr}; }

  auto size() const -> std::size_t { return size_; }
  auto empty() const -> bool { return size_ == 0; }

  void clear() {
    Node* current = head_->forward[0];
    while (current != nullptr) {
      Node* next = current->forward[0];
      delete current;
      current = next;
    }

    for (std::size_t i = 0; i < MaxLevel; ++i) {
      head_->forward[i] = nullptr;
    }
    level_ = 1;
    size_ = 0;
  }

  // Get current number of levels in use (for testing/debugging)
  auto currentLevel() const -> std::size_t { return level_; }

private:
  // Generate random level using geometric distribution
  // Each level has Probability/100 chance of being promoted
  auto randomLevel() -> std::size_t {
    std::size_t lvl = 1;
    std::uniform_int_distribution<int> dist{0, 99};
    while (dist(rng_) < static_cast<int>(Probability) && lvl < MaxLevel) {
      ++lvl;
    }
    return lvl;
  }

  Node* head_;                 // Sentinel head node
  std::size_t level_;          // Current max level in use
  std::size_t size_;           // Number of elements
  std::mt19937 rng_{std::random_device{}()};
};

}  // namespace tempura
