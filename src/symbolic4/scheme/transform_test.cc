// Tests for scheme/transform.h - type-transforming recursion schemes
#include "symbolic4/core.h"
#include "symbolic4/operators.h"
#include "symbolic4/scheme/transform.h"
#include "unit.h"

using namespace tempura;
using namespace tempura::symbolic4;

// ============================================================================
// Test interpreters for transform (catamorphism with deduced return type)
// ============================================================================

// Identity: returns expression unchanged
struct Identity {
  template <typename T>
  static constexpr auto terminal(T t) {
    return t;
  }

  template <typename Op, typename... Children>
  static constexpr auto combine(Children... children) {
    return Expression<Op, Children...>{children...};
  }
};

// NegateConstants: flips sign of integer constants
struct NegateConstants {
  template <typename T>
  static constexpr auto terminal(T t) {
    if constexpr (is_constant_v<T>) {
      return Constant<-T::value>{};
    } else {
      return t;
    }
  }

  template <typename Op, typename... Children>
  static constexpr auto combine(Children... children) {
    return Expression<Op, Children...>{children...};
  }
};

// DoubleConstants: doubles integer constants
struct DoubleConstants {
  template <typename T>
  static constexpr auto terminal(T t) {
    if constexpr (is_constant_v<T>) {
      return Constant<T::value * 2>{};
    } else {
      return t;
    }
  }

  template <typename Op, typename... Children>
  static constexpr auto combine(Children... children) {
    return Expression<Op, Children...>{children...};
  }
};

// ============================================================================
// Test interpreters for paraTransform (paramorphism with deduced return type)
// ============================================================================

// WrapInNeg: wraps each terminal in negation (consistent return type)
struct WrapInNeg {
  template <typename T>
  static constexpr auto terminal(T t) {
    return -t;
  }

  template <typename Op, typename... Pairs>
  static constexpr auto combine(Pairs... pairs) {
    return Expression<Op, decltype(pairs.second)...>{pairs.second...};
  }
};

// OriginalPlusTransformed: for each symbol, returns original + transformed
// This tests that paraTransform provides access to original in combine
struct AddOriginalToTransformed {
  template <typename T>
  static constexpr auto terminal(T t) {
    return t;  // Terminal transformation is identity
  }

  // Combine adds original child to transformed result for binary ops
  template <typename Op, typename L, typename R>
    requires std::is_same_v<Op, AddOp>
  static constexpr auto combine(L l, R r) {
    // For addition, return (l.orig + l.trans) + (r.orig + r.trans)
    return (l.first + l.second) + (r.first + r.second);
  }

  template <typename Op, typename... Pairs>
  static constexpr auto combine(Pairs... pairs) {
    return Expression<Op, decltype(pairs.second)...>{pairs.second...};
  }
};

auto main() -> int {
  Symbol<struct X> x;
  Symbol<struct Y> y;

  // =========================================================================
  // transform tests
  // =========================================================================

  "transform identity terminal"_test = [&] {
    auto result = transform<Identity>(x);
    static_assert(std::is_same_v<decltype(result), Symbol<struct X>>);
  };

  "transform identity constant"_test = [] {
    auto result = transform<Identity>(Constant<42>{});
    static_assert(std::is_same_v<decltype(result), Constant<42>>);
  };

  "transform identity binary"_test = [&] {
    auto expr = x + y;
    auto result = transform<Identity>(expr);
    static_assert(std::is_same_v<decltype(result), decltype(expr)>);
  };

  "transform identity nested"_test = [&] {
    auto expr = sin(x * y) + cos(x);
    auto result = transform<Identity>(expr);
    static_assert(std::is_same_v<decltype(result), decltype(expr)>);
  };

  "transform negate constants"_test = [] {
    auto result = transform<NegateConstants>(Constant<5>{});
    static_assert(std::is_same_v<decltype(result), Constant<-5>>);
  };

  "transform negate in expression"_test = [&] {
    auto expr = x + Constant<3>{};
    auto result = transform<NegateConstants>(expr);
    // x + 3 becomes x + (-3)
    static_assert(
        std::is_same_v<decltype(result), Expression<AddOp, Symbol<X>, Constant<-3>>>);
  };

  "transform double constants"_test = [] {
    auto result = transform<DoubleConstants>(Constant<7>{});
    static_assert(std::is_same_v<decltype(result), Constant<14>>);
  };

  "transform preserves structure"_test = [&] {
    auto expr = (x * Constant<2>{}) + Constant<3>{};
    auto result = transform<DoubleConstants>(expr);
    // (x * 2) + 3 becomes (x * 4) + 6
    using Expected =
        Expression<AddOp, Expression<MulOp, Symbol<X>, Constant<4>>, Constant<6>>;
    static_assert(std::is_same_v<decltype(result), Expected>);
  };

  "transform nullary ops"_test = [] {
    // pi and e should pass through identity unchanged
    constexpr auto result_pi = transform<Identity>(pi);
    constexpr auto result_e = transform<Identity>(e);
    // Just verify they compile and are the same expression type
    static_assert(std::is_same_v<std::remove_const_t<decltype(result_pi)>,
                                 std::remove_const_t<decltype(pi)>>);
    static_assert(std::is_same_v<std::remove_const_t<decltype(result_e)>,
                                 std::remove_const_t<decltype(e)>>);
  };

  // =========================================================================
  // paraTransform tests
  // =========================================================================

  "paraTransform terminal wraps in neg"_test = [&] {
    auto result = paraTransform<WrapInNeg>(x);
    // x becomes -x
    static_assert(std::is_same_v<decltype(result), Expression<NegOp, Symbol<X>>>);
  };

  "paraTransform constant wraps in neg"_test = [] {
    auto result = paraTransform<WrapInNeg>(Constant<5>{});
    static_assert(std::is_same_v<decltype(result), Expression<NegOp, Constant<5>>>);
  };

  "paraTransform in expression"_test = [&] {
    // sin(x) -> sin(-x)
    auto expr = sin(x);
    auto result = paraTransform<WrapInNeg>(expr);
    using Expected = Expression<SinOp, Expression<NegOp, Symbol<X>>>;
    static_assert(std::is_same_v<decltype(result), Expected>);
  };

  "paraTransform nested"_test = [&] {
    // x + y -> (-x) + (-y)
    auto expr = x + y;
    auto result = paraTransform<WrapInNeg>(expr);
    using Expected = Expression<AddOp, Expression<NegOp, Symbol<X>>,
                                Expression<NegOp, Symbol<Y>>>;
    static_assert(std::is_same_v<decltype(result), Expected>);
  };

  "paraTransform accesses original in combine"_test = [&] {
    // x + y with AddOriginalToTransformed should give (x + x) + (y + y)
    auto expr = x + y;
    auto result = paraTransform<AddOriginalToTransformed>(expr);
    // l.first = x, l.second = x (identity terminal)
    // r.first = y, r.second = y
    // result = (x + x) + (y + y)
    using Expected = Expression<AddOp, Expression<AddOp, Symbol<X>, Symbol<X>>,
                                Expression<AddOp, Symbol<Y>, Symbol<Y>>>;
    static_assert(std::is_same_v<decltype(result), Expected>);
  };

  return TestRegistry::result();
}
