#pragma once

#include <format>
#include <string>

#include "src/symbolic/symbolic.h"

namespace tempura::symbolic {

template <auto ID, BinderT... Binders>
auto toString(Symbol<ID>, const Substitution<Binders...>& substitution) -> std::string {
  return std::string(substitution[Symbol<ID>{}]);
}

template <auto VALUE, BinderT... Binders>
auto toString(Constant<VALUE>, const Substitution<Binders...>& substitution) -> std::string {
  return std::to_string(VALUE);
}

template <BinderT... Binders>
auto wrap(Symbolic auto expr, const Substitution<Binders...>& substitution) -> std::string {
  return toString(expr, substitution);
}

template<typename Op, Symbolic... Args, BinderT... Binders>
requires (sizeof...(Args) >= 1) && (Op::kDisplayMode == DisplayMode::kInfix)
auto wrap(SymbolicExpression<Op, Args...> expr, const Substitution<Binders...>& substitution) -> std::string {
  return std::format("({})", toString(expr, substitution));
};

template <typename Op, Symbolic... Args, BinderT... Binders>
auto toString(SymbolicExpression<Op, Args...>, const Substitution<Binders...>& substitution) -> std::string {
  constexpr TypeList<Args...> args;
  if constexpr (args.size() == 0) {
    return std::string{Op::kSymbol};
  } else if constexpr (Op::kDisplayMode == DisplayMode::kInfix) {
    std::string out;
    out.append(wrap(args.head(), substitution));
    args.tail() >>= [&](auto... rest) {
      (out.append(std::format(" {} {}", Op::kSymbol, wrap(rest, substitution))),
        ...);
    };
    return out;
  } else if constexpr (Op::kDisplayMode == DisplayMode::kPrefix) {
    std::string out;
    out.append(toString(args.head(), substitution));
    args.tail() >>= [&](auto... rest) {
      (out.append(std::format(", {}", toString(rest, substitution))),
        ...);
    };
    return std::format("{}({})", Op::kSymbol, out);
  }
  std::unreachable();
}

}  // namespace tempura::symbolic

