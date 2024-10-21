#pragma once

#include <concepts>

namespace tempura {

template <typename Op, typename T>
concept UnaryOp = requires() {
  { Op::operator()(std::declval<T>()) } -> std::convertible_to<T>;
};

template <typename Op, typename T>
concept BinaryOp = requires() {
  {
    Op::operator()(std::declval<T>(), std::declval<T>())
  } -> std::convertible_to<T>;
};

struct Plus {
  static auto operator()(const auto& a, const auto& b) { return a + b; }
};

struct Minus {
  static auto operator()(const auto& a, const auto& b) { return a - b; }
};

struct Negate {
  static auto operator()(const auto& value) { return -value; }
};

}  // namespace tempura
