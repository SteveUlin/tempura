#pragma once

#include <concepts>
#include <cstddef>
#include <cstdio>
#include <exception>
#include <format>
#include <print>
#include <source_location>
#include <string_view>
#include <type_traits>
#include <utility>

#include "comparison.h"

namespace tempura {

class TestRegistry {
 public:
  TestRegistry(const TestRegistry&) = delete;
  auto operator=(const TestRegistry&) -> TestRegistry& = delete;

  static void setFailure() { ++instance().failures_; }
  static auto result() -> int { return static_cast<int>(instance().failures_); }

 private:
  TestRegistry() = default;
  static auto instance() -> TestRegistry& {
    static TestRegistry registry;
    return registry;
  }
  std::size_t failures_ = 0;
};

class Test {
 public:
  template <std::invocable Body>
  void operator=(Body&& body) {
    try {
      std::forward<Body>(body)();
    } catch (const std::exception& e) {
      std::println(stderr, "{}: unexpected exception: {}", name_, e.what());
      TestRegistry::setFailure();
    } catch (...) {
      std::println(stderr, "{}: unexpected unknown exception", name_);
      TestRegistry::setFailure();
    }
  }

 private:
  constexpr explicit Test(std::string_view name) : name_{name} {}
  friend constexpr auto operator""_test(const char*, std::size_t) -> Test;
  std::string_view name_;
};

constexpr auto operator""_test(const char* name, std::size_t size) -> Test {
  return Test{std::string_view{name, size}};
}

template <typename T>
concept Formattable = std::formattable<std::remove_cvref_t<T>, char>;

// Runtime-only: only reached via `if !consteval`, so it may use std::format.
inline void reportFailure(std::source_location loc, std::string_view message) {
  std::println(stderr, "{}:{}: {}", loc.file_name(), loc.line(), message);
  TestRegistry::setFailure();
}

template <typename A, typename B>
constexpr auto reportBinary(bool ok, std::source_location loc, std::string_view op,
                            const A& lhs, const B& rhs) -> bool {
  if (!ok) {
    if !consteval {
      if constexpr (Formattable<A> && Formattable<B>) {
        reportFailure(loc, std::format("{}: {} vs {}", op, lhs, rhs));
      } else {
        reportFailure(loc, std::format("{} failed", op));
      }
    }
  }
  return ok;
}

constexpr auto expectEq(const auto& lhs, const auto& rhs,
                        std::source_location loc = std::source_location::current()) -> bool {
  return reportBinary(lhs == rhs, loc, "expectEq", lhs, rhs);
}
constexpr auto expectNeq(const auto& lhs, const auto& rhs,
                         std::source_location loc = std::source_location::current()) -> bool {
  return reportBinary(lhs != rhs, loc, "expectNeq", lhs, rhs);
}
constexpr auto expectLT(const auto& lhs, const auto& rhs,
                        std::source_location loc = std::source_location::current()) -> bool {
  return reportBinary(lhs < rhs, loc, "expectLT", lhs, rhs);
}
constexpr auto expectLE(const auto& lhs, const auto& rhs,
                        std::source_location loc = std::source_location::current()) -> bool {
  return reportBinary(lhs <= rhs, loc, "expectLE", lhs, rhs);
}
constexpr auto expectGT(const auto& lhs, const auto& rhs,
                        std::source_location loc = std::source_location::current()) -> bool {
  return reportBinary(lhs > rhs, loc, "expectGT", lhs, rhs);
}
constexpr auto expectGE(const auto& lhs, const auto& rhs,
                        std::source_location loc = std::source_location::current()) -> bool {
  return reportBinary(lhs >= rhs, loc, "expectGE", lhs, rhs);
}

constexpr auto expectTrue(const auto& value,
                          std::source_location loc = std::source_location::current()) -> bool {
  const bool ok = static_cast<bool>(value);
  if (!ok) {
    if !consteval {
      reportFailure(loc, "expectTrue failed");
    }
  }
  return ok;
}
constexpr auto expectFalse(const auto& value,
                           std::source_location loc = std::source_location::current()) -> bool {
  const bool ok = !static_cast<bool>(value);
  if (!ok) {
    if !consteval {
      reportFailure(loc, "expectFalse failed");
    }
  }
  return ok;
}

namespace detail {
constexpr double kDefaultDelta = 1e-4;
}  // namespace detail

constexpr auto expectNear(const auto& lhs, const auto& rhs, const auto& delta,
                          std::source_location loc = std::source_location::current()) -> bool {
  return reportBinary(isNear(lhs, rhs, delta), loc, "expectNear", lhs, rhs);
}
constexpr auto expectNear(const auto& lhs, const auto& rhs,
                          std::source_location loc = std::source_location::current()) -> bool {
  return expectNear(lhs, rhs, detail::kDefaultDelta, loc);
}

constexpr auto expectRangeEq(auto&& lhs, auto&& rhs,
                             std::source_location loc = std::source_location::current()) -> bool {
  const bool ok = rangesEqual(lhs, rhs);
  if (!ok) {
    if !consteval {
      reportFailure(loc, "expectRangeEq failed");
    }
  }
  return ok;
}
constexpr auto expectRangeNear(auto&& lhs, auto&& rhs, const auto& delta,
                               std::source_location loc = std::source_location::current()) -> bool {
  const bool ok = rangesNear(lhs, rhs, delta);
  if (!ok) {
    if !consteval {
      reportFailure(loc, "expectRangeNear failed");
    }
  }
  return ok;
}
constexpr auto expectRangeNear(auto&& lhs, auto&& rhs,
                               std::source_location loc = std::source_location::current()) -> bool {
  return expectRangeNear(std::forward<decltype(lhs)>(lhs),
                         std::forward<decltype(rhs)>(rhs), detail::kDefaultDelta, loc);
}

}  // namespace tempura
