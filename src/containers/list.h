#pragma once

#include <cstddef>
#include <initializer_list>
#include <iterator>

namespace tempura {

template <typename T>
class List {
public:
  struct Node {
    T value;
    Node* next = nullptr;
    Node* prev = nullptr;
  };

  class Iterator {
  public:
    using difference_type = std::ptrdiff_t;
    using value_type = T;

    Iterator(List& list, Node* node) : list_{list}, node_{node} {}
    Iterator(const Iterator& other) : list_{other.list_}, node_{other.node} {}
    Iterator(Iterator&& other) noexcept
        : list_{other.list_}, node_{other.node} {}

    auto operator=(const Iterator&) -> Iterator& = default;
    auto operator=(Iterator&&) -> Iterator& = default;

    auto operator*() const -> value_type& {
      return node_->value;
    }

    auto operator++() -> Iterator& {
      node_ = node_->next;
      return *this;
    }

    auto operator++(int) -> Iterator {
      Iterator copy = *this;
      ++*this;
      return copy;
    }

    auto operator--() -> Iterator& {
      if (node_ == nullptr) {
        node_ = list_.tail_;
        return *this;
      }
      node_ = node_->prev;
      return *this;
    }

    auto operator--(int) -> Iterator {
      Iterator copy = *this;
      --*this;
      return copy;
    }

    auto operator==(const Iterator& other) const -> bool {
      return node_ == other.node_;
    }

  private:
    List& list_;
    Node* node_;
  };
  static_assert(std::bidirectional_iterator<Iterator>);

  List() = default;

  List(const List& other) {
    Node* itr = other.head_;
    while (itr != nullptr) {
      pushBack(itr->value);
      itr = itr->next;
    }
  };

  List(List&& other) noexcept {
    Node* itr = other.head_;
    while (itr != nullptr) {
      auto next = itr->next;
      itr->next = nullptr;
      itr->prev = nullptr;
      pushBack(itr);
      itr = next;
    }
    other.size_ = 0;
  }

  auto operator=(const List& other) -> List& {
    if (this == &other) {
      return *this;
    }

    while (tail_ != nullptr) {
      Node* old = tail_->prev;
      tail_ = tail_->prev;
      delete old;
    }

    Node* itr = other.head_;
    while (itr != nullptr) {
      pushBack(itr->value);
      itr = itr->next;
    }
    return *this;
  };

  auto operator=(List&& other) noexcept -> List& {
    if (this == &other) {
      return *this;
    }
    while (tail_ != nullptr) {
      Node* old = tail_->prev;
      tail_ = tail_->prev;
      delete old;
    }

    Node* itr = other.head_;
    while (itr != nullptr) {
      auto next = itr->next;
      itr->next = nullptr;
      itr->prev = nullptr;
      pushBack(itr);
      itr = next;
    }
    other.size_ = 0;
    return *this;
  };

  List(std::initializer_list<T> init) {
    for (auto&& value : init) {
      pushBack(std::move(value));
    }
  }

  ~List() {
    while (tail_ != nullptr) {
      Node* old = tail_->prev;
      tail_ = tail_->prev;
      delete old;
    }
  }

  void pushBack(Node* node) {
    if (head_ == nullptr) {
      head_ = node;
      tail_ = head_;
    } else {
      tail_->next = node;
      node->prev = tail_;
      tail_ = tail_->next;
    }
    ++size_;
  }

  void pushBack(T value) {
    pushBack(new Node{std::move(value)});
  }

  void popBack() {
    Node* old = tail_;
    tail_ = tail_->prev.get();
    tail_->next = nullptr;
    delete old;
    --size_;
  }

  void pushFront(Node* node) {
    if (head_ == nullptr) {
      head_ = node;
      tail_ = head_;
    } else {
      head_->prev = node;
      node->next= head_;
      head_ = node;
    }
    ++size_;
  }

  void pushFront(T value) {
    pushFront(new Node{std::move(value)});
  }

  void popFront() {
    Node* old = head_;
    head_ = head_->next;
    head_->prev = nullptr;
    delete old;
    --size_;
  }

  [[nodiscard]] auto size() const -> size_t {
    return size_;
  }

  auto begin() -> Iterator {
    return Iterator{*this, head_};
  }

  auto end() -> Iterator {
    return Iterator{*this, nullptr};
  }

private:
  size_t size_ = 0;

  Node* head_ = nullptr;
  Node* tail_ = nullptr;
};

}  // namespace tempura
