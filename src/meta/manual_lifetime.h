#pragma once

// Manual Lifetime Management - Deferred construction/destruction via union
//
// Uses a union to provide uninitialized storage that's constexpr-compatible.
// The union approach avoids reinterpret_cast (forbidden in constexpr) and
// works with std::construct_at/std::destroy_at which are constexpr in C++20+.

#include <memory>
#include <type_traits>
#include <utility>

namespace tempura {

// ManualLifetime<T> provides storage for T with deferred
// construction/destruction. Fully constexpr-compatible.
template <typename T>
class ManualLifetime {
 public:
  constexpr ManualLifetime() noexcept {}
  // Does NOT destroy - you must call destruct()
  constexpr ~ManualLifetime() noexcept = default;

  // Non-copyable, non-moveable (raw storage shouldn't be relocated)
  ManualLifetime(const ManualLifetime&) = delete;
  auto operator=(const ManualLifetime&) -> ManualLifetime& = delete;
  ManualLifetime(ManualLifetime&&) = delete;
  auto operator=(ManualLifetime&&) -> ManualLifetime& = delete;

  // Construct T in-place from constructor arguments.
  //
  // PRECONDITION: storage is not currently constructed (your responsibility)
  template <typename... Args>
  constexpr void construct(Args&&... args) noexcept(
      std::is_nothrow_constructible_v<T, Args...>) {
    std::construct_at(&storage_.value_, std::forward<Args>(args)...);
  }

  // Construct T from the return value of a zero-argument factory callable.
  // Placement new triggers guaranteed copy elision when factory() returns a
  // prvalue — the only way to in-place-construct non-moveable operation states.
  //
  // PRECONDITION: storage is not currently constructed (your responsibility)
  template <typename F>
  void constructWith(F&& factory) {
    ::new (static_cast<void*>(&storage_.value_)) T(std::forward<F>(factory)());
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
  constexpr void destruct() noexcept(std::is_nothrow_destructible_v<T>) {
    std::destroy_at(&storage_.value_);
  }

  // Access the contained object.
  //
  // PRECONDITION:
  //   - Storage contains a constructed T (your responsibility)
  //
  // DANGER:
  //   - Accessing unconstructed storage is undefined behavior.
  constexpr auto get() noexcept -> T& { return storage_.value_; }
  constexpr auto get() const noexcept -> const T& { return storage_.value_; }

  constexpr auto operator->() noexcept -> T* { return &storage_.value_; }
  constexpr auto operator->() const noexcept -> const T* { return &storage_.value_; }
  constexpr auto operator*() noexcept -> T& { return storage_.value_; }
  constexpr auto operator*() const noexcept -> const T& { return storage_.value_; }

 private:
  // Union with non-trivial T would have deleted destructor by default.
  // We provide an empty destructor since lifetime is managed manually.
  union Storage {
    constexpr Storage() noexcept {}
    constexpr ~Storage() noexcept {}
    T value_;
  } storage_;
};

}  // namespace tempura
