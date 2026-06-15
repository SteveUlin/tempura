#include "scalar.h"

#include <cstddef>
#include <string>

using namespace tempura;

// Floating-point built-ins are the canonical scalars.
static_assert(Scalar<double>);
static_assert(Scalar<float>);

// Integers have the operators but never opt in — `int/int` truncates.
static_assert(!Scalar<int>);
static_assert(!Scalar<std::size_t>);

// Non-numbers are excluded outright.
static_assert(!Scalar<std::string>);

// A custom ordered number-like joins by opting in.
struct Fixed {
  double v;
  friend auto operator+(Fixed, Fixed) -> Fixed;
  friend auto operator-(Fixed, Fixed) -> Fixed;
  friend auto operator*(Fixed, Fixed) -> Fixed;
  friend auto operator/(Fixed, Fixed) -> Fixed;
  friend auto operator<=>(const Fixed&, const Fixed&) = default;
};

// Same arithmetic, no comparison: opting in isn't enough.
struct Unordered {
  double v;
  friend auto operator+(Unordered, Unordered) -> Unordered;
  friend auto operator-(Unordered, Unordered) -> Unordered;
  friend auto operator*(Unordered, Unordered) -> Unordered;
  friend auto operator/(Unordered, Unordered) -> Unordered;
};

namespace tempura {
template <> inline constexpr bool enable_scalar<Fixed> = true;
template <> inline constexpr bool enable_scalar<Unordered> = true;
}  // namespace tempura

static_assert(Scalar<Fixed>);       // opted in, arithmetic, ordered
static_assert(!Scalar<Unordered>);  // opted in but unordered — fails the ordering clause

auto main() -> int { return 0; }
