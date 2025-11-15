#pragma once

#include <mutex>
#include <cassert>
#include <tuple>

// Guarded
// RAII wrapper for a value that is guarded by a mutex.
//
// Inspired by folly::Synchronized
// https://github.com/facebook/folly/blob/main/folly/docs/Synchronized.md
// but this class is greatly simplified:
//   - No reader/writer locks
//   - Fewer methods

namespace tempura {

template <typename T, typename Mutex = std::mutex>
class GuardedHandle {
 public:
  GuardedHandle(T& value, std::unique_lock<Mutex> lock)
      : value_{&value}, lock_{std::move(lock)} {}

  GuardedHandle(const GuardedHandle&) = delete;
  auto operator=(const GuardedHandle&) -> GuardedHandle& = delete;

  GuardedHandle(GuardedHandle&& other) noexcept
      : value_{other.value_}, lock_{std::move(other.lock_)} {}

  auto operator=(GuardedHandle&& other) noexcept -> GuardedHandle& {
    value_ = other.value_;
    lock_ = std::move(other.lock_);
    return *this;
  }

  auto get() -> T& { return *value_; }
  auto get() const -> const T& { return *value_; }

  auto operator*() -> T& { return *value_; }
  auto operator*() const -> const T& { return *value_; }
  auto operator->() -> T* { return value_; }
  auto operator->() const -> const T* { return value_; }

  auto owns_lock() const -> bool { return lock_.owns_lock(); }
  explicit operator bool() const { return lock_.owns_lock(); }

 private:
  T* value_;
  mutable std::unique_lock<Mutex> lock_;
};

template <typename T, typename Mutex = std::mutex>
class Guarded {
 public:
  // Copy and move operations deleted.
  // Copying/moving acquires locks, which can deadlock and introduces hidden
  // blocking. Moving a shared synchronization point is almost always a bug.
  // If you need to copy the value, use: T copy = *guarded.acquire();
  Guarded(const Guarded&) = delete;
  auto operator=(const Guarded&) -> Guarded& = delete;
  Guarded(Guarded&&) = delete;
  auto operator=(Guarded&&) -> Guarded& = delete;

  template <typename... Args>
  explicit Guarded(Args&&... args) : value_{std::forward<Args>(args)...} {}

  template <typename... Args>
  [[nodiscard]] auto acquire(Args... args) -> GuardedHandle<T, Mutex> {
    return GuardedHandle{value_, std::unique_lock{mutex_, args...}};
  }
  template <typename... Args>
  [[nodiscard]] auto acquire(Args... args) const
      -> GuardedHandle<const T, Mutex> {
    return GuardedHandle{value_, std::unique_lock{mutex_, args...}};
  }

  template <typename F>
  auto withLock(F&& func) {
    return func(*acquire());
  }

  template <typename F>
  auto withLock(F&& func) const {
    return func(*acquire());
  }

 private:
  mutable Mutex mutex_;
  T value_;
};

}  // namespace tempura
