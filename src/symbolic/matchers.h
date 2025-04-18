#pragma once

#include "symbolic/symbolic.h"
#include "type_list.h"

namespace tempura::symbolic {

template <Matcher Lhs, Matcher Rhs>
consteval auto match(Lhs /*unused*/, Rhs /*unused*/) {
  return false;
}

template <Matcher Arg>
consteval auto match(Arg /*unused*/, Arg /*unused*/) {
  return true;
}

struct Any {};

template <>
inline constexpr bool internal::MatcherTypeTrait<Any> = true;

template <Matcher Rhs>
consteval auto match(Any /*unused*/, Rhs /*unused*/) {
  return true;
}

template <Matcher Lhs>
consteval auto match(Lhs /*unused*/, Any /*unused*/) {
  return true;
}

struct AnyConstant {};

template <>
inline constexpr bool internal::MatcherTypeTrait<AnyConstant> = true;

template <auto VALUE>
consteval auto match(AnyConstant /*unused*/, Constant<VALUE> /*unused*/) {
  return true;
}

template <auto VALUE>
consteval auto match(Constant<VALUE> /*unused*/, AnyConstant /*unused*/) {
  return true;
}

struct AnyConstantExpr {};

template <>
inline constexpr bool internal::MatcherTypeTrait<AnyConstantExpr> = true;

template <Operator Op, auto... VALUES>
consteval auto match(AnyConstantExpr /*unused*/,
                     SymbolicExpression<Op, Constant<VALUES>...> /*unused*/) {
  return true;
}

struct AnySymbol {};

template <>
inline constexpr bool internal::MatcherTypeTrait<AnySymbol> = true;

template <StringLiteral name, auto id>
consteval auto match(AnySymbol /*unused*/,
                     Symbol<name, id> /*unused*/) -> bool {
  return true;
}

template <StringLiteral name, auto id>
consteval auto match(Symbol<name, id> /*unused*/,
                     AnySymbol /*unused*/) -> bool {
  return true;
}

struct AnyNTerms {};

template <>
inline constexpr bool internal::MatcherTypeTrait<AnyNTerms> = true;

struct AnyOp {
  static inline constexpr std::string_view kSymbol = "AnyOp"sv;
  static inline constexpr DisplayMode kDisplayMode = DisplayMode::kInfix;

  template <typename... Args>
  static constexpr auto operator()(Args... /*unused*/) {
    static_assert(false);
    std::unreachable();
  }
};
static_assert(Operator<AnyOp>);

template <typename... Args>
consteval auto anyOp(Args... args) {
  return MatcherExpression{AnyOp{}, args...};
}

namespace internal {

template <Matcher... Lhs, Matcher... Rhs>
consteval auto cmpTypeLists(TypeList<Lhs...> lhs, TypeList<Rhs...> rhs) {
  if constexpr (lhs.empty and rhs.empty) {
    return true;
  } else if constexpr (lhs.empty) {
    return (rhs.size == 1) && std::is_same_v<decltype(rhs.head()), AnyNTerms>;
  } else if constexpr (rhs.empty) {
    return (lhs.size == 1) && std::is_same_v<decltype(lhs.head()), AnyNTerms>;
  } else if constexpr (std::is_same_v<decltype(lhs.head()), AnyNTerms> ||
                       std::is_same_v<decltype(rhs.head()), AnyNTerms>) {
    return true;
  } else if constexpr (!match(lhs.head(), rhs.head())) {
    return false;
  } else {
    return cmpTypeLists(lhs.tail(), rhs.tail());
  }
}

}  // namespace internal

template <typename OpLhs, typename OpRhs, Symbolic... TermsLhs,
          Matcher... TermsRhs>
consteval auto match(SymbolicExpression<OpLhs, TermsLhs...> /*unused*/,
                     MatcherExpression<OpRhs, TermsRhs...> /*unused*/) -> bool {
  if constexpr (!std::is_same_v<OpLhs, AnyOp> &&
                !std::is_same_v<AnyOp, OpRhs> &&
                !std::is_same_v<OpLhs, OpRhs>) {
    return false;
  } else {
    return internal::cmpTypeLists(TypeList<TermsLhs...>{},
                                  TypeList<TermsRhs...>{});
  }
}

template <typename OpLhs, typename OpRhs, Matcher... TermsLhs,
          Symbolic... TermsRhs>
consteval auto match(MatcherExpression<OpLhs, TermsLhs...> /*unused*/,
                     SymbolicExpression<OpRhs, TermsRhs...> /*unused*/)
    -> bool {
  if constexpr (!std::is_same_v<OpLhs, AnyOp> &&
                !std::is_same_v<AnyOp, OpRhs> &&
                !std::is_same_v<OpLhs, OpRhs>) {
    return false;
  } else {
    return internal::cmpTypeLists(TypeList<TermsLhs...>{},
                                  TypeList<TermsRhs...>{});
  }
}

template <typename OpLhs, typename OpRhs, Matcher... TermsLhs,
          Matcher... TermsRhs>
  requires(!(std::is_same_v<TermsLhs, TermsRhs> && ...))
consteval auto match(MatcherExpression<OpLhs, TermsLhs...> /*unused*/,
                     MatcherExpression<OpRhs, TermsRhs...> /*unused*/) -> bool {
  if constexpr (!std::is_same_v<OpLhs, AnyOp> &&
                !std::is_same_v<AnyOp, OpRhs> &&
                !std::is_same_v<OpLhs, OpRhs>) {
    return false;
  } else {
    return internal::cmpTypeLists(TypeList<TermsLhs...>{},
                                  TypeList<TermsRhs...>{});
  }
}

template <typename T, typename U>
concept Matching = Symbolic<T> && Matcher<U> && match(T{}, U{});

template <typename T, typename Op, typename... Args>
concept MatchingExpr =
    (Symbolic<T> && Operator<Op> && (true && ... && Matcher<Args>) &&
     match(T{}, MatcherExpression<Op, Args...>{}));

}  // namespace tempura::symbolic
