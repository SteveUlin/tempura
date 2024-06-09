#include <array>
#include <atomic>
#include <mutex>
#include <optional>
#include <new>

namespace tempura {

// A simple thread-safe lock-free single producer single consumer queue
// Based on https://www.youtube.com/watch?v=K3P_Lmq6pw0
//
// Uses a single fix-sized ring buffer
//
// x-x-o-o-o-o-x-x
//           ^ Push Cursor
//     ^ Pop Cursor
//
// o-o-x-x-x-x-o-o
//   ^ Push Cursor
//             ^ Pop Cursor
template<typename T, size_t N>
class FIFOQueue {
public:
  constexpr static auto capacity() -> size_t {
    return N;
  }

  auto size() -> size_t {
    return (push_cursor_ - pop_cursor_) % N;
  }

  auto full() -> bool {
    return push_cursor_ - pop_cursor_ == N;
  }

  auto empty() -> bool {
    return push_cursor_ == pop_cursor_;
  }

  template <typename U>
  requires std::convertible_to<U, T>
  auto push(U&& value) -> bool {
    auto push_cursor = push_cursor_.load(std::memory_order_relaxed);
    if (push_cursor - cached_pop_cursor_ == N) {
      cached_pop_cursor_ = pop_cursor_.load(std::memory_order_acquire);
      if (push_cursor - cached_pop_cursor_ == N) {
        return false;
      }
    }
    buffer_[push_cursor_ % N] = value;
    push_cursor_.store(push_cursor_ + 1, std::memory_order_release);
    return true;
  }

  auto pop() -> std::optional<T> {
    auto pop_cursor = pop_cursor_.load(std::memory_order_relaxed);
    if (cached_push_cursor_ == pop_cursor) {
      cached_push_cursor_ = push_cursor_.load(std::memory_order_acquire);
      if (cached_push_cursor_ == pop_cursor) {
        return std::nullopt;
      }
    }
    std::optional<T> out = std::move(buffer_[pop_cursor_ % N]);
    pop_cursor_.store(pop_cursor_ + 1, std::memory_order_release);
    return out;
  }

private:
  std::array<T, N> buffer_;
  alignas(128) std::atomic<size_t> push_cursor_ = 0;
  alignas(128) size_t cached_push_cursor_ = 0;
  alignas(128) std::atomic<size_t> pop_cursor_ = 0;
  alignas(128) size_t cached_pop_cursor_ = 0;
  // Buffer to prevent other data getting put on the same cache
  // line as the above atomic
  std::array<std::byte, 128 - sizeof(size_t)> buf_ = {};
  // std::atomic<size_t> push_cursor_ = 0;
  // std::atomic<size_t> pop_cursor_ = 0;
};

}
