#pragma once

#include <mutex>
#include <condition_variable>
#include <queue>

namespace tempura {

// ThreadSafeQueue is a simple thread-safe queue implementation using condition
// variables.
template <typename T>
class ThreadSafeQueue {
 public:
  ThreadSafeQueue() = default;

  // Disallow copy and move
  ThreadSafeQueue(const ThreadSafeQueue&) = delete;
  ThreadSafeQueue(ThreadSafeQueue&&) = delete;
  auto operator=(const ThreadSafeQueue&) -> ThreadSafeQueue& = delete;
  auto operator=(ThreadSafeQueue&&) -> ThreadSafeQueue& = delete;

  void push(T value) {
    std::lock_guard<std::mutex> lock{mutex_};
    queue_.push(value);
    cond_var_.notify_one();
  }

  void waitAndPop(T& value) {
    std::unique_lock<std::mutex> lock{mutex_};
    cond_var_.wait(lock, [this] { return !queue_.empty(); });
    value = queue_.front();
    queue_.pop();
  }

  auto tryPop(T& value) -> bool {
    std::lock_guard<std::mutex> lock{mutex_};
    if (queue_.empty()) {
      return false;  // Queue is empty
    }
    value = queue_.front();
    queue_.pop();
    return true;
  }

 private:
  mutable std::mutex mutex_;
  std::condition_variable cond_var_;
  std::queue<T> queue_;
};
} // namespace tempura
