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
  requires(N_ == 1) {
    chars_[0] = c;
    chars_[1] = '\0';  // Null-terminate
  }

  // Default constructor for empty string StaticString<0>{}
  constexpr StaticString()
    requires(N_ == 0)
  {
    chars_[0] = '\0';
  }

  constexpr const char* c_str() const { return chars_.data(); }
  constexpr size_t size() const { return N_; }

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

struct PiOp {
  static constexpr StaticString kSymbol = "π";
  static constexpr DisplayMode kDisplayMode = DisplayMode::kInfix;

  constexpr auto operator()() const { return M_PI; }
};

struct EOp {
  static constexpr StaticString kSymbol = "e";
  static constexpr DisplayMode kDisplayMode = DisplayMode::kInfix;

  constexpr auto operator()() const { return M_E; }
};

struct AddOp {
  static constexpr StaticString kSymbol = "+";
  static constexpr DisplayMode kDisplayMode = DisplayMode::kInfix;

  constexpr auto operator()(auto a, auto b) const { return a + b; }
};
struct SubOp {
  static constexpr StaticString kSymbol = "-";
  static constexpr DisplayMode kDisplayMode = DisplayMode::kInfix;

  constexpr auto operator()(auto a, auto b) const { return a - b; }
};
struct NegOp {
  static constexpr StaticString kSymbol = "-";
  static constexpr DisplayMode kDisplayMode = DisplayMode::kPrefix;

  constexpr auto operator()(auto a) const { return -a; }
};
struct MulOp {
  static constexpr StaticString kSymbol = "*";
  static constexpr DisplayMode kDisplayMode = DisplayMode::kInfix;

  constexpr auto operator()(auto a, auto b) const { return a * b; }
};
struct DivOp {
  static constexpr StaticString kSymbol = "/";
  static constexpr DisplayMode kDisplayMode = DisplayMode::kInfix;

  constexpr auto operator()(auto a, auto b) const { return a / b; }
};
struct ModOp {
  static constexpr StaticString kSymbol = "%";
  static constexpr DisplayMode kDisplayMode = DisplayMode::kInfix;

  constexpr auto operator()(auto a, auto b) const { return a % b; }
};
struct EqOp {
  static constexpr StaticString kSymbol = "==";
  static constexpr DisplayMode kDisplayMode = DisplayMode::kInfix;

  constexpr auto operator()(auto a, auto b) const { return a == b; }
};
struct NeqOp {
  static constexpr StaticString kSymbol = "!=";
  static constexpr DisplayMode kDisplayMode = DisplayMode::kInfix;

  constexpr auto operator()(auto a, auto b) const { return a != b; }
};
struct LtOp {
  static constexpr StaticString kSymbol = "<";
  static constexpr DisplayMode kDisplayMode = DisplayMode::kInfix;

  constexpr auto operator()(auto a, auto b) const { return a < b; }
};
struct LeqOp {
  static constexpr StaticString kSymbol = "<=";
  static constexpr DisplayMode kDisplayMode = DisplayMode::kInfix;

  constexpr auto operator()(auto a, auto b) const { return a <= b; }
};
struct GtOp {
  static constexpr StaticString kSymbol = ">";
  static constexpr DisplayMode kDisplayMode = DisplayMode::kInfix;

  constexpr auto operator()(auto a, auto b) const { return a > b; }
};
struct GeqOp {
  static constexpr StaticString kSymbol = ">=";
  static constexpr DisplayMode kDisplayMode = DisplayMode::kInfix;

  constexpr auto operator()(auto a, auto b) const { return a >= b; }
};
struct AndOp {
  static constexpr StaticString kSymbol = "&&";
  static constexpr DisplayMode kDisplayMode = DisplayMode::kInfix;

  constexpr auto operator()(auto a, auto b) const { return a && b; }
};
struct OrOp {
  static constexpr StaticString kSymbol = "||";
  static constexpr DisplayMode kDisplayMode = DisplayMode::kInfix;

  constexpr auto operator()(auto a, auto b) const { return a || b; }
};
struct NotOp {
  static constexpr StaticString kSymbol = "¬";
  static constexpr DisplayMode kDisplayMode = DisplayMode::kPrefix;

  constexpr auto operator()(auto a) const { return !a; }
};
struct BitNotOp {
  static constexpr StaticString kSymbol = "~";
  static constexpr DisplayMode kDisplayMode = DisplayMode::kPrefix;

  constexpr auto operator()(auto a) const { return ~a; }
};
struct BitAndOp {
  static constexpr StaticString kSymbol = "&";
  static constexpr DisplayMode kDisplayMode = DisplayMode::kInfix;

  constexpr auto operator()(auto a, auto b) const { return a & b; }
};
struct BitOrOp {
  static constexpr StaticString kSymbol = "|";
  static constexpr DisplayMode kDisplayMode = DisplayMode::kInfix;

  constexpr auto operator()(auto a, auto b) const { return a | b; }
};
struct BitXorOp {
  static constexpr StaticString kSymbol = "^";
  static constexpr DisplayMode kDisplayMode = DisplayMode::kInfix;

  constexpr auto operator()(auto a, auto b) const { return a ^ b; }
};
struct BitShiftLeftOp {
  static constexpr StaticString kSymbol = "<<";
  static constexpr DisplayMode kDisplayMode = DisplayMode::kInfix;

  constexpr auto operator()(auto a, auto b) const { return a << b; }
};
struct BitShiftRightOp {
  static constexpr StaticString kSymbol = ">>";
  static constexpr DisplayMode kDisplayMode = DisplayMode::kInfix;

  constexpr auto operator()(auto a, auto b) const { return a >> b; }
};

// Math Ops
struct SinOp {
  static constexpr StaticString kSymbol = "sin";
  static constexpr DisplayMode kDisplayMode = DisplayMode::kPrefix;

  constexpr auto operator()(auto a) const {
    using namespace std;
    return sin(a);
  }
};
struct CosOp {
  static constexpr StaticString kSymbol = "cos";
  static constexpr DisplayMode kDisplayMode = DisplayMode::kPrefix;

  constexpr auto operator()(auto a) const {
    using namespace std;
    return cos(a);
  }
};
struct TanOp {
  static constexpr StaticString kSymbol = "tan";
  static constexpr DisplayMode kDisplayMode = DisplayMode::kPrefix;

  constexpr auto operator()(auto a) const {
    using namespace std;
    return tan(a);
  }
};
struct AsinOp {
  static constexpr StaticString kSymbol = "asin";
  static constexpr DisplayMode kDisplayMode = DisplayMode::kPrefix;

  constexpr auto operator()(auto a) const {
    using namespace std;
    return asin(a);
  }
};
struct AcosOp {
  static constexpr StaticString kSymbol = "acos";
  static constexpr DisplayMode kDisplayMode = DisplayMode::kPrefix;

  constexpr auto operator()(auto a) const {
    using namespace std;
    return acos(a);
  }
};
struct AtanOp {
  static constexpr StaticString kSymbol = "atan";
  static constexpr DisplayMode kDisplayMode = DisplayMode::kPrefix;

  constexpr auto operator()(auto a) const {
    using namespace std;
    return atan(a);
  }
};
struct Atan2Op {
  static constexpr StaticString kSymbol = "atan2";
  static constexpr DisplayMode kDisplayMode = DisplayMode::kPrefix;

  constexpr auto operator()(auto a, auto b) const {
    using namespace std;
    return atan2(a, b);
  }
};
struct SinhOp {
  static constexpr StaticString kSymbol = "sinh";
  static constexpr DisplayMode kDisplayMode = DisplayMode::kPrefix;

  constexpr auto operator()(auto a) const {
    using namespace std;
    return sinh(a);
  }
};
struct CoshOp {
  static constexpr StaticString kSymbol = "cosh";
  static constexpr DisplayMode kDisplayMode = DisplayMode::kPrefix;

  constexpr auto operator()(auto a) const {
    using namespace std;
    return cosh(a);
  }
};
struct TanhOp {
  static constexpr StaticString kSymbol = "tanh";
  static constexpr DisplayMode kDisplayMode = DisplayMode::kPrefix;

  constexpr auto operator()(auto a) const {
    using namespace std;
    return tanh(a);
  }
};
struct ExpOp {
  static constexpr StaticString kSymbol = "exp";
  static constexpr DisplayMode kDisplayMode = DisplayMode::kPrefix;

  constexpr auto operator()(auto a) const {
    using namespace std;
    return exp(a);
  }
};
struct LogOp {
  static constexpr StaticString kSymbol = "log";
  static constexpr DisplayMode kDisplayMode = DisplayMode::kPrefix;

  constexpr auto operator()(auto a) const {
    using namespace std;
    return log(a);
  }
};
struct SqrtOp {
  static constexpr StaticString kSymbol = "√";
  static constexpr DisplayMode kDisplayMode = DisplayMode::kPrefix;

  constexpr auto operator()(auto a) const {
    using namespace std;
    return sqrt(a);
  }
};
struct PowOp {
  static constexpr StaticString kSymbol = "pow";
  static constexpr DisplayMode kDisplayMode = DisplayMode::kPrefix;

  constexpr auto operator()(auto a, auto b) const {
    using namespace std;
    return pow(a, b);
  }
};

}  // namespace tempura
