#pragma once

#include "stop_token.h"

namespace tempura {

// ============================================================================
// Environment - Query interface for receiver properties
// ============================================================================

// Empty environment (default)
struct EmptyEnv {
  static constexpr auto get_stop_token() noexcept { return NeverStopToken{}; }
};

// Environment with stop token
template <StopToken Token>
class EnvWithStopToken {
  Token token_;

 public:
  explicit constexpr EnvWithStopToken(Token token) noexcept : token_(token) {}

  constexpr auto get_stop_token() const noexcept { return token_; }
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

}  // namespace tempura
