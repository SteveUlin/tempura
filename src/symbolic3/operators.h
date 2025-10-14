#pragma once

#include "meta/function_objects.h"
#include "symbolic3/core.h"

// Operations for building symbolic expressions
// All operators have kSymbol and kDisplayMode for string conversion
// Operators are callable for evaluation with values (see evaluate.h)

namespace tempura::symbolic3 {

// Operation tags - made callable for evaluation with metadata for display
// AddOp is variadic and associative/commutative
struct AddOp {
  static constexpr StaticString kSymbol = "+";
  static constexpr DisplayMode kDisplayMode = DisplayMode::kInfix;

  // Unary - identity
  constexpr auto operator()(auto a) const { return a; }

  // Binary
  constexpr auto operator()(auto a, auto b) const { return a + b; }

  // Variadic (3+ arguments) - left fold expansion builds left-associated tree
  //
  // FOLD EXPANSION SEMANTICS:
  // The fold expression ((first + second) + ... + rest) is C++17 syntax for:
  //
  //   AddOp{}(1, 2, 3, 4)
  //   = ((1 + 2) + ... + {3, 4})       [fold begins with first + second]
  //   = (((1 + 2) + 3) + 4)            [left-associates remaining args]
  //
  // This creates Expression<AddOp, Expression<AddOp, ...>> structure
  // Matches the canonical form expected by simplification rules
  constexpr auto operator()(auto first, auto second, auto... rest) const {
    return ((first + second) + ... + rest);
  }
};

struct SubOp {
  static constexpr StaticString kSymbol = "-";
  static constexpr DisplayMode kDisplayMode = DisplayMode::kInfix;
  constexpr auto operator()(auto a, auto b) const { return a - b; }
};

// MulOp is variadic and associative/commutative
struct MulOp {
  static constexpr StaticString kSymbol = "*";
  static constexpr DisplayMode kDisplayMode = DisplayMode::kInfix;

  // Unary - identity
  constexpr auto operator()(auto a) const { return a; }

  // Binary
  constexpr auto operator()(auto a, auto b) const { return a * b; }

  // Variadic (3+ arguments) - left fold expansion
  // Similar to AddOp: MulOp{}(2, 3, 4, 5) = (((2 * 3) * 4) * 5)
  // Example: MulOp{}(2, 3, 4) expands to (((2 * 3) * 4)
  constexpr auto operator()(auto first, auto second, auto... rest) const {
    return ((first * second) * ... * rest);
  }
};

struct DivOp {
  static constexpr StaticString kSymbol = "/";
  static constexpr DisplayMode kDisplayMode = DisplayMode::kInfix;
  constexpr auto operator()(auto a, auto b) const { return a / b; }
};

struct PowOp {
  static constexpr StaticString kSymbol = "^";
  static constexpr DisplayMode kDisplayMode = DisplayMode::kInfix;
  constexpr auto operator()(auto a, auto b) const {
    using namespace std;
    return pow(a, b);
  }
};

struct NegOp {
  static constexpr StaticString kSymbol = "-";
  static constexpr DisplayMode kDisplayMode = DisplayMode::kPrefix;
  constexpr auto operator()(auto a) const { return -a; }
};

// Transcendental operations - all constexpr in C++26
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

// Hyperbolic trigonometric functions
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

// Mathematical constants (zero-arg operations)
struct PiOp {
  static constexpr StaticString kSymbol = "π";
  static constexpr DisplayMode kDisplayMode = DisplayMode::kPrefix;
  constexpr auto operator()() const {
    return 3.14159265358979323846264338327950288;
  }
};

struct EOp {
  static constexpr StaticString kSymbol = "e";
  static constexpr DisplayMode kDisplayMode = DisplayMode::kPrefix;
  constexpr auto operator()() const {
    return 2.71828182845904523536028747135266250;
  }
};

// Comparison operations
struct EqOp {};
struct NeqOp {};
struct LtOp {};
struct LeOp {};
struct GtOp {};
struct GeOp {};

// Logical operations
struct AndOp {};
struct OrOp {};
struct NotOp {};

// ============================================================================
// Binary Operations
// ============================================================================

// Addition - binary operation (canonical form will be applied by strategies)
template <Symbolic L, Symbolic R>
constexpr auto operator+(L, R) {
  return Expression<AddOp, L, R>{};
}

template <Symbolic L, Symbolic R>
constexpr auto operator-(L, R) {
  return Expression<SubOp, L, R>{};
}

// Multiplication - binary operation (canonical form will be applied by
// strategies)
template <Symbolic L, Symbolic R>
constexpr auto operator*(L, R) {
  return Expression<MulOp, L, R>{};
}

template <Symbolic L, Symbolic R>
constexpr auto operator/(L, R) {
  return Expression<DivOp, L, R>{};
}

// ============================================================================
// Unary Operations
// ============================================================================

template <Symbolic S>
constexpr auto operator-(S) {
  return Expression<NegOp, S>{};
}

// ============================================================================
// Transcendental Functions
// ============================================================================

template <Symbolic S>
constexpr auto sin(S) {
  return Expression<SinOp, S>{};
}

template <Symbolic S>
constexpr auto cos(S) {
  return Expression<CosOp, S>{};
}

template <Symbolic S>
constexpr auto tan(S) {
  return Expression<TanOp, S>{};
}

template <Symbolic S>
constexpr auto sinh(S) {
  return Expression<SinhOp, S>{};
}

template <Symbolic S>
constexpr auto cosh(S) {
  return Expression<CoshOp, S>{};
}

template <Symbolic S>
constexpr auto tanh(S) {
  return Expression<TanhOp, S>{};
}

template <Symbolic S>
constexpr auto exp(S) {
  return Expression<ExpOp, S>{};
}

template <Symbolic S>
constexpr auto log(S) {
  return Expression<LogOp, S>{};
}

template <Symbolic S>
constexpr auto sqrt(S) {
  return Expression<SqrtOp, S>{};
}

template <Symbolic L, Symbolic R>
constexpr auto pow(L, R) {
  return Expression<PowOp, L, R>{};
}

// ============================================================================
// Convenience: Constant Helpers
// ============================================================================

// Use template variables for common constants
template <int64_t val>
constexpr Constant<val> c{};

template <auto val>
constexpr Constant<val> constant{};

// Common mathematical constants
constexpr Constant<0> zero_c{};
constexpr Constant<1> one_c{};
constexpr Constant<2> two_c{};
constexpr Constant<-1> neg_one_c{};

// Mathematical constants (as zero-arg expressions)
constexpr Expression<PiOp> π{};
constexpr Expression<EOp> e{};

// ============================================================================
// Type Predicates for Operations
// ============================================================================

template <typename T>
constexpr bool is_add = false;

template <Symbolic L, Symbolic R>
constexpr bool is_add<Expression<AddOp, L, R>> = true;

template <typename T>
constexpr bool is_mul = false;

template <Symbolic L, Symbolic R>
constexpr bool is_mul<Expression<MulOp, L, R>> = true;

template <typename T>
constexpr bool is_trig_function = false;

template <Symbolic S>
constexpr bool is_trig_function<Expression<SinOp, S>> = true;

template <Symbolic S>
constexpr bool is_trig_function<Expression<CosOp, S>> = true;

template <Symbolic S>
constexpr bool is_trig_function<Expression<TanOp, S>> = true;

template <typename T>
constexpr bool is_transcendental = is_trig_function<T>;

template <Symbolic S>
constexpr bool is_transcendental<Expression<ExpOp, S>> = true;

template <Symbolic S>
constexpr bool is_transcendental<Expression<LogOp, S>> = true;

}  // namespace tempura::symbolic3
