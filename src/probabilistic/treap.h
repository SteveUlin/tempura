#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <iterator>
#include <random>
#include <utility>

namespace tempura {

// ============================================================================
// Treap (Randomized Binary Search Tree)
// ============================================================================
//
// A treap is a hybrid of a binary search tree and a heap that uses randomization
// to achieve expected O(log n) operations without complex rebalancing logic.
//
// THE KEY INSIGHT
// ---------------
// Each node has two values: a key (user-provided) and a priority (random).
// The structure maintains TWO invariants simultaneously:
//   1. BST property on keys: left.key < node.key < right.key
//   2. Max-heap property on priorities: node.priority >= children.priority
//
// Since priorities are random, the tree's shape is statistically equivalent to
// a random BST - which has expected height O(log n). No red-black rotations,
// no AVL balance factors, no splay operations. Just randomness.
//
// STRUCTURE EXAMPLE
// -----------------
//                    [30, p=95]
//                   /          |
//            [10, p=80]      [50, p=70]
//           /        |              |
//      [5, p=40]  [20, p=60]     [60, p=30]
//
// Reading keys: 5 < 10 < 20 < 30 < 50 < 60  (BST order ✓)
// Reading priorities: parent >= children    (heap order ✓)
//
// HOW ROTATIONS MAINTAIN BOTH PROPERTIES
// --------------------------------------
// After insertion or deletion, the heap property may be violated. We fix it
// with rotations that preserve BST order while bubbling high-priority nodes up:
//
//   Right rotation (when left child has higher priority):
//
//        y                x
//       / |   ---->      / |
//      x   C            A   y
//     / |                  / |
//    A   B                B   C
//
//   BST order preserved: A < x < B < y < C (unchanged)
//   Heap: x moves up (it had higher priority than y)
//
// WHY RANDOMIZED PRIORITIES WORK
// ------------------------------
// If we insert n keys with random priorities, the resulting tree has the same
// distribution as a BST built by inserting keys in a random order. Random BSTs
// have expected height O(log n) and expected depth of any node is O(log n).
//
// This gives us the simplicity of a BST with the performance of balanced trees.
// The catch: worst case is still O(n), but it's astronomically unlikely.
//
// COMPARISON WITH OTHER STRUCTURES
// --------------------------------
// vs Red-Black/AVL trees:
//   ✓ Much simpler to implement - no color/balance bookkeeping
//   ✓ Same expected O(log n) performance
//   ✗ Worst case O(n) vs guaranteed O(log n)
//   ✗ Slightly more memory (storing priorities)
//
// vs Skip Lists:
//   ✓ Lower memory overhead (no multiple forward pointers)
//   ✓ Cache-friendlier in-order traversal
//   ≈ Similar expected complexity
//   ✗ Skip lists easier to make lock-free
//
// WHEN TO USE A TREAP
// -------------------
// • You want balanced BST performance with simple code
// • You can tolerate probabilistic (not worst-case) guarantees
// • You need in-order iteration or range queries
// • You don't need lock-free concurrent access
//
// TEMPLATE PARAMETERS
// -------------------
//   Key - Element type, must support operator<
//
// COMPLEXITY
// ----------
//   insert, erase, find, contains:  O(log n) expected, O(n) worst
//   iteration (begin to end):       O(n)
//   Space:                          O(n)
//
template <typename Key>
class Treap {
 public:
  struct Node {
    Key key;
    std::uint64_t priority;  // Random priority for heap property
    Node* left;
    Node* right;
    Node* parent;  // For iteration

    explicit Node(Key k, std::uint64_t p, Node* par = nullptr)
        : key{std::move(k)},
          priority{p},
          left{nullptr},
          right{nullptr},
          parent{par} {}
  };

  class Iterator {
   public:
    using difference_type = std::ptrdiff_t;
    using value_type = const Key;
    using pointer = const Key*;
    using reference = const Key&;
    using iterator_category = std::bidirectional_iterator_tag;

    Iterator() : node_{nullptr} {}
    explicit Iterator(Node* node) : node_{node} {}

    auto operator*() const -> reference { return node_->key; }
    auto operator->() const -> pointer { return &node_->key; }

    auto operator++() -> Iterator& {
      node_ = successor(node_);
      return *this;
    }

    auto operator++(int) -> Iterator {
      Iterator tmp = *this;
      ++*this;
      return tmp;
    }

    auto operator--() -> Iterator& {
      node_ = predecessor(node_);
      return *this;
    }

    auto operator--(int) -> Iterator {
      Iterator tmp = *this;
      --*this;
      return tmp;
    }

    auto operator==(const Iterator& other) const -> bool {
      return node_ == other.node_;
    }

   private:
    // In-order successor: smallest node in right subtree, or first ancestor
    // where we came from the left
    static auto successor(Node* node) -> Node* {
      if (node == nullptr) {
        return nullptr;
      }

      if (node->right != nullptr) {
        // Go right, then all the way left
        node = node->right;
        while (node->left != nullptr) {
          node = node->left;
        }
        return node;
      }

      // Go up until we find an ancestor where we came from the left
      while (node->parent != nullptr && node == node->parent->right) {
        node = node->parent;
      }
      return node->parent;
    }

    // In-order predecessor: largest node in left subtree, or first ancestor
    // where we came from the right
    static auto predecessor(Node* node) -> Node* {
      if (node == nullptr) {
        return nullptr;
      }

      if (node->left != nullptr) {
        // Go left, then all the way right
        node = node->left;
        while (node->right != nullptr) {
          node = node->right;
        }
        return node;
      }

      // Go up until we find an ancestor where we came from the right
      while (node->parent != nullptr && node == node->parent->left) {
        node = node->parent;
      }
      return node->parent;
    }

    Node* node_;
  };
  static_assert(std::bidirectional_iterator<Iterator>);

  Treap() : root_{nullptr}, size_{0} {}

  Treap(std::initializer_list<Key> init) : Treap{} {
    for (auto&& key : init) {
      insert(key);
    }
  }

  ~Treap() { destroySubtree(root_); }

  // No copy (would need deep copy)
  Treap(const Treap&) = delete;
  auto operator=(const Treap&) -> Treap& = delete;

  Treap(Treap&& other) noexcept
      : root_{other.root_}, size_{other.size_}, rng_{std::move(other.rng_)} {
    other.root_ = nullptr;
    other.size_ = 0;
  }

  auto operator=(Treap&& other) noexcept -> Treap& {
    if (this != &other) {
      destroySubtree(root_);

      root_ = other.root_;
      size_ = other.size_;
      rng_ = std::move(other.rng_);

      other.root_ = nullptr;
      other.size_ = 0;
    }
    return *this;
  }

  // Insert a key. Returns true if inserted, false if key already exists.
  auto insert(const Key& key) -> bool {
    // Standard BST insertion with parent tracking
    Node* parent = nullptr;
    Node** current = &root_;

    while (*current != nullptr) {
      parent = *current;
      if (key < (*current)->key) {
        current = &(*current)->left;
      } else if ((*current)->key < key) {
        current = &(*current)->right;
      } else {
        // Key already exists
        return false;
      }
    }

    // Create new node with random priority
    std::uint64_t priority = rng_();
    *current = new Node{key, priority, parent};
    ++size_;

    // Restore heap property by rotating up
    bubbleUp(*current);
    return true;
  }

  // Remove a key. Returns true if found and removed.
  auto erase(const Key& key) -> bool {
    Node* node = findNode(key);
    if (node == nullptr) {
      return false;
    }

    // Rotate node down until it's a leaf
    sinkDown(node);

    // Now node is a leaf, just remove it
    if (node->parent == nullptr) {
      root_ = nullptr;
    } else if (node == node->parent->left) {
      node->parent->left = nullptr;
    } else {
      node->parent->right = nullptr;
    }

    delete node;
    --size_;
    return true;
  }

  // Check if key exists
  auto contains(const Key& key) const -> bool { return findNode(key) != nullptr; }

  // Find a key. Returns end() if not found.
  auto find(const Key& key) const -> Iterator {
    Node* node = findNode(key);
    if (node != nullptr) {
      return Iterator{node};
    }
    return end();
  }

  // Return iterator to first element >= key
  auto lowerBound(const Key& key) const -> Iterator {
    Node* result = nullptr;
    Node* current = root_;

    while (current != nullptr) {
      if (key < current->key) {
        result = current;  // Current is a candidate (>= key)
        current = current->left;
      } else if (current->key < key) {
        current = current->right;
      } else {
        // Exact match
        return Iterator{current};
      }
    }

    return Iterator{result};
  }

  // Return iterator to first element > key
  auto upperBound(const Key& key) const -> Iterator {
    Node* result = nullptr;
    Node* current = root_;

    while (current != nullptr) {
      if (key < current->key) {
        result = current;  // Current is a candidate (> key)
        current = current->left;
      } else {
        current = current->right;
      }
    }

    return Iterator{result};
  }

  auto begin() const -> Iterator {
    if (root_ == nullptr) {
      return end();
    }
    // Find minimum: go all the way left
    Node* node = root_;
    while (node->left != nullptr) {
      node = node->left;
    }
    return Iterator{node};
  }

  auto end() const -> Iterator { return Iterator{nullptr}; }

  auto size() const -> std::size_t { return size_; }
  auto empty() const -> bool { return size_ == 0; }

  void clear() {
    destroySubtree(root_);
    root_ = nullptr;
    size_ = 0;
  }

  // Get tree height (for testing/debugging)
  auto height() const -> std::size_t { return subtreeHeight(root_); }

 private:
  // Find node by key, return nullptr if not found
  auto findNode(const Key& key) const -> Node* {
    Node* current = root_;
    while (current != nullptr) {
      if (key < current->key) {
        current = current->left;
      } else if (current->key < key) {
        current = current->right;
      } else {
        return current;
      }
    }
    return nullptr;
  }

  // Restore heap property after insertion by rotating node up
  void bubbleUp(Node* node) {
    while (node->parent != nullptr && node->priority > node->parent->priority) {
      if (node == node->parent->left) {
        rotateRight(node->parent);
      } else {
        rotateLeft(node->parent);
      }
    }

    // Update root if needed
    if (node->parent == nullptr) {
      root_ = node;
    }
  }

  // Sink node down until it's a leaf (for deletion)
  void sinkDown(Node* node) {
    while (node->left != nullptr || node->right != nullptr) {
      // Rotate toward the child with higher priority
      if (node->left == nullptr) {
        rotateLeft(node);
      } else if (node->right == nullptr) {
        rotateRight(node);
      } else if (node->left->priority > node->right->priority) {
        rotateRight(node);
      } else {
        rotateLeft(node);
      }

      // Update root if needed
      if (node->parent != nullptr && node->parent->parent == nullptr) {
        root_ = node->parent;
      }
    }
  }

  // Right rotation: y becomes right child of x
  //       y            x
  //      / |   -->    / |
  //     x   C        A   y
  //    / |              / |
  //   A   B            B   C
  void rotateRight(Node* y) {
    Node* x = y->left;
    assert(x != nullptr);

    // Move B from x's right to y's left
    y->left = x->right;
    if (x->right != nullptr) {
      x->right->parent = y;
    }

    // Link x to y's parent
    x->parent = y->parent;
    if (y->parent == nullptr) {
      root_ = x;
    } else if (y == y->parent->left) {
      y->parent->left = x;
    } else {
      y->parent->right = x;
    }

    // Put y as x's right child
    x->right = y;
    y->parent = x;
  }

  // Left rotation: x becomes left child of y
  //     x              y
  //    / |    -->     / |
  //   A   y          x   C
  //      / |        / |
  //     B   C      A   B
  void rotateLeft(Node* x) {
    Node* y = x->right;
    assert(y != nullptr);

    // Move B from y's left to x's right
    x->right = y->left;
    if (y->left != nullptr) {
      y->left->parent = x;
    }

    // Link y to x's parent
    y->parent = x->parent;
    if (x->parent == nullptr) {
      root_ = y;
    } else if (x == x->parent->left) {
      x->parent->left = y;
    } else {
      x->parent->right = y;
    }

    // Put x as y's left child
    y->left = x;
    x->parent = y;
  }

  void destroySubtree(Node* node) {
    if (node == nullptr) {
      return;
    }
    destroySubtree(node->left);
    destroySubtree(node->right);
    delete node;
  }

  auto subtreeHeight(Node* node) const -> std::size_t {
    if (node == nullptr) {
      return 0;
    }
    return 1 + std::max(subtreeHeight(node->left), subtreeHeight(node->right));
  }

  Node* root_;
  std::size_t size_;
  std::mt19937_64 rng_{std::random_device{}()};
};

}  // namespace tempura
