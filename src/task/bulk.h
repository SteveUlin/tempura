// BulkSender - execute a function for each index in a shape
//
// The bulk() algorithm executes a function for each element in a range.
// When the input sender completes with values, the function is called for
// each index from 0 to shape-1 with the index and the sender's values.
// After all invocations complete, the original values are forwarded.
//
// Semantics:
// - Input sender completes with values Args...
// - Function f(Index, Args...) is called for each index in [0, shape)
// - Original Args... are forwarded after all bulk operations complete
// - Errors and stopped signals pass through unchanged
//
// Example:
//   auto result = syncWait(
//       just(std::vector<int>{1, 2, 3})
//       | bulk(3, [](std::size_t i, auto& vec) {
//           vec[i] *= 2;  // Double each element
//         }));
//   // result contains {2, 4, 6}

#pragma once

#include <cstddef>
#include <functional>
#include <tuple>
#include <type_traits>
#include <utility>

#include "completion_signatures.h"
#include "concepts.h"

namespace tempura {

// Forward declarations
template <typename S, typename Shape, typename F>
class BulkSender;

template <typename S, typename Shape, typename F, typename R>
class BulkOperationState;

template <typename Shape, typename F, typename R>
class BulkReceiver;

// ============================================================================
// BulkReceiver - Receiver that executes bulk function on completion
// ============================================================================

template <typename Shape, typename F, typename R>
class BulkReceiver {
 public:
  BulkReceiver(Shape shape, F* func, R* receiver)
      : shape_(shape), func_(func), receiver_(receiver) {}

  template <typename... Args>
  void setValue(Args&&... args) noexcept {
    // Execute function for each index in [0, shape)
    for (Shape i = 0; i < shape_; ++i) {
      std::invoke(*func_, i, args...);
    }
    // Forward the original values
    receiver_->setValue(std::forward<Args>(args)...);
  }

  template <typename... ErrorArgs>
  void setError(ErrorArgs&&... args) noexcept {
    receiver_->setError(std::forward<ErrorArgs>(args)...);
  }

  void setStopped() noexcept { receiver_->setStopped(); }

 private:
  Shape shape_;
  F* func_;
  R* receiver_;
};

// ============================================================================
// BulkOperationState - Operation state for bulk execution
// ============================================================================

template <typename S, typename Shape, typename F, typename R>
class BulkOperationState {
 public:
  using InnerReceiver = BulkReceiver<Shape, F, R>;
  using InnerOpState =
      decltype(std::declval<S>().connect(std::declval<InnerReceiver>()));

  BulkOperationState(S sender, Shape shape, F func, R receiver)
      : shape_(shape),
        func_(std::move(func)),
        receiver_(std::move(receiver)),
        inner_op_(std::move(sender).connect(
            InnerReceiver{shape_, &func_, &receiver_})) {}

  // Non-copyable, non-movable
  BulkOperationState(const BulkOperationState&) = delete;
  auto operator=(const BulkOperationState&) = delete;
  BulkOperationState(BulkOperationState&&) = delete;
  auto operator=(BulkOperationState&&) = delete;

  void start() noexcept { inner_op_.start(); }

 private:
  Shape shape_;
  F func_;
  R receiver_;
  InnerOpState inner_op_;
};

// ============================================================================
// BulkSender - Sender that applies function to each index
// ============================================================================

template <typename S, typename Shape, typename F>
class BulkSender {
 public:
  // Completion signatures: same as underlying sender
  // (bulk forwards the original values after running the function)
  template <typename Env = EmptyEnv>
  using CompletionSignatures = GetCompletionSignaturesT<S, Env>;

  BulkSender(S sender, Shape shape, F func)
      : sender_(std::move(sender)),
        shape_(shape),
        func_(std::move(func)) {}

  template <typename R>
  auto connect(R&& receiver) && {
    return BulkOperationState<S, Shape, F, std::remove_cvref_t<R>>{
        std::move(sender_), shape_, std::move(func_),
        std::forward<R>(receiver)};
  }

 private:
  S sender_;
  Shape shape_;
  F func_;
};

template <typename S, typename Shape, typename F>
BulkSender(S, Shape, F)
    -> BulkSender<std::remove_cvref_t<S>, Shape, std::remove_cvref_t<F>>;

// Adaptor for pipe operator
template <typename Shape, typename F>
struct BulkAdaptor {
  Shape shape;
  F func;

  template <typename S>
  auto operator()(S&& sender) const {
    return BulkSender{std::forward<S>(sender), shape, func};
  }
};

// Helper function - three-argument form
template <Sender S, typename Shape, typename F>
auto bulk(S&& sender, Shape shape, F&& func) {
  return BulkSender{std::forward<S>(sender), shape, std::forward<F>(func)};
}

// Helper function - two-argument form for pipe syntax
template <typename Shape, typename F>
auto bulk(Shape shape, F&& func) {
  return BulkAdaptor<Shape, std::decay_t<F>>{shape, std::forward<F>(func)};
}

// Pipe operator
template <typename S, typename Shape, typename F>
auto operator|(S&& sender, const BulkAdaptor<Shape, F>& adaptor) {
  return adaptor(std::forward<S>(sender));
}

}  // namespace tempura
