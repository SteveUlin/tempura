// Shared test helpers for task library tests

#pragma once

#include <string>
#include <tuple>
#include <utility>

namespace tempura {

// ============================================================================
// Custom Error Sender Types (for testing variadic error types)
// ============================================================================

// Sender with custom error types (tuple of string and int)
class CustomErrorSender1 {
 public:
  template <typename Env = EmptyEnv>
  using CompletionSignatures = tempura::CompletionSignatures<
      SetValueTag(int),
      SetErrorTag(std::tuple<std::string, int>),
      SetStoppedTag()>;

  template <typename R>
  auto connect(R&& receiver) && {
    class OpState {
     public:
      OpState(R r) : receiver_(std::move(r)) {}
      void start() noexcept {
        receiver_.setError(std::make_tuple(std::string{"error message"}, 404));
      }
     private:
      R receiver_;
    };
    return OpState{std::forward<R>(receiver)};
  }
};

// Sender with different custom error types (tuple of double and string)
class CustomErrorSender2 {
 public:
  template <typename Env = EmptyEnv>
  using CompletionSignatures = tempura::CompletionSignatures<
      SetValueTag(int),
      SetErrorTag(std::tuple<double, std::string>),
      SetStoppedTag()>;

  template <typename R>
  auto connect(R&& receiver) && {
    class OpState {
     public:
      OpState(R r) : receiver_(std::move(r)) {}
      void start() noexcept {
        receiver_.setError(std::make_tuple(3.14, std::string{"pi error"}));
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
  template <typename Env = EmptyEnv>
  using CompletionSignatures = tempura::CompletionSignatures<
      SetErrorTag(int),
      SetErrorTag(double),
      SetErrorTag(std::string),
      SetStoppedTag()>;

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

// ============================================================================
// Stopped Sender (for testing stopped channel handling)
// ============================================================================

// Sender that always completes with stopped
class StoppedSender {
 public:
  template <typename Env = EmptyEnv>
  using CompletionSignatures = tempura::CompletionSignatures<
      SetValueTag(int),
      SetStoppedTag()>;

  template <typename R>
  auto connect(R&& receiver) && {
    class OpState {
     public:
      OpState(R r) : receiver_(std::move(r)) {}
      void start() noexcept { receiver_.setStopped(); }
     private:
      R receiver_;
    };
    return OpState{std::forward<R>(receiver)};
  }
};

}  // namespace tempura
