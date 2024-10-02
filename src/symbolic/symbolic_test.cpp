#include "symbolic/symbolic.h"

#include "unit.h"

using namespace tempura;
using namespace tempura::symbolic;

class Plus {
 public:
  static inline constexpr std::string_view kSymbol = "+"sv;
  static inline constexpr DisplayMode kDisplayMode = DisplayMode::kInfix;

  template <typename... Args>
  static constexpr auto operator()(Args... args) {
    return (args + ...);
  }
};
static_assert(Operator<Plus>);

auto main() -> int {
  "Constants"_test = [] {
    constexpr Constant<3> a;
    constexpr Constant<4> b;
    static_assert(a == a);
    static_assert(a != b);
  };

  "Comparision"_test = [] {
    constexpr Constant<3> a;
    constexpr Constant<4> b;
    static_assert(a < b);
    static_assert(b > a);

    TEMPURA_SYMBOLS(d, c);
    static_assert(c < d);
    static_assert(d > c);
  };

  "Symbols"_test = [] {
    TEMPURA_SYMBOLS(a, b);
    static_assert(a == a);
    static_assert(a != b);
  };

  "Substitution"_test = [] {
    TEMPURA_SYMBOLS(a, b);
    static_assert(SymbolicExpression(Plus{}, a, a)(Substitution{a = 5}) == 10);
    static_assert(
        SymbolicExpression(Plus{}, a, b)(Substitution{a = 5, b = 2}) == 7);
  };

  return TestRegistry::result();
}
