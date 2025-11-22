#pragma once

#include "schedulers.h"
#include "stop_token.h"

namespace tempura {

// ============================================================================
// Environment - Generic composable query interface
// ============================================================================
//
// Environments are composable objects that hold properties (like schedulers,
// stop tokens, allocators, etc.) and support querying via type tags.
//
// Design:
//   • EmptyEnv provides defaults for all queries
//   • Prop<Parent, Tag, Value> wraps a parent and adds/overrides one property
//   • Query tags (GetStopTokenTag, etc.) identify properties
//   • Environments compose via inheritance - queries fall through to parent
//
// Usage:
//   auto env = withScheduler(withStopToken(token), sched);
//   auto sched = get_scheduler(env);
//   auto token = get_stop_token(env);
//
// ============================================================================

// Query type tags - identify what property is being queried
struct GetStopTokenTag {};
struct GetSchedulerTag {};

// Empty environment - provides defaults for all queries
struct EmptyEnv {
  constexpr auto query(GetStopTokenTag) const noexcept {
    return NeverStopToken{};
  }
  constexpr auto query(GetSchedulerTag) const noexcept {
    return InlineScheduler{};
  }
};

// Generic property wrapper - adds one property to an environment
// Parent: the parent environment (supports composition via inheritance)
// Tag: query tag that identifies this property
// Value: the value to return for this query
template <typename Parent, typename Tag, typename Value>
class Prop : public Parent {
  [[no_unique_address]] Value value_;

 public:
  explicit constexpr Prop(Parent parent, Value v) noexcept
      : Parent(static_cast<Parent&&>(parent)), value_(static_cast<Value&&>(v)) {}

  // Inherit parent's query() overloads (fall-through for unmatched queries)
  using Parent::query;

  // Override query for this tag - return by value to strip const/ref
  constexpr Value query(Tag) const noexcept { return value_; }
};

// ============================================================================
// Builder functions - compose environments fluently
// ============================================================================

// Add stop token to an environment
template <typename Env, StopToken T>
constexpr auto withStopToken(Env env, T token) noexcept {
  return Prop<Env, GetStopTokenTag, T>{static_cast<Env&&>(env),
                                       static_cast<T&&>(token)};
}

// Add stop token to empty environment
template <StopToken T>
constexpr auto withStopToken(T token) noexcept {
  return Prop<EmptyEnv, GetStopTokenTag, T>{EmptyEnv{},
                                            static_cast<T&&>(token)};
}

// Add scheduler to an environment
template <typename Env, Scheduler S>
constexpr auto withScheduler(Env env, S sched) noexcept {
  return Prop<Env, GetSchedulerTag, S>{static_cast<Env&&>(env),
                                       static_cast<S&&>(sched)};
}

// Add scheduler to empty environment
template <Scheduler S>
constexpr auto withScheduler(S sched) noexcept {
  return Prop<EmptyEnv, GetSchedulerTag, S>{EmptyEnv{}, static_cast<S&&>(sched)};
}

// ============================================================================
// Customization point objects (CPOs)
// ============================================================================

// Queries a receiver for its environment
inline constexpr struct GetEnvFn {
  template <typename R>
  constexpr auto operator()(const R& r) const noexcept {
    if constexpr (requires { r.get_env(); }) {
      return r.get_env();
    } else {
      return EmptyEnv{};
    }
  }
} get_env{};

// Queries an environment for its stop token
inline constexpr struct GetStopTokenFn {
  template <typename Env>
  constexpr auto operator()(const Env& env) const noexcept {
    if constexpr (requires { env.query(GetStopTokenTag{}); }) {
      return env.query(GetStopTokenTag{});
    } else {
      return NeverStopToken{};
    }
  }
} get_stop_token{};

// Queries an environment for its scheduler
inline constexpr struct GetSchedulerFn {
  template <typename Env>
  constexpr auto operator()(const Env& env) const noexcept {
    if constexpr (requires { env.query(GetSchedulerTag{}); }) {
      return env.query(GetSchedulerTag{});
    } else {
      return InlineScheduler{};
    }
  }
} get_scheduler{};

}  // namespace tempura
