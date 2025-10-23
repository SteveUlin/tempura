#pragma once

#include <cmath>

namespace tempura {

// --- Compile-time String Literal Class ---

template <size_t N_>
struct StaticString {
  static constexpr size_t N = N_;

  // Constructor from string literal e.g. StaticString s("abc"); N_ will be 3
  template <size_t LenWithNull>
  constexpr StaticString(const char (&str_literal)[LenWithNull]) {
    static_assert(LenWithNull == N_ + 1,
                  "Literal length mismatch for StaticString constructor");
    for (size_t i = 0; i < LenWithNull; ++i) chars_[i] = str_literal[i];
  }

  constexpr StaticString(char c)
    requires(N_ == 1)
  {
    chars_[0] = c;
    chars_[1] = '\0';  // Null-terminate
  }

  // Default constructor for empty string StaticString<0>{}
  constexpr StaticString()
    requires(N_ == 0)
  {
    chars_[0] = '\0';
  }

  constexpr const char* c_str() const { return chars_; }
  constexpr size_t size() const { return N_; }

  template <size_t M>
  constexpr bool operator==(const StaticString<M>& other) const {
    if constexpr (N_ != M) {
      return false;
    } else {
      for (size_t i = 0; i < N_; ++i) {
        if (chars_[i] != other.chars_[i]) return false;
      }
      return true;
    }
  }

  // String literal comparison (M includes null terminator)
  template <size_t M>
  constexpr bool operator==(const char (&str_literal)[M]) const {
    if constexpr (N_ != M - 1) {
      return false;
    } else {
      for (size_t i = 0; i < N_; ++i) {
        if (chars_[i] != str_literal[i]) return false;
      }
      return true;
    }
  }

  template <size_t M>
  constexpr auto operator+(const StaticString<M>& other) const
      -> StaticString<N_ + M> {
    char new_chars[N_ + M + 1] = {};
    size_t current_idx = 0;
    for (size_t i = 0; i < N_; ++i)
      new_chars[current_idx++] = chars_[i];  // Copy LHS chars (excluding null)
    for (size_t i = 0; i < M; ++i)
      new_chars[current_idx++] =
          other.chars_[i];     // Copy RHS chars (excluding null)
    new_chars[N_ + M] = '\0';  // Add new null terminator
    return StaticString<N_ + M>{new_chars};
  }

  char chars_[N_ + 1] = {};  // +1 for null terminator
};

enum class DisplayMode : char {
  kInfix,
  kPrefix,
};

// Deduction guide for string literals
template <size_t LenWithNull>
StaticString(const char (&str_literal)[LenWithNull])
    -> StaticString<LenWithNull - 1>;
StaticString(char c) -> StaticString<1>;

// User-defined literal for compile-time strings
// Usage: "hello"_cts or "alpha + beta"_cts
template <StaticString Str>
constexpr auto operator""_cts() {
  return Str;
}

struct PiOp {
  constexpr auto operator()() const { return M_PI; }
};

struct EOp {

  constexpr auto operator()() const { return M_E; }
};

struct AddOp {
  // Unary - identity
  constexpr auto operator()(auto a) const { return a; }

  // Binary
  constexpr auto operator()(auto a, auto b) const { return a + b; }

  // Variadic (3+ arguments) - left fold expansion
  constexpr auto operator()(auto first, auto second, auto... rest) const {
    return ((first + second) + ... + rest);
  }
};
struct SubOp {

  constexpr auto operator()(auto a, auto b) const { return a - b; }
};
struct NegOp {

  constexpr auto operator()(auto a) const { return -a; }
};
struct MulOp {
  // Unary - identity
  constexpr auto operator()(auto a) const { return a; }

  // Binary
  constexpr auto operator()(auto a, auto b) const { return a * b; }

  // Variadic (3+ arguments) - left fold expansion
  constexpr auto operator()(auto first, auto second, auto... rest) const {
    return ((first * second) * ... * rest);
  }
};
struct DivOp {

  constexpr auto operator()(auto a, auto b) const { return a / b; }
};
struct ModOp {

  constexpr auto operator()(auto a, auto b) const { return a % b; }
};
struct EqOp {

  constexpr auto operator()(auto a, auto b) const { return a == b; }
};
struct NeqOp {

  constexpr auto operator()(auto a, auto b) const { return a != b; }
};
struct LtOp {

  constexpr auto operator()(auto a, auto b) const { return a < b; }
};
struct LeqOp {

  constexpr auto operator()(auto a, auto b) const { return a <= b; }
};
struct GtOp {

  constexpr auto operator()(auto a, auto b) const { return a > b; }
};
struct GeqOp {

  constexpr auto operator()(auto a, auto b) const { return a >= b; }
};
struct AndOp {

  constexpr auto operator()(auto a, auto b) const { return a && b; }
};
struct OrOp {

  constexpr auto operator()(auto a, auto b) const { return a || b; }
};
struct NotOp {

  constexpr auto operator()(auto a) const { return !a; }
};
struct BitNotOp {

  constexpr auto operator()(auto a) const { return ~a; }
};
struct BitAndOp {

  constexpr auto operator()(auto a, auto b) const { return a & b; }
};
struct BitOrOp {

  constexpr auto operator()(auto a, auto b) const { return a | b; }
};
struct BitXorOp {

  constexpr auto operator()(auto a, auto b) const { return a ^ b; }
};
struct BitShiftLeftOp {

  constexpr auto operator()(auto a, auto b) const { return a << b; }
};
struct BitShiftRightOp {

  constexpr auto operator()(auto a, auto b) const { return a >> b; }
};

// Math Ops
struct SinOp {

  constexpr auto operator()(auto a) const {
    using namespace std;
    return sin(a);
  }
};
struct CosOp {

  constexpr auto operator()(auto a) const {
    using namespace std;
    return cos(a);
  }
};
struct TanOp {

  constexpr auto operator()(auto a) const {
    using namespace std;
    return tan(a);
  }
};
struct AsinOp {

  constexpr auto operator()(auto a) const {
    using namespace std;
    return asin(a);
  }
};
struct AcosOp {

  constexpr auto operator()(auto a) const {
    using namespace std;
    return acos(a);
  }
};
struct AtanOp {

  constexpr auto operator()(auto a) const {
    using namespace std;
    return atan(a);
  }
};
struct Atan2Op {

  constexpr auto operator()(auto a, auto b) const {
    using namespace std;
    return atan2(a, b);
  }
};
struct SinhOp {

  constexpr auto operator()(auto a) const {
    using namespace std;
    return sinh(a);
  }
};
struct CoshOp {

  constexpr auto operator()(auto a) const {
    using namespace std;
    return cosh(a);
  }
};
struct TanhOp {

  constexpr auto operator()(auto a) const {
    using namespace std;
    return tanh(a);
  }
};
struct ExpOp {

  constexpr auto operator()(auto a) const {
    using namespace std;
    return exp(a);
  }
};
struct LogOp {

  constexpr auto operator()(auto a) const {
    using namespace std;
    return log(a);
  }
};
struct SqrtOp {

  constexpr auto operator()(auto a) const {
    using namespace std;
    return sqrt(a);
  }
};
struct PowOp {

  constexpr auto operator()(auto a, auto b) const {
    using namespace std;
    return pow(a, b);
  }
};

}  // namespace tempura
