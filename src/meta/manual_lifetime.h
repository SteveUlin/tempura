#pragma once

// Manual Lifetime Management - Raw storage with placement new helpers

#include <cassert>
#include <cstddef>
#include <new>
#include <type_traits>
#include <utility>

namespace tempura {

// ManualLifetime<T> provides storage for T with deferred
// construction/destruction.
template <typename T>
class ManualLifetime {
 public:
  ManualLifetime() noexcept = default;
  // Does NOT destroy - you must call destruct()
  ~ManualLifetime() noexcept = default;

  // Non-copyable, non-moveable (raw storage shouldn't be relocated)
  ManualLifetime(const ManualLifetime&) = delete;
  auto operator=(const ManualLifetime&) -> ManualLifetime& = delete;
  ManualLifetime(ManualLifetime&&) = delete;
  auto operator=(ManualLifetime&&) -> ManualLifetime& = delete;

  // Construct T in-place from constructor arguments.
  //
  // Use this when you have constructor arguments, not a factory function:
  //   storage.construct(42, "hello");  // Calls T(int, const char*)
  //
  // PRECONDITION:
  //   Storage is not currently constructed (your responsibility)
  // POSTCONDITION:
  //   Storage contains a constructed T
  template <typename... Args>
  void construct(Args&&... args) noexcept(
      std::is_nothrow_constructible_v<T, Args...>) {
    ::new (static_cast<void*>(&storage_)) T(std::forward<Args>(args)...);
  }

  // Construct T in-place from a factory function.
  //
  // REQUIREMENT:
  //   - Callable must return exactly T (not T&, not convertible-to-T)
  // PRECONDITION:
  //   - Storage is not currently constructed (your responsibility)
  // POSTCONDITION:
  //   - Storage contains a constructed T
  template <typename Func>
  void constructWith(Func&& func) noexcept(
      noexcept(T(std::forward<Func>(func)()))) {
    ::new (static_cast<void*>(&storage_)) T(std::forward<Func>(func)());
  }

  // Destroy the contained object.
  //
  // PRECONDITION:
  //   - Storage contains a constructed T (your responsibility)
  // POSTCONDITION:
  //   - Storage is unconstructed (ready for reuse or destruction)
  //
  // DANGER:
  //   - Calling this on unconstructed storage is undefined behavior.
  //   - Not calling this before storage destruction leaks the object.
  void destruct() noexcept(std::is_nothrow_destructible_v<T>) { get().~T(); }

  // Access the contained object.
  //
  // PRECONDITION:
  //   - Storage contains a constructed T (your responsibility)
  //
  // DANGER:
  //   - Accessing unconstructed storage is undefined behavior.
  auto get() noexcept -> T& {
    return *std::launder(reinterpret_cast<T*>(&storage_));
  }

  auto get() const noexcept -> const T& {
    return *std::launder(reinterpret_cast<const T*>(&storage_));
  }

  auto operator->() noexcept -> T* { return &get(); }
  auto operator->() const noexcept -> const T* { return &get(); }
  auto operator*() noexcept -> T& { return get(); }
  auto operator*() const noexcept -> const T& { return get(); }

 private:
  alignas(T) std::byte storage_[sizeof(T)];
};

}  // namespace tempura
