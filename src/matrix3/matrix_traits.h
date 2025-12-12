#pragma once

#include <cstdint>

namespace tempura::matrix3 {

// Forward declaration
template <typename Scalar, std::size_t... Ns>
class InlineDense;

// Helper to extract matrix traits from various matrix types
template <typename T>
struct MatrixTraits;

template <typename Scalar, std::size_t... Ns>
struct MatrixTraits<InlineDense<Scalar, Ns...>> {
  using ValueType = Scalar;
  static constexpr int64_t kRows =
      []() constexpr -> int64_t {
        std::size_t values[] = {Ns...};
        return static_cast<int64_t>(values[0]);
      }();
  static constexpr int64_t kCols =
      []() constexpr -> int64_t {
        std::size_t values[] = {Ns...};
        return static_cast<int64_t>(values[1]);
      }();
};

}  // namespace tempura::matrix3
