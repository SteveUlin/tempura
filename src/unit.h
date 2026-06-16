#pragma once

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <format>
#include <print>
#include <ranges>
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

  static void beginTest() { instance().current_failed_ = false; }
  static void setFailure() { instance().current_failed_ = true; }
  static void endTest() {
    if (instance().current_failed_) {
      ++instance().failed_tests_;
    }
  }
  static auto result() -> int {
    const auto failed = instance().failed_tests_;
    if (failed > 0) {
      std::println(stderr, "{} test(s) failed", failed);
    }
    return failed > 0 ? 1 : 0;
  }

 private:
  TestRegistry() = default;
  static auto instance() -> TestRegistry& {
    static TestRegistry registry;
    return registry;
  }
  bool current_failed_ = false;
  std::size_t failed_tests_ = 0;
};

class Test {
 public:
  template <std::invocable Body>
  void operator=(Body&& body) {
    TestRegistry::beginTest();
    try {
      std::forward<Body>(body)();
    } catch (const std::exception& e) {
      std::println(stderr, "{}: unexpected exception: {}", name_, e.what());
      TestRegistry::setFailure();
    } catch (...) {
      std::println(stderr, "{}: unexpected unknown exception", name_);
      TestRegistry::setFailure();
    }
    TestRegistry::endTest();
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

constexpr auto expectClose(const auto& lhs, const auto& rhs, Tolerance tol,
                           std::source_location loc = std::source_location::current()) -> bool {
  return reportBinary(isClose(lhs, rhs, tol), loc, "expectClose", lhs, rhs);
}

constexpr auto expectWithinUlps(double lhs, double rhs, std::uint64_t max_ulps = 4,
                                std::source_location loc = std::source_location::current()) -> bool {
  const auto dist = ulpDistance(lhs, rhs);
  const bool ok = dist <= max_ulps;
  if (!ok) {
    if !consteval {
      reportFailure(loc, std::format("expectWithinUlps: {} ulps apart (> {})", dist, max_ulps));
    }
  }
  return ok;
}

constexpr auto expectFinite(double x,
                            std::source_location loc = std::source_location::current()) -> bool {
  const bool ok = isFinite(x);
  if (!ok) {
    if !consteval {
      reportFailure(loc, std::format("expectFinite: {}", x));
    }
  }
  return ok;
}
constexpr auto expectNan(double x,
                         std::source_location loc = std::source_location::current()) -> bool {
  const bool ok = isNan(x);
  if (!ok) {
    if !consteval {
      reportFailure(loc, std::format("expectNan: {}", x));
    }
  }
  return ok;
}
constexpr auto expectInf(double x,
                         std::source_location loc = std::source_location::current()) -> bool {
  const bool ok = isInf(x);
  if (!ok) {
    if !consteval {
      reportFailure(loc, std::format("expectInf: {}", x));
    }
  }
  return ok;
}

template <typename R1, typename R2, typename Pred>
constexpr auto checkRange(std::source_location loc, std::string_view op, R1&& lhs, R2&& rhs,
                          Pred pred) -> bool {
  if constexpr (std::ranges::sized_range<R1> && std::ranges::sized_range<R2>) {
    if (std::ranges::size(lhs) != std::ranges::size(rhs)) {
      if !consteval {
        reportFailure(loc, std::format("{}: size mismatch ({} vs {})", op,
                                       std::ranges::size(lhs), std::ranges::size(rhs)));
      }
      return false;
    }
  }
  auto it1 = std::ranges::begin(lhs);
  auto it2 = std::ranges::begin(rhs);
  const auto end1 = std::ranges::end(lhs);
  const auto end2 = std::ranges::end(rhs);
  std::size_t i = 0;
  while (it1 != end1 && it2 != end2) {
    if (!pred(*it1, *it2)) {
      if !consteval {
        using L = std::remove_cvref_t<decltype(*it1)>;
        using R = std::remove_cvref_t<decltype(*it2)>;
        if constexpr (Formattable<L> && Formattable<R>) {
          reportFailure(loc, std::format("{}: index {}: {} vs {}", op, i, *it1, *it2));
        } else {
          reportFailure(loc, std::format("{}: mismatch at index {}", op, i));
        }
      }
      return false;
    }
    ++it1;
    ++it2;
    ++i;
  }
  if (it1 != end1 || it2 != end2) {
    if !consteval {
      reportFailure(loc, std::format("{}: length mismatch", op));
    }
    return false;
  }
  return true;
}

constexpr auto expectRangeEq(auto&& lhs, auto&& rhs,
                             std::source_location loc = std::source_location::current()) -> bool {
  return checkRange(loc, "expectRangeEq", lhs, rhs,
                    [](const auto& l, const auto& r) { return l == r; });
}
constexpr auto expectRangeClose(auto&& lhs, auto&& rhs, Tolerance tol,
                                std::source_location loc = std::source_location::current()) -> bool {
  return checkRange(loc, "expectRangeClose", lhs, rhs,
                    [tol](const auto& l, const auto& r) { return isClose(l, r, tol); });
}

}  // namespace tempura
