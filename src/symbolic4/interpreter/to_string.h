#pragma once

#include <format>
#include <string>
#include <tuple>

#include "symbolic4/core.h"
#include "symbolic4/indexed/reduce_over.h"  // For is_reduce_over_v, ROp::symbol()
#include "symbolic4/let.h"
#include "symbolic4/operators.h"

// ============================================================================
// to_string.h - Pretty-printing symbolic expressions
// ============================================================================
//
// Converts symbolic expressions to human-readable strings using direct
// recursion with if constexpr dispatch. No cata/fold apparatus needed.
//
// Usage:
//   Symbol<struct X> x;
//   Symbol<struct Y> y;
//   auto expr = x * y + one;
//   std::string str = toString(expr, name(x, "x"), name(y, "y"));
//   // str == "x * y + 1"
//
// Handles ReduceOver via ROp::symbol() (e.g., "Σ(body)").
//
// ============================================================================

namespace tempura::symbolic4 {

// ============================================================================
// Precedence levels (higher = binds more tightly)
// ============================================================================

namespace Precedence {
constexpr int kAddition = 10;
constexpr int kMultiplication = 20;
constexpr int kPower = 30;
constexpr int kUnary = 40;
constexpr int kAtomic = 50;
}  // namespace Precedence

// ============================================================================
// DisplayTraits - presentation metadata for operators
// ============================================================================

enum class DisplayMode { kPrefix, kInfix };

template <typename Op>
struct DisplayTraits {
  static constexpr const char* symbol = "?";
  static constexpr DisplayMode mode = DisplayMode::kPrefix;
  static constexpr int precedence = 0;
};

// Binary arithmetic
template <>
struct DisplayTraits<AddOp> {
  static constexpr const char* symbol = "+";
  static constexpr DisplayMode mode = DisplayMode::kInfix;
  static constexpr int precedence = Precedence::kAddition;
};

template <>
struct DisplayTraits<SubOp> {
  static constexpr const char* symbol = "-";
  static constexpr DisplayMode mode = DisplayMode::kInfix;
  static constexpr int precedence = Precedence::kAddition;
};

template <>
struct DisplayTraits<MulOp> {
  static constexpr const char* symbol = "*";
  static constexpr DisplayMode mode = DisplayMode::kInfix;
  static constexpr int precedence = Precedence::kMultiplication;
};

template <>
struct DisplayTraits<DivOp> {
  static constexpr const char* symbol = "/";
  static constexpr DisplayMode mode = DisplayMode::kInfix;
  static constexpr int precedence = Precedence::kMultiplication;
};

template <>
struct DisplayTraits<PowOp> {
  static constexpr const char* symbol = "^";
  static constexpr DisplayMode mode = DisplayMode::kInfix;
  static constexpr int precedence = Precedence::kPower;
};

// Unary
template <>
struct DisplayTraits<NegOp> {
  static constexpr const char* symbol = "-";
  static constexpr DisplayMode mode = DisplayMode::kPrefix;
  static constexpr int precedence = Precedence::kUnary;
};

// Trigonometric
template <>
struct DisplayTraits<SinOp> {
  static constexpr const char* symbol = "sin";
  static constexpr DisplayMode mode = DisplayMode::kPrefix;
  static constexpr int precedence = Precedence::kUnary;
};

template <>
struct DisplayTraits<CosOp> {
  static constexpr const char* symbol = "cos";
  static constexpr DisplayMode mode = DisplayMode::kPrefix;
  static constexpr int precedence = Precedence::kUnary;
};

template <>
struct DisplayTraits<TanOp> {
  static constexpr const char* symbol = "tan";
  static constexpr DisplayMode mode = DisplayMode::kPrefix;
  static constexpr int precedence = Precedence::kUnary;
};

// Hyperbolic
template <>
struct DisplayTraits<SinhOp> {
  static constexpr const char* symbol = "sinh";
  static constexpr DisplayMode mode = DisplayMode::kPrefix;
  static constexpr int precedence = Precedence::kUnary;
};

template <>
struct DisplayTraits<CoshOp> {
  static constexpr const char* symbol = "cosh";
  static constexpr DisplayMode mode = DisplayMode::kPrefix;
  static constexpr int precedence = Precedence::kUnary;
};

template <>
struct DisplayTraits<TanhOp> {
  static constexpr const char* symbol = "tanh";
  static constexpr DisplayMode mode = DisplayMode::kPrefix;
  static constexpr int precedence = Precedence::kUnary;
};

// Exponential/logarithmic
template <>
struct DisplayTraits<ExpOp> {
  static constexpr const char* symbol = "exp";
  static constexpr DisplayMode mode = DisplayMode::kPrefix;
  static constexpr int precedence = Precedence::kUnary;
};

template <>
struct DisplayTraits<LogOp> {
  static constexpr const char* symbol = "log";
  static constexpr DisplayMode mode = DisplayMode::kPrefix;
  static constexpr int precedence = Precedence::kUnary;
};

template <>
struct DisplayTraits<SqrtOp> {
  static constexpr const char* symbol = "sqrt";
  static constexpr DisplayMode mode = DisplayMode::kPrefix;
  static constexpr int precedence = Precedence::kUnary;
};

// Constants
template <>
struct DisplayTraits<PiOp> {
  static constexpr const char* symbol = "π";
  static constexpr DisplayMode mode = DisplayMode::kPrefix;
  static constexpr int precedence = Precedence::kAtomic;
};

template <>
struct DisplayTraits<EOp> {
  static constexpr const char* symbol = "e";
  static constexpr DisplayMode mode = DisplayMode::kPrefix;
  static constexpr int precedence = Precedence::kAtomic;
};

// Inverse trigonometric
template <>
struct DisplayTraits<AsinOp> {
  static constexpr const char* symbol = "asin";
  static constexpr DisplayMode mode = DisplayMode::kPrefix;
  static constexpr int precedence = Precedence::kUnary;
};

template <>
struct DisplayTraits<AcosOp> {
  static constexpr const char* symbol = "acos";
  static constexpr DisplayMode mode = DisplayMode::kPrefix;
  static constexpr int precedence = Precedence::kUnary;
};

template <>
struct DisplayTraits<AtanOp> {
  static constexpr const char* symbol = "atan";
  static constexpr DisplayMode mode = DisplayMode::kPrefix;
  static constexpr int precedence = Precedence::kUnary;
};

// Special functions for probability distributions
template <>
struct DisplayTraits<LgammaOp> {
  static constexpr const char* symbol = "lgamma";
  static constexpr DisplayMode mode = DisplayMode::kPrefix;
  static constexpr int precedence = Precedence::kUnary;
};

template <>
struct DisplayTraits<DigammaOp> {
  static constexpr const char* symbol = "digamma";
  static constexpr DisplayMode mode = DisplayMode::kPrefix;
  static constexpr int precedence = Precedence::kUnary;
};

template <>
struct DisplayTraits<ErfOp> {
  static constexpr const char* symbol = "erf";
  static constexpr DisplayMode mode = DisplayMode::kPrefix;
  static constexpr int precedence = Precedence::kUnary;
};

template <>
struct DisplayTraits<ErfcOp> {
  static constexpr const char* symbol = "erfc";
  static constexpr DisplayMode mode = DisplayMode::kPrefix;
  static constexpr int precedence = Precedence::kUnary;
};

// Numerical stability functions
template <>
struct DisplayTraits<Log1pOp> {
  static constexpr const char* symbol = "log1p";
  static constexpr DisplayMode mode = DisplayMode::kPrefix;
  static constexpr int precedence = Precedence::kUnary;
};

template <>
struct DisplayTraits<Expm1Op> {
  static constexpr const char* symbol = "expm1";
  static constexpr DisplayMode mode = DisplayMode::kPrefix;
  static constexpr int precedence = Precedence::kUnary;
};

// Additional math functions
template <>
struct DisplayTraits<AbsOp> {
  static constexpr const char* symbol = "abs";
  static constexpr DisplayMode mode = DisplayMode::kPrefix;
  static constexpr int precedence = Precedence::kUnary;
};

template <>
struct DisplayTraits<FloorOp> {
  static constexpr const char* symbol = "floor";
  static constexpr DisplayMode mode = DisplayMode::kPrefix;
  static constexpr int precedence = Precedence::kUnary;
};

template <>
struct DisplayTraits<CeilOp> {
  static constexpr const char* symbol = "ceil";
  static constexpr DisplayMode mode = DisplayMode::kPrefix;
  static constexpr int precedence = Precedence::kUnary;
};

template <>
struct DisplayTraits<CbrtOp> {
  static constexpr const char* symbol = "cbrt";
  static constexpr DisplayMode mode = DisplayMode::kPrefix;
  static constexpr int precedence = Precedence::kUnary;
};

template <>
struct DisplayTraits<Log10Op> {
  static constexpr const char* symbol = "log10";
  static constexpr DisplayMode mode = DisplayMode::kPrefix;
  static constexpr int precedence = Precedence::kUnary;
};

template <>
struct DisplayTraits<Log2Op> {
  static constexpr const char* symbol = "log2";
  static constexpr DisplayMode mode = DisplayMode::kPrefix;
  static constexpr int precedence = Precedence::kUnary;
};

template <>
struct DisplayTraits<Exp2Op> {
  static constexpr const char* symbol = "exp2";
  static constexpr DisplayMode mode = DisplayMode::kPrefix;
  static constexpr int precedence = Precedence::kUnary;
};

// Binary functions (prefix style with two args)
template <>
struct DisplayTraits<HypotOp> {
  static constexpr const char* symbol = "hypot";
  static constexpr DisplayMode mode = DisplayMode::kPrefix;
  static constexpr int precedence = Precedence::kUnary;
};

template <>
struct DisplayTraits<MaxOp> {
  static constexpr const char* symbol = "max";
  static constexpr DisplayMode mode = DisplayMode::kPrefix;
  static constexpr int precedence = Precedence::kUnary;
};

template <>
struct DisplayTraits<MinOp> {
  static constexpr const char* symbol = "min";
  static constexpr DisplayMode mode = DisplayMode::kPrefix;
  static constexpr int precedence = Precedence::kUnary;
};

// ============================================================================
// Type-based unique ID generation (works with incomplete types)
// ============================================================================

namespace detail {
template <typename T>
struct TypeIdHolder {
  static constexpr char id = 0;
};

template <typename T>
constexpr auto typeId() -> SizeT {
  return reinterpret_cast<SizeT>(&TypeIdHolder<T>::id);
}
}  // namespace detail

// ============================================================================
// StringResult - carries string + precedence for parenthesization
// ============================================================================

struct StringResult {
  std::string str;
  int precedence = Precedence::kAtomic;

  auto wrap(int parent_prec) const -> std::string {
    if (precedence < parent_prec) {
      return "(" + str + ")";
    }
    return str;
  }

  // For RIGHT operands of non-associative operators (-, /):
  // wrap if precedence <= parent to preserve mathematical meaning.
  auto wrapRight(int parent_prec) const -> std::string {
    if (precedence <= parent_prec) {
      return "(" + str + ")";
    }
    return str;
  }
};

// ============================================================================
// String binding: name(sym, "str") syntax
// ============================================================================

template <typename Id>
struct StringBinding {
  using id_type = Id;
  std::string str;
};

template <typename Id>
auto name(Atom<Id, Free>, const char* str) -> StringBinding<Id> {
  return StringBinding<Id>{std::string(str)};
}

template <typename Id>
auto name(Atom<Id, Free>, std::string str) -> StringBinding<Id> {
  return StringBinding<Id>{std::move(str)};
}

// StringBinderPack: holds all name bindings, provides lookup<Id>()
template <typename... Bindings>
struct StringBinderPack {
  std::tuple<Bindings...> bindings;

  template <typename Id>
  auto lookup() const -> std::string {
    return lookupImpl<Id>(std::make_index_sequence<sizeof...(Bindings)>{});
  }

 private:
  template <typename Id, SizeT... Is>
  auto lookupImpl(std::index_sequence<Is...>) const -> std::string {
    std::string result = "?";
    auto try_match = [&]<SizeT I>() {
      using B = std::tuple_element_t<I, std::tuple<Bindings...>>;
      if constexpr (std::is_same_v<typename B::id_type, Id>) {
        result = std::get<I>(bindings).str;
      }
    };
    (try_match.template operator()<Is>(), ...);
    return result;
  }
};

// ============================================================================
// Direct recursion toString implementation
// ============================================================================

namespace tostring_detail {

// Forward declaration
template <Symbolic E, typename Ctx>
auto toStringImpl(E expr, Ctx& ctx) -> StringResult;

// Terminal rendering
template <typename T, typename Ctx>
auto toStringTerminal(T, Ctx& ctx) -> StringResult {
  if constexpr (is_constant_v<T>) {
    if constexpr (std::is_integral_v<decltype(T::value)>) {
      return {std::format("{}", T::value), Precedence::kAtomic};
    } else {
      return {std::format("{:.6g}", T::value), Precedence::kAtomic};
    }
  } else if constexpr (is_fraction_v<T>) {
    if constexpr (T::denominator == 1) {
      return {std::format("{}", T::numerator), Precedence::kAtomic};
    } else {
      return {std::format("{}/{}", T::numerator, T::denominator),
              Precedence::kMultiplication};
    }
  } else if constexpr (is_atom_v<T>) {
    return {ctx.template lookup<typename T::id_type>(), Precedence::kAtomic};
  } else {
    return {"?", Precedence::kAtomic};
  }
}

// Combine child StringResults for an Expression node
template <typename Op, typename... Rs>
auto combineResults(Rs... children) -> StringResult {
  constexpr auto mode = DisplayTraits<Op>::mode;
  constexpr auto prec = DisplayTraits<Op>::precedence;
  const char* sym = DisplayTraits<Op>::symbol;

  if constexpr (sizeof...(Rs) == 0) {
    // Nullary (pi, e)
    return {sym, prec};
  } else if constexpr (sizeof...(Rs) == 1) {
    // Unary
    auto [child] = std::tuple{children...};
    if (mode == DisplayMode::kPrefix) {
      return {std::format("{}({})", sym, child.str), prec};
    } else {
      return {std::format("{}{}", sym, child.wrap(prec)), prec};
    }
  } else if constexpr (sizeof...(Rs) == 2) {
    // Binary
    auto [left, right] = std::tuple{children...};
    if (mode == DisplayMode::kInfix) {
      constexpr bool is_non_associative =
          std::is_same_v<Op, SubOp> || std::is_same_v<Op, DivOp>;
      std::string right_str =
          is_non_associative ? right.wrapRight(prec) : right.wrap(prec);
      return {std::format("{} {} {}", left.wrap(prec), sym, right_str), prec};
    }
    return {std::format("{}({}, {})", sym, left.str, right.str), prec};
  } else {
    // More than binary
    if (mode == DisplayMode::kInfix) {
      std::string result;
      bool first = true;
      auto append = [&](const StringResult& r) {
        if (!first) {
          result += std::format(" {} ", sym);
        }
        result += r.wrap(prec);
        first = false;
      };
      (append(children), ...);
      return {result, prec};
    }
    std::string result = std::format("{}(", sym);
    bool first = true;
    auto append = [&](const StringResult& r) {
      if (!first) result += ", ";
      result += r.str;
      first = false;
    };
    (append(children), ...);
    result += ")";
    return {result, prec};
  }
}

// Helper: render Expression node by expanding children
template <typename Op, typename... Args, typename Ctx, SizeT... Is>
auto toStringExpression(Expression<Op, Args...> expr, Ctx& ctx,
                        IndexSequence<Is...>) -> StringResult {
  return combineResults<Op>(toStringImpl(expr.template arg<Is>(), ctx)...);
}

// Direct recursive toString
template <Symbolic E, typename Ctx>
auto toStringImpl(E expr, Ctx& ctx) -> StringResult {
  if constexpr (is_reduce_over_v<E>) {
    // ReduceOver: render as "ROp::symbol()(body)"
    using ROp = typename E::reduce_op;
    auto body = toStringImpl(expr.expr(), ctx);
    return {std::format("{}({})", ROp::symbol(), body.str), Precedence::kUnary};
  } else if constexpr (is_expression_v<E>) {
    return toStringExpression(expr, ctx, MakeIndexSequence<E::arity>{});
  } else {
    return toStringTerminal(expr, ctx);
  }
}

}  // namespace tostring_detail

// ============================================================================
// Convenience function: toString(expr, name(x, "x"), name(y, "y"), ...)
// ============================================================================

template <Symbolic E, typename... Bindings>
auto toString(E expr, Bindings... bindings) -> std::string {
  using Pack = StringBinderPack<Bindings...>;
  Pack ctx{std::tuple{bindings...}};
  return tostring_detail::toStringImpl(expr, ctx).str;
}

// ============================================================================
// LetNode support - show as "let_NNN = expr"
// ============================================================================

template <typename Id, Symbolic E, typename... Bindings>
auto toString(LetNode<Id, E> node, Bindings... bindings) -> std::string {
  SizeT id = detail::typeId<Id>();
  return std::format("let_{} = {}", id % 1000, toString(node.expr(), bindings...));
}

}  // namespace tempura::symbolic4
