#pragma once

#include <cmath>

#include "core.h"
#include "meta/function_objects.h"

// String conversion for symbolic expressions

namespace tempura {

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
