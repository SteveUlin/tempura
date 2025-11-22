#pragma once

#include "schedulers.h"
#include "stop_token.h"

namespace tempura {

// ============================================================================
// Environment - Query interface for receiver properties
// ============================================================================

// Empty environment (default)
struct EmptyEnv {
  static constexpr auto get_stop_token() noexcept { return NeverStopToken{}; }
  static constexpr auto get_scheduler() noexcept { return InlineScheduler{}; }
};

// Environment with stop token
template <StopToken Token>
class EnvWithStopToken {
  Token token_;

 public:
  explicit constexpr EnvWithStopToken(Token token) noexcept : token_(token) {}

  constexpr auto get_stop_token() const noexcept { return token_; }
};

// Environment with scheduler
template <Scheduler Sched>
class EnvWithScheduler {
  Sched scheduler_;

 public:
  explicit constexpr EnvWithScheduler(Sched sched) noexcept
      : scheduler_(sched) {}

  constexpr auto get_scheduler() const noexcept { return scheduler_; }
};

// Environment with both stop token and scheduler
template <StopToken Token, Scheduler Sched>
class EnvWithStopTokenAndScheduler {
  Token token_;
  Sched scheduler_;

 public:
  constexpr EnvWithStopTokenAndScheduler(Token token, Sched sched) noexcept
      : token_(token), scheduler_(sched) {}

  constexpr auto get_stop_token() const noexcept { return token_; }
  constexpr auto get_scheduler() const noexcept { return scheduler_; }
};

// Customization point: get_env
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

// Customization point: get_stop_token
// Queries an environment for its stop token
inline constexpr struct GetStopTokenFn {
  template <typename Env>
  constexpr auto operator()(const Env& env) const noexcept {
    if constexpr (requires { env.get_stop_token(); }) {
      return env.get_stop_token();
    } else {
      return NeverStopToken{};
    }
  }
} get_stop_token{};

// Customization point: get_scheduler
// Queries an environment for its scheduler
inline constexpr struct GetSchedulerFn {
  template <typename Env>
  constexpr auto operator()(const Env& env) const noexcept {
    if constexpr (requires { env.get_scheduler(); }) {
      return env.get_scheduler();
    } else {
      return InlineScheduler{};
    }
  }
} get_scheduler{};

}  // namespace tempura
