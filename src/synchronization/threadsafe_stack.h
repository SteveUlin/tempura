#include <memory>
#include <mutex>
#include <stack>
#include <type_traits>

// ThreadSafeStack is a simple thread-safe stack implementation. It protects
// the underlying stack with a mutex to ensure that only one thread can access
// the stack at a time.
//
// This class is thread-safe.
template <typename T>
class ThreadSafeStack {
 public:
  ThreadSafeStack() = default;

  // Disallow copy and move
  //   - Copying would require locking the other stack, which is a complex
  //     operation for a constructor.
  //   - If you lock and move, another thread might try to call a function
  //     on the moved-from stack. Which could lead to undefined behavior.
  ThreadSafeStack(const ThreadSafeStack&) = delete;
  ThreadSafeStack(ThreadSafeStack&&) = delete;
  auto operator=(const ThreadSafeStack&) -> ThreadSafeStack& = delete;
  auto operator=(ThreadSafeStack&&) -> ThreadSafeStack& = delete;

  auto push(T value) -> void {
    std::lock_guard<std::mutex> lock{mutex_};
    stack_.push(std::move(value));
  }

  auto pop(T& value) -> bool {
    std::lock_guard<std::mutex> lock{mutex_};
    if (stack_.empty()) {
      return false;  // Stack is empty
    }
    value = std::move(stack_.top());
    stack_.pop();
    return true;
  }

  auto empty() const -> bool {
    std::lock_guard<std::mutex> lock{mutex_};
    return stack_.empty();
  }

  auto size() const -> std::size_t {
    std::lock_guard<std::mutex> lock{mutex_};
    return stack_.size();
  }

 private:
  mutable std::mutex mutex_;
  std::stack<T> stack_;
};
