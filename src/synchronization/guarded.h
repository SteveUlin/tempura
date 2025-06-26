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
  };

  auto lock() const -> void { lock_.lock(); }
  auto unlock() const -> void { lock_.unlock(); }
  [[nodiscard]] auto try_lock() const -> bool { return lock_.try_lock(); }

  auto get() -> T& { return *value_; }
  auto get() const -> const T& { return *value_; }

  auto operator*() -> T& { return *value_; }
  auto operator*() const -> const T& { return *value_; }
  auto operator->() -> T* { return value_; }
  auto operator->() const -> const T* { return value_; }

 private:
  T* value_;
  mutable std::unique_lock<Mutex> lock_;
};

template <typename T, typename Mutex = std::mutex>
class Guarded {
 public:
  Guarded(const Guarded& other) : value_{*other.acquire()} {}
  Guarded(Guarded&& other) noexcept : value_{std::move(*other.acquire())} {}
  auto operator=(const Guarded& other) -> Guarded& {
    if (this == &other) {
      return *this;
    }
    value_ = *other.acquire();
    return *this;
  }
  auto operator=(Guarded&& other) noexcept -> Guarded& {
    if (this == &other) {
      return *this;
    }
    value_ = std::move(*other.acquire());
    return *this;
  }

  template <typename... Args>
  explicit Guarded(Args&&... args) : value_{std::forward<Args>(args)...} {}

  auto operator=(const T& value) -> Guarded& { *acquire() = value; }
  auto operator=(T&& value) -> Guarded& { *acquire() = value; }

  auto lock() const -> void { mutex_.lock(); }
  auto unlock() const -> void { mutex_.unlock(); }
  [[nodiscard]] auto try_lock() const -> bool { return mutex_.try_lock(); }

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
  auto withLock(F&& func) -> void {
    return func(*acquire());
  }

  template <typename F>
  auto withLock(F&& func) const -> void {
    return func(*acquire());
  }

 private:
  mutable Mutex mutex_;
  T value_;
};

template <typename... Args, typename... Mutexes>
[[nodiscard]] auto acquire(Guarded<Args, Mutexes>&... args)
    -> std::tuple<GuardedHandle<Args, Mutexes>...> {
  std::tuple<GuardedHandle<Args, Mutexes>...> out{
      args.acquire(std::defer_lock)...};
  std::apply([](auto&... handles) { std::lock(handles...); }, out);
  return out;
}

template <typename... Args, typename... Mutexes, typename F>
[[nodiscard]] auto withLocks(Guarded<Args, Mutexes>&... args, F func) {
  auto handles = acquire(args...);
  return std::apply([&func](auto&... handles) { return func(handles...); },
                    handles);
}

}  // namespace tempura
