#pragma once

#include <cmath>

namespace tempura {

struct PlusFn {
  static constexpr auto operator()(const auto& a, const auto& b) {
    return a + b;
  }
};

struct MinusFn {
  static constexpr auto operator()(const auto& a, const auto& b) {
    return a - b;
  }
};

struct NegateFn {
  static constexpr auto operator()(const auto& value) { return -value; }
};

struct MultiplyFn {
  static constexpr auto operator()(const auto& a, const auto& b) {
    return a * b;
  }
};

struct DivideFn {
  static constexpr auto operator()(const auto& a, const auto& b) {
    return a / b;
  }
};

struct ModuloFn {
  static constexpr auto operator()(const auto& a, const auto& b) {
    return a % b;
  }
};

struct SqrtFn {
  static constexpr auto operator()(const auto& value) {
    return std::sqrt(value);
  }
};

struct ExpFn {
  static constexpr auto operator()(const auto& value) {
    return std::exp(value);
  }
};

struct LogFn {
  static constexpr auto operator()(const auto& value) {
    return std::log(value);
  }
};

struct Log10Fn {
  static constexpr auto operator()(const auto& value) {
    return std::log10(value);
  }
};

struct SinFn {
  static constexpr auto operator()(const auto& value) {
    return std::sin(value);
  }
};

struct CosFn {
  static constexpr auto operator()(const auto& value) {
    return std::cos(value);
  }
};

struct TanFn {
  static constexpr auto operator()(const auto& value) {
    return std::tan(value);
  }
};

struct FloorFn {
  static constexpr auto operator()(const auto& value) {
    return std::floor(value);
  }
};

struct CeilFn {
  static constexpr auto operator()(const auto& value) {
    return std::ceil(value);
  }
};

struct RoundFn {
  static constexpr auto operator()(const auto& value) {
    return std::round(value);
  }
};

}  // namespace tempura
