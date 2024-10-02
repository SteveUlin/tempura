#pragma once

#include <format>
#include <string>

#include "symbolic/symbolic.h"

namespace tempura::symbolic {

template <StringLiteral name, auto ID>
auto toString(Symbol<name, ID> /*unused*/) {
  return std::string(name.value);
}

template <auto VALUE>
auto toString(Constant<VALUE> /*unused*/) {
  return std::to_string(VALUE);
}

auto wrap(Symbolic auto expr) { return toString(expr); }

template <typename Op, Symbolic... Args>
  requires(sizeof...(Args) >= 1) && (Op::kDisplayMode == DisplayMode::kInfix)
auto wrap(SymbolicExpression<Op, Args...> expr) {
  return std::format("({})", toString(expr));
};

auto wrapIfNotMatching(auto /*unused*/, Symbolic auto expr) {
  return toString(expr);
}

template <typename OpA, typename Op, Symbolic... Args>
  requires(sizeof...(Args) >= 1) && (Op::kDisplayMode == DisplayMode::kInfix)
auto wrapIfNotMatching(OpA op, SymbolicExpression<Op, Args...> expr) {
  if (op == expr.op()) {
    return toString(expr);
  }
  return std::format("({})", toString(expr));
};

template <typename Op, Symbolic... Args>
auto toString(SymbolicExpression<Op, Args...> /*unused*/) -> std::string {
  using ArgsList = TypeList<Args...>;
  if constexpr (ArgsList::empty) {
    return std::string{Op::kSymbol};
  } else if constexpr (Op::kDisplayMode == DisplayMode::kInfix) {
    std::string out;
    out.append(wrap(typename ArgsList::HeadType{}));
    [&]<typename... Rest>(TypeList<Rest...> /*unused*/) {
      (out.append(
           std::format(" {} {}", Op::kSymbol, wrapIfNotMatching(Op{}, Rest{}))),
       ...);
    }(typename ArgsList::TailType{});
    return out;
  } else if constexpr (Op::kDisplayMode == DisplayMode::kPrefix) {
    std::string out;
    out.append(toString(typename ArgsList::HeadType{}));
    [&]<typename... Rest>(TypeList<Rest...> /*unused*/) {
      (out.append(std::format(", {}", toString(Rest{}))), ...);
    }(typename ArgsList::TailType{});
    return std::format("{}({})", Op::kSymbol, out);
  }
  std::unreachable();
}

}  // namespace tempura::symbolic

namespace std {
auto operator<<(std::ostream& os,
                tempura::symbolic::Symbolic auto expr) -> std::ostream& {
  return os << tempura::symbolic::toString(expr);
}
}  // namespace std
