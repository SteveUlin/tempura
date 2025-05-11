#pragma once

#include <charconv>

#include "meta/containers.h"
#include "meta/function_objects.h"
#include "meta/tags.h"
#include "meta/tuple.h"
#include "meta/type_id.h"
#include "meta/utility.h"

// Tempura Symbolic, is a header-only C++26 library for compile-time symbolic
// mathematics and expression manipulation.
//
// Example:
//   Symbol x;
//   Symbol y;
//   auto expr = x + y;
//   println("x + y = ", evaluate(expr, BinderPack{x = 1, y = 2}));

namespace tempura {

// --- Core Symbolic Concept & Tag ---

struct SymbolicTag {};

template <typename T>
concept Symbolic = DerivedFrom<T, SymbolicTag>;

// --- Type-Value Binding (Evaluation and Substitution) ---

// Binds a specific compile-time type (Symbol) to a runtime or compile-time
// value.
template <typename TypeKey, typename ValueType>
class TypeValueBinder {
 public:
  constexpr TypeValueBinder(TypeKey /*key*/, ValueType value) : value_(value) {}

  // Retrieve the held value if you supply the TypeKey.
  // BinderPack Overloads this function for different TypeKeys.
  constexpr auto operator[](TypeKey /*key*/) const -> ValueType {
    return value_;
  }

 private:
  ValueType value_;
};

// A collection of TypeKey-Value pairs enabling lookup with
// `binder_pack[TypeKey{}]`.
template <typename... Binders>
struct BinderPack : Binders... {
  constexpr BinderPack(Binders... binders) : Binders(binders)... {}
  using Binders::operator[]...;
};
template <typename... Ts, typename... Us>
BinderPack(TypeValueBinder<Ts, Us>...)
    -> BinderPack<TypeValueBinder<Ts, Us>...>;

// --- Type-Value Binding (Evaluation and Substitution) ---

// Symbol is a unique symbolic variable.
//
// The lambda in the template ensures that each call to Symbol{}, will produce
// a different unique type.
template <typename unique = decltype([] {})>
struct Symbol : SymbolicTag {
  // kMeta acts as a compile-time counter for types. Symbols with lower ids
  // (defined first) will have higher precedence in comparisons.
  static constexpr auto id = kMeta<Symbol<unique>>;

  constexpr Symbol() {
    // In order to preserve symbol order, force type id generation for all
    // Symbols
    (void)id;
  };

  // Create a type value binder with the assignment operator
  // auto binder x = 5;
  //   -> binder[x] == 5;
  constexpr auto operator=(auto value) const {
    return TypeValueBinder{Symbol{}, value};
  }
};

// --- Constant ---

template <auto val>
struct Constant : SymbolicTag {
  static constexpr auto value = val;
};

// --- Constant - Helper Literals ---
//
// Convert 1_c to Constant<1> and 3.14_c to Constant<3.14>
//
// Note that prefixes such as + and - in +10_c and -4_c are not parsed
// as part of the number. Instead C++ treats these as unary operators and
// must be handled separately.

template <char... chars>
  requires(sizeof...(chars) > 0)
constexpr auto toInt() {
  return [] constexpr {
    long long value = 0;
    const auto process_char = [&](char c) {
      if ('0' <= c && c <= '9') {
        value = value * 10 + (c - '0');
      }
    };

    (process_char(chars), ...);
    return static_cast<int>(value);
  }();
}

// Assume that there is exactly one decimal point in the number
template <char... chars>
constexpr auto toDouble() {
  double value = 0.;
  double fraction = 1.;
  bool is_fraction = false;
  const auto process_char = [&](char c) {
    if ('0' <= c && c <= '9') {
      if (is_fraction) {
        fraction /= 10.;
      }
      value = value * 10. + (c - '0');
    } else if (c == '.') {
      is_fraction = true;
    }
  };
  (process_char(chars), ...);
  return value * fraction;
}

template <char... chars>
constexpr auto count(char c) {
  return ((c == chars) + ... + 0);
}

template <char... chars>
constexpr auto operator""_c() {
  constexpr int dot_count = count<chars...>('.');
  if constexpr (dot_count == 0) {
    return Constant<toInt<chars...>()>{};
  } else if constexpr (dot_count == 1) {
    return Constant<toDouble<chars...>()>{};
  } else {
    static_assert(dot_count < 2, "Invalid floating point constant");
  }
}

template <typename Op, Symbolic... Args>
struct Expression : SymbolicTag {
  constexpr Expression() = default;
  constexpr Expression(Op, Args...) {}
};

struct AnyArg : SymbolicTag {};

struct AnyExpr : SymbolicTag {};

struct AnyConstant : SymbolicTag {};

struct AnySymbol : SymbolicTag {};

struct Never : SymbolicTag {};

constexpr auto operand(Symbolic auto) { return Never{}; }
template <typename Op, Symbolic Arg>
constexpr auto operand(Expression<Op, Arg>) {
  return Arg{};
}

constexpr auto left(Symbolic auto) { return Never{}; }
template <typename Op, Symbolic LHS, Symbolic RHS>
constexpr auto left(Expression<Op, LHS, RHS>) {
  return LHS{};
}

constexpr auto right(Symbolic auto) { return Never{}; }
template <typename Op, Symbolic LHS, Symbolic RHS>
constexpr auto right(Expression<Op, LHS, RHS>) {
  return RHS{};
}

constexpr auto getOp(Symbolic auto) { return Never{}; }
template <typename Op, Symbolic... Args>
constexpr auto getOp(Expression<Op, Args...>) {
  return Op{};
}

// --- Matching Logic ---

// Rank4: Highest priority - Never is always a mismatch.
constexpr auto matchImpl(Rank5, Never, Never) -> bool { return false; }
template <Symbolic S>
constexpr auto matchImpl(Rank5, Never, S) -> bool {
  return false;
}
template <Symbolic S>
constexpr auto matchImpl(Rank5, S, Never) -> bool {
  return false;
}

// Rank3: Exact Type Match
template <Symbolic S>
constexpr auto matchImpl(Rank4, S, S) -> bool {
  return true;
}

// Rank2: Wildcard matching
template <Symbolic S>
constexpr auto matchImpl(Rank3, S, AnyArg) -> bool {
  return true;
}
template <Symbolic S>
constexpr auto matchImpl(Rank3, AnyArg, S) -> bool {
  return true;
}
template <typename Op, Symbolic... Args>
constexpr auto matchImpl(Rank3, Expression<Op, Args...>, AnyExpr) -> bool {
  return true;
}
template <typename Op, Symbolic... Args>
constexpr auto matchImpl(Rank3, AnyExpr, Expression<Op, Args...>) -> bool {
  return true;
}
template <auto value>
constexpr auto matchImpl(Rank3, Constant<value>, AnyConstant) -> bool {
  return true;
}
template <auto value>
constexpr auto matchImpl(Rank3, AnyConstant, Constant<value>) -> bool {
  return true;
}
template <typename Unique>
constexpr auto matchImpl(Rank3, Symbol<Unique>, AnySymbol) -> bool {
  return true;
}
template <typename Unique>
constexpr auto matchImpl(Rank3, AnySymbol, Symbol<Unique>) -> bool {
  return true;
}

// Rank2: Values check the held number for equality so 1.0 == 1;
template <auto value_lhs, auto value_rhs>
constexpr auto matchImpl(Rank2, Constant<value_lhs>, Constant<value_rhs>)
    -> bool {
  return value_lhs == value_rhs;
}

// Rank1: Handles comparison of Expression types.
// Requires the same operator type and recursively matches arguments.
template <typename Op, Symbolic... LHSArgs, Symbolic... RHSArgs>
constexpr auto matchImpl(Rank1, Expression<Op, LHSArgs...>,
                         Expression<Op, RHSArgs...>) -> bool {
  // Ensure the number of arguments is the same. The operator `Op` must match
  // due to template specialization.
  if constexpr (sizeof...(LHSArgs) != sizeof...(RHSArgs)) {
    return false;
  } else if constexpr (sizeof...(LHSArgs) == 0 && (sizeof...(RHSArgs) == 0)) {
    return true;
  } else {
    return (matchImpl(Rank4{}, LHSArgs{}, RHSArgs{}) && ...);
  }
}

// Rank0: Lowest priority - Default catch-all when no higher-priority rule
// matches.
constexpr auto matchImpl(Rank0, Symbolic auto, Symbolic auto) -> bool {
  return false;
}

// Public interface for compile-time expression matching.
// It initiates the matching process starting with the highest priority rank.
template <Symbolic LHS, Symbolic RHS>
constexpr auto match(LHS lhs, RHS rhs) -> bool {
  return matchImpl(Rank5{}, lhs, rhs);
}

// --- Compile-Time Expression Evaluation ---

// Evaluation base case: A Constant evaluates to its stored value.
template <auto V, Symbolic... Tags, typename... Ts>
constexpr auto evaluate(Constant<V>,
                        const BinderPack<TypeValueBinder<Tags, Ts>...>&) {
  return V;
}

template <typename SymbolTag, typename... Tags, typename... Ts>
constexpr auto evaluate(
    Symbol<SymbolTag>,
    const BinderPack<TypeValueBinder<Tags, Ts>...>& binders) {
  // The symbol's *type* is used as the key to look up the value.
  return binders[Symbol<SymbolTag>{}];
}

template <typename Op, Symbolic... Args, typename... Tags, typename... Ts>
constexpr auto evaluate(
    Expression<Op, Args...>,
    const BinderPack<TypeValueBinder<Tags, Ts>...>& binders) {
  // Instantiate the operator tag `Op{}` and call it with the evaluated
  // arguments. `evaluate(Args{}, binders)...` expands to evaluate each
  // argument
  // recursively.
  return Op{}(evaluate(Args{}, binders)...);
}

template <Symbolic LHS, Symbolic RHS>
constexpr auto operator+(LHS, RHS) {
  return Expression<AddOp, LHS, RHS>{};
}

template <Symbolic LHS, Symbolic RHS>
constexpr auto operator-(LHS, RHS) {
  return Expression<SubOp, LHS, RHS>{};
}

template <Symbolic LHS, Symbolic RHS>
constexpr auto operator*(LHS, RHS) {
  return Expression<MulOp, LHS, RHS>{};
}

template <Symbolic LHS, Symbolic RHS>
constexpr auto operator/(LHS, RHS) {
  return Expression<DivOp, LHS, RHS>{};
}

template <Symbolic LHS, Symbolic RHS>
constexpr auto operator%(LHS, RHS) {
  return Expression<ModOp, LHS, RHS>{};
}

template <Symbolic LHS, Symbolic RHS>
constexpr auto operator==(LHS, RHS) {
  return Expression<EqOp, LHS, RHS>{};
}

template <Symbolic LHS, Symbolic RHS>
constexpr auto operator!=(LHS, RHS) {
  return Expression<NeqOp, LHS, RHS>{};
}

template <Symbolic LHS, Symbolic RHS>
constexpr auto operator<(LHS, RHS) {
  return Expression<LtOp, LHS, RHS>{};
}

template <Symbolic LHS, Symbolic RHS>
constexpr auto operator<=(LHS, RHS) {
  return Expression<LeqOp, LHS, RHS>{};
}

template <Symbolic LHS, Symbolic RHS>
constexpr auto operator>(LHS, RHS) {
  return Expression<GtOp, LHS, RHS>{};
}

template <Symbolic LHS, Symbolic RHS>
constexpr auto operator>=(LHS, RHS) {
  return Expression<GeqOp, LHS, RHS>{};
}

template <Symbolic LHS, Symbolic RHS>
constexpr auto operator&&(LHS, RHS) {
  return Expression<AndOp, LHS, RHS>{};
}

template <Symbolic LHS, Symbolic RHS>
constexpr auto operator||(LHS, RHS) {
  return Expression<OrOp, LHS, RHS>{};
}

template <Symbolic Arg>
constexpr auto operator!(Arg) {
  return Expression<NotOp, Arg>{};
}

template <Symbolic Arg>
constexpr auto operator~(Arg) {
  return Expression<BitNotOp, Arg>{};
}

template <Symbolic LHS, Symbolic RHS>
constexpr auto operator&(LHS, RHS) {
  return Expression<BitAndOp, LHS, RHS>{};
}

template <Symbolic LHS, Symbolic RHS>
constexpr auto operator|(LHS, RHS) {
  return Expression<BitOrOp, LHS, RHS>{};
}

template <Symbolic LHS, Symbolic RHS>
constexpr auto operator^(LHS, RHS) {
  return Expression<BitXorOp, LHS, RHS>{};
}

template <Symbolic LHS, Symbolic RHS>
constexpr auto operator<<(LHS, RHS) {
  return Expression<BitShiftLeftOp, LHS, RHS>{};
}

template <Symbolic LHS, Symbolic RHS>
constexpr auto operator>>(LHS, RHS) {
  return Expression<BitShiftRightOp, LHS, RHS>{};
}

template <Symbolic Arg>
constexpr auto sin(Arg) {
  return Expression<SinOp, Arg>{};
}

template <Symbolic Arg>
constexpr auto cos(Arg) {
  return Expression<CosOp, Arg>{};
}

template <Symbolic Arg>
constexpr auto tan(Arg) {
  return Expression<TanOp, Arg>{};
}

template <Symbolic Arg>
constexpr auto asin(Arg) {
  return Expression<AsinOp, Arg>{};
}

template <Symbolic Arg>
constexpr auto acos(Arg) {
  return Expression<AcosOp, Arg>{};
}

template <Symbolic Arg>
constexpr auto atan(Arg) {
  return Expression<AtanOp, Arg>{};
}

template <Symbolic LHS, Symbolic RHS>
constexpr auto atan2(LHS, RHS) {
  return Expression<Atan2Op, LHS, RHS>{};
}

template <Symbolic Arg>
constexpr auto sinh(Arg) {
  return Expression<SinhOp, Arg>{};
}

template <Symbolic Arg>
constexpr auto cosh(Arg) {
  return Expression<CoshOp, Arg>{};
}

template <Symbolic Arg>
constexpr auto tanh(Arg) {
  return Expression<TanhOp, Arg>{};
}

template <Symbolic Arg>
constexpr auto exp(Arg) {
  return Expression<ExpOp, Arg>{};
}

template <Symbolic Arg>
constexpr auto log(Arg) {
  return Expression<LogOp, Arg>{};
}

template <Symbolic Arg>
constexpr auto sqrt(Arg) {
  return Expression<SqrtOp, Arg>{};
}

template <Symbolic LHS, Symbolic RHS>
constexpr auto pow(LHS, RHS) {
  return Expression<PowOp, LHS, RHS>{};
}

// -- Zero Arg Expressions ---

static constexpr auto π = Expression<PiOp>{};
static constexpr auto e = Expression<EOp>{};

// --- Symbolic Less Than ---
enum class PartialOrdering : char { kLess, kEqual, kGreater };

template <typename LhsOp, typename RhsOp>
constexpr auto opCompare(LhsOp, RhsOp) {
  constexpr static MinimalArray kOpOrder{
      // Special Constants
      kMeta<EOp>,
      kMeta<PiOp>,
      // Arithmetic
      kMeta<AddOp>,
      kMeta<SubOp>,
      kMeta<MulOp>,
      kMeta<DivOp>,
      // Power and Roots
      kMeta<PowOp>,
      kMeta<Atan2Op>,
      kMeta<SqrtOp>,
      // Exponentials and Logarithms
      kMeta<ExpOp>,
      kMeta<LogOp>,
      // Trigonometric Functions
      kMeta<SinOp>,
      kMeta<CosOp>,
      kMeta<TanOp>,
      // Inverse Trigonometric Functions
      kMeta<AsinOp>,
      kMeta<AcosOp>,
      kMeta<AtanOp>,
      // Hyperbolic Functions
      kMeta<SinhOp>,
      kMeta<CoshOp>,
      kMeta<TanhOp>,
      // Comparison Operators
      kMeta<EqOp>,
      kMeta<NeqOp>,
      kMeta<LtOp>,
      kMeta<LeqOp>,
      kMeta<GtOp>,
      kMeta<GeqOp>,
      // Logical
      kMeta<AndOp>,
      kMeta<OrOp>,
      kMeta<NotOp>,
      // Bitwise
      kMeta<BitAndOp>,
      kMeta<BitOrOp>,
      kMeta<BitXorOp>,
      kMeta<BitShiftLeftOp>,
      kMeta<BitShiftRightOp>,
  };
  constexpr auto getIndex = [](TypeId id) -> SizeT {
    for (SizeT i = 0; i < kOpOrder.size(); ++i) {
      if (kOpOrder[i] == id) {
        return i;
      }
    }
    return kOpOrder.size();
  };
  constexpr SizeT lhs_id = getIndex(kMeta<LhsOp>);
  static_assert(lhs_id < kOpOrder.size());
  constexpr SizeT rhs_id = getIndex(kMeta<RhsOp>);
  static_assert(rhs_id < kOpOrder.size());

  if (lhs_id < rhs_id) {
    return PartialOrdering::kLess;
  }
  if (lhs_id > rhs_id) {
    return PartialOrdering::kGreater;
  }
  return PartialOrdering::kEqual;
}
static_assert(opCompare(AddOp{}, LogOp{}) == PartialOrdering::kLess);

// A total ordering of symbols
//
// Values are sorted by what is "most likely to be simplified"
// 1. Expressions
// 2. Symbols
// 3. Constants
//
// The goal is the LHS of an expr be used at the first comparison
// point when comparing two like expressions. So the base x in x^3 or y
// in y*3.
constexpr auto symbolicCompare(Symbolic auto lhs, Symbolic auto rhs)
    -> PartialOrdering {
  // --- Category Comparison ---
  constexpr bool lhs_is_expr = match(lhs, AnyExpr{});
  constexpr bool rhs_is_expr = match(rhs, AnyExpr{});
  constexpr bool lhs_is_constant = match(lhs, AnyConstant{});
  constexpr bool rhs_is_constant = match(rhs, AnyConstant{});
  constexpr bool lhs_is_symbol = match(lhs, AnySymbol{});
  constexpr bool rhs_is_symbol = match(rhs, AnySymbol{});
  constexpr bool lhs_is_never = isSame<decltype(lhs), Never>;
  constexpr bool rhs_is_never = isSame<decltype(rhs), Never>;

  // Edge Case: Everything is less than Never
  if constexpr (lhs_is_never && lhs_is_never) {
    return PartialOrdering::kEqual;
  } else if constexpr (lhs_is_never) {
    return PartialOrdering::kGreater;
  } else if constexpr (rhs_is_never) {
    return PartialOrdering::kLess;
  }

  // Comparison Normalization
  // a => a + 0
  else if constexpr (match(lhs, AnyExpr{} + AnyExpr{}) &&
                     !match(rhs, AnyExpr{} + AnyExpr{})) {
    return symbolicCompare(lhs, rhs + 0_c);
  } else if constexpr (!match(lhs, AnyExpr{} + AnyExpr{}) &&
                       match(rhs, AnyExpr{} + AnyExpr{})) {
    return symbolicCompare(lhs + 0_c, rhs);
  }
  // a => a * 1
  else if constexpr (match(lhs, AnyExpr{} * AnyExpr{}) &&
                     !match(rhs, AnyExpr{} * AnyExpr{})) {
    return symbolicCompare(lhs, rhs * 1_c);
  } else if constexpr (!match(lhs, AnyExpr{} * AnyExpr{}) &&
                       match(rhs, AnyExpr{} * AnyExpr{})) {
    return symbolicCompare(lhs * 1_c, rhs);
  }
  // a => a ^ 1
  else if constexpr (match(lhs, pow(AnyArg{}, AnyArg{})) &&
                     !match(rhs, pow(AnyArg{}, AnyArg{}))) {
    return symbolicCompare(lhs, pow(rhs, 1_c));
  } else if constexpr (!match(lhs, pow(AnyArg{}, AnyArg{})) &&
                       match(rhs, pow(AnyArg{}, AnyArg{}))) {
    return symbolicCompare(pow(lhs, 1_c), rhs);
  }

  // First Expressions
  else if constexpr (lhs_is_expr && !rhs_is_expr) {
    return PartialOrdering::kLess;
  } else if constexpr (!lhs_is_expr && rhs_is_expr) {
    return PartialOrdering::kGreater;
  }

  // Then Symbols
  else if constexpr (lhs_is_symbol && !rhs_is_symbol) {
    return PartialOrdering::kLess;
  } else if constexpr (!lhs_is_symbol && rhs_is_symbol) {
    return PartialOrdering::kGreater;
  }

  // -- Within Category Comparison --
  //
  // Expressions: If operators are different, then compare them.
  else if constexpr (lhs_is_expr && rhs_is_expr) {
    constexpr auto get_arg_count =
        []<typename Op, typename... Args>(Expression<Op, Args...>) {
          return sizeof...(Args);
        };
    constexpr SizeT lhs_arg_count = get_arg_count(lhs);
    constexpr SizeT rhs_arg_count = get_arg_count(rhs);

    constexpr auto op_compare = opCompare(getOp(lhs), getOp(rhs));
    if constexpr (op_compare != PartialOrdering::kEqual) {
      return op_compare;
    } else if constexpr (lhs_arg_count < rhs_arg_count) {
      return PartialOrdering::kLess;
    } else if constexpr (lhs_arg_count > rhs_arg_count) {
      return PartialOrdering::kGreater;
    } else {
      constexpr auto compare_args_recursively =
          []<typename Op, typename... LhsArgs, typename... RhsArgs>(
              Expression<Op, LhsArgs...>, Expression<Op, RhsArgs...>) {
            PartialOrdering result = PartialOrdering::kEqual;
            [[maybe_unused]] auto compare_args =
                (((result = symbolicCompare(LhsArgs{}, RhsArgs{})),
                  result != PartialOrdering::kEqual) ||
                 ...);
            return result;
          };
      return compare_args_recursively(lhs, rhs);
    }
  }

  // Constants: Compare values
  else if constexpr (lhs_is_constant && rhs_is_constant) {
    if (lhs.value < rhs.value) {
      return PartialOrdering::kLess;
    }
    if (lhs.value > rhs.value) {
      return PartialOrdering::kGreater;
    }
    return PartialOrdering::kEqual;
  }

  else if constexpr (lhs_is_symbol && rhs_is_symbol) {
    if (kMeta<decltype(lhs)> < kMeta<decltype(rhs)>) {
      return PartialOrdering::kLess;
    }
    if (kMeta<decltype(lhs)> > kMeta<decltype(rhs)>) {
      return PartialOrdering::kGreater;
    }
    return PartialOrdering::kEqual;
  }

  else {
    static_assert(false,
                  "Invalid comparison between Symbolic types. Please check the "
                  "implementation.");
  }
}

constexpr auto symbolicLessThan(Symbolic auto lhs, Symbolic auto rhs) -> bool {
  return symbolicCompare(lhs, rhs) == PartialOrdering::kLess;
}

// --- Simplification Rules ---

template <Symbolic S>
constexpr auto simplifySymbol(S sym);

// Evaluate a Symbolic expression if every term is Constant<...>.
template <typename Op, Symbolic... Args>
  requires((match(Args{}, AnyConstant{}) && ...) && sizeof...(Args) > 0)
constexpr auto evalConstantExpr(Expression<Op, Args...> expr) {
  return Constant<evaluate(expr, BinderPack{})>{};
}

template <Symbolic S>
  requires(match(S{}, pow(AnyArg{}, AnyArg{})))
constexpr auto powerIdentities(S expr) {
  constexpr auto base = left(expr);
  constexpr auto exponent = right(expr);

  // x ^ 0 == 1
  if constexpr (match(exponent, 0_c)) {
    return 1_c;
  }
  // x ^ 1 == x
  else if constexpr (match(exponent, 1_c)) {
    return base;
  }
  // 1 ^ x == 1
  else if constexpr (match(base, 1_c)) {
    return 1_c;
  }
  // 0 ^ x == 0
  else if constexpr (match(base, 0_c)) {
    return 0_c;
  }

  // (x ^ a) ^ b == x ^ (a * b)
  else if constexpr (match(expr, pow(pow(AnyArg{}, AnyArg{}), AnyArg{}))) {
    constexpr auto x = left(left(expr));
    constexpr auto a = right(left(expr));
    constexpr auto b = right(expr);
    return simplifySymbol(pow(x, simplifySymbol(a * b)));
  }

  // default
  else {
    return expr;
  }
}

template <Symbolic S>
  requires(match(S{}, AnyArg{} + AnyArg{}))
constexpr auto additionIdentities(S expr) {
  // 0 + x = x
  if constexpr (match(expr, 0_c + AnyArg{})) {
    return right(expr);
  }

  // x + 0 = x
  else if constexpr (match(expr, AnyArg{} + 0_c)) {
    return left(expr);
  }

  // x + x = x * 2
  else if constexpr (match(left(expr), right(expr))) {
    constexpr auto x = left(expr);
    return simplifySymbol(x * 2_c);
  }

  // Canonical Form
  else if constexpr (symbolicLessThan(right(expr), left(expr))) {
    return simplifySymbol(right(expr) + left(expr));
  }

  // If b < c
  // (a + c) + b = (a + b) + c
  else if constexpr (match(expr, (AnyArg{} + AnyArg{}) + AnyArg{}) &&
                     symbolicLessThan(right(expr), right(left(expr)))) {
    constexpr auto a = left(left(expr));
    constexpr auto c = right(left(expr));
    constexpr auto b = right(expr);
    return simplifySymbol(simplifySymbol(a + b) + c);
  }

  // Factoring

  // Note only factor if you get a simpler expression
  // x * a + x = x * (a + 1);
  else if constexpr (match(expr, AnyArg{} * AnyArg{} + AnyArg{}) &&
                     match(left(left(expr)), right(expr))) {
    constexpr auto x = left(left(expr));
    constexpr auto a = right(left(expr));
    constexpr auto coeff = a + 1_c;
    constexpr auto simplified_coeff = simplifySymbol(coeff);
    if constexpr (match(coeff, simplified_coeff)) {
      // The coefficient is unchanged, so we can return the original
      return expr;
    } else {
      return simplifySymbol(x * simplified_coeff);
    }
  }

  // Note only factor if you get a simpler expression
  // x * a + x * b = x * (a + b);
  else if constexpr (match(expr, AnyArg{} * AnyArg{} + AnyArg{} * AnyArg{}) &&
                     match(left(left(expr)), left(right(expr)))) {
    constexpr auto x = left(left(expr));
    constexpr auto a = right(left(expr));
    constexpr auto b = right(right(expr));
    constexpr auto coeff = a + b;
    constexpr auto simplified_coeff = simplifySymbol(coeff);
    if constexpr (match(coeff, simplified_coeff)) {
      // The coefficient is unchanged, so we can return the original
      return expr;
    } else {
      return simplifySymbol(x * simplified_coeff);
    }
  }

  // Reduce the right-hand side if you end up with a shorter result
  // (a + b) + c = a + (b + c)
  else if constexpr (match(expr, (AnyArg{} + AnyArg{}) + AnyArg{})) {
    constexpr auto b = right(left(expr));
    constexpr auto c = right(expr);
    constexpr auto rhs = b + c;
    constexpr auto simplified_rhs = simplifySymbol(rhs);
    if constexpr (match(rhs, simplified_rhs)) {
      // The right-hand side is unchanged, so we return the original
      return expr;
    } else {
      constexpr auto a = left(left(expr));
      return simplifySymbol(a + simplified_rhs);
    }
  }

  // default
  else {
    return expr;
  }
}

template <Symbolic S>
  requires(match(S{}, AnyArg{} * AnyArg{}))
constexpr auto multiplicationIdentities(S expr) {
  // 0 * x = 0
  if constexpr (match(expr, 0_c * AnyArg{})) {
    return 0_c;
  }

  // x * 0 = 0
  else if constexpr (match(expr, AnyArg{} * 0_c)) {
    return 0_c;
  }

  // 1 * x = x
  else if constexpr (match(expr, 1_c * AnyArg{})) {
    return right(expr);
  }

  // x * 1 = x
  else if constexpr (match(expr, AnyArg{} * 1_c)) {
    return left(expr);
  }

  // x * x = x²
  else if constexpr (match(left(expr), right(expr))) {
    return simplifySymbol(pow(left(expr), 2_c));
  }

  // x * xᵃ = xᵃ⁺¹
  else if constexpr (match(expr, AnyArg{} * pow(AnyArg{}, AnyArg{})) and
                     match(left(expr), left(right(expr)))) {
    constexpr auto x = left(expr);
    constexpr auto a = right(right(expr));
    constexpr auto power = simplifySymbol(a + 1_c);
    return simplifySymbol(pow(x, power));
  }

  // xᵃ * x = xᵃ⁺¹
  else if constexpr (match(expr, pow(AnyArg{}, AnyArg{}) * AnyArg{}) and
                     match(left(left(expr)), right(expr))) {
    constexpr auto x = left(left(expr));
    constexpr auto a = right(left(expr));
    constexpr auto power = simplifySymbol(a + 1_c);
    return simplifySymbol(pow(x, power));
  }

  // xᵃ * xᵇ = xᵃ⁺ᵇ
  else if constexpr (match(expr, (pow(AnyArg{}, AnyArg{}) *
                                  pow(AnyArg{}, AnyArg{}))) and
                     (match(left(left(expr)), left(right(expr))))) {
    constexpr auto x = left(left(expr));
    constexpr auto a = right(left(expr));
    constexpr auto b = right(right(expr));
    constexpr auto power = simplifySymbol(a + b);
    return simplifySymbol(pow(x, power));
  }

  // Distributive Property
  // (a + b) * c = a * c + b * c
  else if constexpr (match(expr, (AnyArg{} + AnyArg{}) * AnyArg{})) {
    constexpr auto a = left(left(expr));
    constexpr auto b = right(left(expr));
    constexpr auto c = right(expr);
    constexpr auto lhs = simplifySymbol(a * c);
    constexpr auto rhs = simplifySymbol(b * c);
    return simplifySymbol(lhs + rhs);
  }

  // a * (b + c) = a * b + a * c
  else if constexpr (match(expr, AnyArg{} * (AnyArg{} + AnyArg{}))) {
    constexpr auto a = left(expr);
    constexpr auto b = left(right(expr));
    constexpr auto c = right(right(expr));
    constexpr auto lhs = simplifySymbol(a * b);
    constexpr auto rhs = simplifySymbol(a * c);
    return simplifySymbol(lhs + rhs);
  }

  // Canonical Form
  // if a < b
  // b * a = a * b
  else if constexpr (symbolicLessThan(right(expr), left(expr))) {
    constexpr auto a = left(expr);
    constexpr auto b = right(expr);
    return simplifySymbol(b * a);
  }

  // a * (b * c) = (a * b) * c
  else if constexpr (match(expr, AnyArg{} * (AnyArg{} * AnyArg{}))) {
    constexpr auto a = left(expr);
    constexpr auto b = left(right(expr));
    constexpr auto c = right(right(expr));
    constexpr auto lhs = simplifySymbol(a * b);
    return simplifySymbol(lhs * c);
  }

  // if b < c
  // (a * c) * b = (a * b) * c
  else if constexpr (match(expr, (AnyArg{} * AnyArg{}) * AnyArg{}) and
                     symbolicLessThan(right(expr), right(left(expr)))) {
    constexpr auto a = left(left(expr));
    constexpr auto c = right(left(expr));
    constexpr auto b = right(expr);
    constexpr auto lhs = simplifySymbol(a * b);
    return simplifySymbol(lhs * c);
  }

  // Reduce the right-hand side if you end up with a shorter result
  // (a * b) * c = a * (b * c)
  else if constexpr (match(expr, (AnyArg{} * AnyArg{}) * AnyArg{})) {
    constexpr auto b = right(left(expr));
    constexpr auto c = right(expr);
    constexpr auto rhs = b * c;
    constexpr auto simplified_rhs = simplifySymbol(rhs);
    if constexpr (match(rhs, simplified_rhs)) {
      // The right-hand side is unchanged, so we return the original
      return expr;
    } else {
      constexpr auto a = left(left(expr));
      return simplifySymbol(a * simplified_rhs);
    }
  }

  // default
  else {
    return expr;
  }
}

template <Symbolic S>
  requires(match(S{}, AnyArg{} - AnyArg{}))
constexpr auto subtractionIdentities(S expr) {
  constexpr auto a = left(expr);
  constexpr auto b = right(expr);
  return simplifySymbol(a + simplifySymbol(Constant<-1>{} * b));
}

template <Symbolic S>
  requires(match(S{}, AnyArg{} / AnyArg{}))
constexpr auto divisionIdentities(S expr) {
  constexpr auto a = left(expr);
  constexpr auto b = right(expr);
  return simplifySymbol(a * simplifySymbol(pow(b, Constant<-1>{})));
}

template <Symbolic S>
  requires(match(S{}, exp(AnyArg{})))
constexpr auto expIdentities(S expr) {
  // exp(log(x)) == x
  if constexpr (match(expr, exp(log(AnyArg{})))) {
    return right(operand(expr));
  }

  // e^(log(x)) == x
  else if constexpr (match(expr, log(pow(e, AnyArg{})))) {
    return right(operand(expr));
  }

  else {
    return simplifySymbol(pow(e, operand(expr)));
  }
}

template <Symbolic S>
  requires(match(S{}, log(AnyArg{})))
constexpr auto logIdentities(S expr) {
  // log(1) == 0
  if constexpr (match(expr, log(1_c))) {
    return 0_c;
  }

  // log(e) == 1
  else if constexpr (match(expr, log(e))) {
    return 1_c;
  }

  // log(x^a) == a * log(x)
  else if constexpr (match(expr, log(pow(AnyArg{}, AnyArg{})))) {
    constexpr auto x = left(operand(expr));
    constexpr auto a = right(operand(expr));
    return simplifySymbol(a * simplifySymbol(log(x)));
  }

  // log(a * b) == log(a) + log(b)
  else if constexpr (match(expr, log(AnyArg{} * AnyArg{}))) {
    constexpr auto a = left(operand(expr));
    constexpr auto b = right(operand(expr));
    return simplifySymbol(simplifySymbol(log(a)) + simplifySymbol(log(b)));
  }

  // log(a / b) == log(a) - log(b)
  else if constexpr (match(expr, log(AnyArg{} / AnyArg{}))) {
    constexpr auto a = left(operand(expr));
    constexpr auto b = right(operand(expr));
    return simplifySymbol(simplifySymbol(log(a)) - simplifySymbol(log(b)));
  }

  // log(exp(x)) == x
  else if constexpr (match(expr, log(exp(AnyArg{})))) {
    return operand(operand(expr));
  }

  else {
    return expr;
  }
}

template <Symbolic S>
  requires(match(S{}, sin(AnyArg{})))
constexpr auto sinIdentities(S expr) {
  // sin(π/2) = 1
  if constexpr (match(expr, sin(π * 0.5_c))) {
    return 1_c;
  }
  // sin(π) = 0
  else if constexpr (match(expr, sin(π))) {
    return 0_c;
  }
  // sin(3π/2) = -1
  else if constexpr (match(expr, sin(π * 1.5_c))) {
    return Constant<-1>{};
  }
  // TODO: Make this reduction more general, maybe any negative number?
  // sin(-x) = -sin(x)
  else if constexpr (match(expr, sin(AnyArg{} * Constant<-1>{}))) {
    return Constant<-1>{} * simplifySymbol(sin(operand(expr)));
  }
  // TODO: sin(x + 2π) = sin(x)
  else {
    return expr;
  }
}

template <Symbolic S>
  requires(match(S{}, cos(AnyArg{})))
constexpr auto cosIdentities(S expr) {
  // cos(π/2) = 0
  if constexpr (match(expr, cos(π * 0.5_c))) {
    return 0_c;
  }
  // cos(π) = -1
  else if constexpr (match(expr, cos(π))) {
    return Constant<-1>{};
  }
  // cos(3π/2) = 0
  else if constexpr (match(expr, cos(π * 1.5_c))) {
    return 0_c;
  } else {
    return expr;
  }
}

template <Symbolic S>
  requires(match(S{}, tan(AnyArg{})))
constexpr auto tanIdentities(S expr) {
  // tan(π) = 0
  if constexpr (match(expr, tan(π))) {
    return 0_c;
  } else {
    return expr;
  }
}

// Simplify a single term
template <Symbolic S>
constexpr auto simplifySymbol(S sym) {
  if constexpr (requires { evalConstantExpr(sym); }) {
    return evalConstantExpr(sym);
  }

  else if constexpr (requires { powerIdentities(sym); }) {
    return powerIdentities(sym);
  }

  else if constexpr (requires { additionIdentities(sym); }) {
    return additionIdentities(sym);
  }

  else if constexpr (requires { subtractionIdentities(sym); }) {
    return subtractionIdentities(sym);
  }

  else if constexpr (requires { multiplicationIdentities(sym); }) {
    return multiplicationIdentities(sym);
  }

  else if constexpr (requires { divisionIdentities(sym); }) {
    return divisionIdentities(sym);
  }

  else if constexpr (requires { expIdentities(sym); }) {
    return expIdentities(sym);
  }

  else if constexpr (requires { logIdentities(sym); }) {
    return logIdentities(sym);
  }

  else if constexpr (requires { sinIdentities(sym); }) {
    return sinIdentities(sym);
  }

  else if constexpr (requires { cosIdentities(sym); }) {
    return cosIdentities(sym);
  }

  else {
    return S{};
  }
}

template <typename Op, Symbolic... Args>
constexpr auto simplifyTerms(Expression<Op, Args...>) {
  return Expression{Op{}, simplify(Args{})...};
}

// Public API: Fully simplifies a symbolic expression.
// This function orchestrates the simplification process.
// It typically simplifies arguments first (bottom-up / inside-out),
// then applies rules to the expression with simplified arguments.
template <Symbolic S>
constexpr auto simplify(S sym) {
  if constexpr (requires { simplifyTerms(sym); }) {
    return simplifySymbol(simplifyTerms(sym));
  }

  else {
    return simplifySymbol(sym);
  }
}

// --- To String ---

template <int N>
auto toStringImpl(Constant<N>) {
  if constexpr (N == 0) {
    return StaticString("");
  } else {
    SizeT val = N % 10;
    return toStringImpl(Constant<N / 10>{}) +
           StaticString(static_cast<char>('0' + val));
  }
}

template <int N>
auto toString(Constant<N>) {
  if constexpr (N == 0) {
    return StaticString("0");
  } else if constexpr (N < 0) {
    return StaticString("-") + toStringImpl(Constant<-N>{});
  } else {
    return toStringImpl(Constant<N>{});
  }
}

template <double Orig, double VALUE>
auto toStringFaction(Constant<VALUE>) {
  if constexpr (VALUE / Orig < 0.000001) {
    return StaticString("");
  } else {
    constexpr auto val = VALUE * 10;
    constexpr auto diget = floor(val);
    return StaticString(static_cast<char>('0' + diget)) +
           toStringFaction<Orig>(Constant<val - diget>{});
  }
}

template <double VALUE>
auto toString(Constant<VALUE>) {
  if constexpr (VALUE == 0.0) {
    return StaticString("0.");
  } else if constexpr (VALUE < 0.0) {
    return StaticString("-") + toStringFaction(Constant<-VALUE>{});
  } else {
    return toString(Constant<static_cast<int>(VALUE)>{}) + StaticString(".") +
           toStringFaction<VALUE>(Constant<VALUE - static_cast<int>(VALUE)>{});
  }
}

template <auto VALUE>
auto toString(Constant<VALUE>) {
  return StaticString("<Constant>");
}

template <typename SymbolTag>
auto toString(Symbol<SymbolTag>) {
  return StaticString("Symbol<") +
         toString(Constant<static_cast<int>(kMeta<Symbol<SymbolTag>>)>{}) +
         StaticString(">");
}

template <typename Op, Symbolic... Args>
auto toString(Expression<Op, Args...> expr) {
  // Prefix notation
  if constexpr (Op::kDisplayMode == DisplayMode::kPrefix) {
    return Op::kSymbol + StaticString("(") +
           ((StaticString(" ") + toString(Args{})) + ... + StaticString(")"));
  } else {
    const auto print_first =
        []<Symbolic First, Symbolic... Rest>(Expression<Op, First, Rest...>) {
          return toString(First{});
        };
    const auto print_rest =
        []<Symbolic First, Symbolic... Rest>(Expression<Op, First, Rest...>) {
          return ((StaticString(" ") + Op::kSymbol + StaticString(" ") +
                   toString(Rest{})) +
                  ...);
        };

    return StaticString("(") + print_first(expr) + print_rest(expr) +
           StaticString(")");
  }
}

}  // namespace tempura
