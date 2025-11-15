// Shared test helpers for task library tests

#pragma once

#include <string>
#include <tuple>
#include <utility>

namespace tempura {

// ============================================================================
// Custom Error Sender Types (for testing variadic error types)
// ============================================================================

// Sender with custom error types (string, int)
class CustomErrorSender1 {
 public:
  using ValueTypes = std::tuple<int>;
  using ErrorTypes = std::tuple<std::string, int>;

  template <typename R>
  auto connect(R&& receiver) && {
    class OpState {
     public:
      OpState(R r) : receiver_(std::move(r)) {}
      void start() noexcept {
        receiver_.setError(std::string{"error message"}, 404);
      }
     private:
      R receiver_;
    };
    return OpState{std::forward<R>(receiver)};
  }
};

// Sender with different custom error types (double, string)
class CustomErrorSender2 {
 public:
  using ValueTypes = std::tuple<int>;
  using ErrorTypes = std::tuple<double, std::string>;

  template <typename R>
  auto connect(R&& receiver) && {
    class OpState {
     public:
      OpState(R r) : receiver_(std::move(r)) {}
      void start() noexcept {
        receiver_.setError(3.14, std::string{"pi error"});
      }
     private:
      R receiver_;
    };
    return OpState{std::forward<R>(receiver)};
  }
};

// Multi-error type sender (for static testing)
class MultiErrorSender {
 public:
  using ValueTypes = std::tuple<>;
  using ErrorTypes = std::tuple<int, double, std::string>;

  template <typename R>
  auto connect(R&&) && {
    struct OpState { void start() noexcept {} };
    return OpState{};
  }
};

// Move-only type for testing
struct MoveOnly {
  explicit MoveOnly(int v) : value(v) {}
  MoveOnly(const MoveOnly&) = delete;
  MoveOnly(MoveOnly&&) = default;
  int value;
};

}  // namespace tempura
