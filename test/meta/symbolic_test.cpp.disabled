#include "meta/symbolic.h"

#include <memory>
#include <print>

#include "type_id.h"
#include "unit.h"

using namespace tempura;

template <typename T>
struct fail;

auto main() -> int {
  "matching"_test = [] {
    // Test the basic functionality of the `Symbol` class.
    constexpr Symbol a{};
    constexpr Symbol b{};

    static_assert(!match(a, b),
                  "Symbols should not match if they are different");

    static_assert(match(a, a), "Symbols should match if they are the same");

    static_assert(match(3_c, 3_c),
                  "Constants should match if they are the same");

    static_assert(!match(3_c, 2_c),
                  "Constants should not match if they are different");

    static_assert(!match(a, 2_c), "Symbols should not match constants");

    static_assert(!match(2_c, a), "Constants should not match symbols");

    static_assert(match(a + b, a + b),
                  "Expressions should match if they are the same");

    static_assert(!match(a + b, a * b),
                  "Expressions should not match if they are different");

    static_assert(
        !match(a + b, a + a),
        "Expressions should not match if their structure is different");

    static_assert(match(a + b, a + AnyArg{}),
                  "Expressions should match if one side is a wildcard");

    static_assert(match(AnyArg{}, b),
                  "Expressions should match if one side is a wildcard");

    static_assert(match(a + b, AnyArg{}),
                  "Expressions should match if one side is a wildcard");

    static_assert(match(a + AnyArg{}, a + b),
                  "Expressions should match if one side is a wildcard");
  };

  "evaluation"_test = [] {
    // Define example symbols.
    constexpr Symbol a{};
    constexpr Symbol b{};

    // Test basic evaluation.
    static_assert(evaluate(100_c, BinderPack{}) == 100);

    static_assert(evaluate(a, BinderPack{a = 5}) == 5);

    static_assert(evaluate(b, BinderPack{b = 10}) == 10);

    static_assert(evaluate(a + b, BinderPack{a = 5, b = 10}) == 15);

    static_assert(evaluate(a - b, BinderPack{a = 5, b = 10}) == -5);

    static_assert(evaluate(a * (b + 1_c), BinderPack{a = 5, b = 10}) == 55);
  };

  "function eval"_test = [] {
    constexpr Symbol a{};

    static_assert(
        evaluate(exp(a), BinderPack{a = 1}) == ExpOp{}(1),
        "Expression should evaluate to the exponential of itsbound value");

    static_assert(
        evaluate(log(a), BinderPack{a = 10}) == LogOp{}(10),
        "Expression should evaluate to the logarithm of its bound value");
  };

  "eval constant expr"_test = [] {
    // Test the evaluation of constant expressions.
    static_assert(match(evalConstantExpr(0_c + 0_c), 0_c));

    static_assert(match(evalConstantExpr(1_c + 2_c), 3_c));

    static_assert(match(evalConstantExpr(3_c - 1_c), 2_c));
  };

  "simplify constant expressiosn"_test = [] {
    static_assert(match(simplify(0_c + 0_c), 0_c));

    static_assert(match(simplify(1_c + 2_c), 3_c));

    static_assert(match(simplify(3_c - 1_c), 2_c));

    static_assert(match(simplify(3_c * 1_c), 3_c));

    static_assert(match(simplify(3_c / 1_c), 3_c));

    static_assert(match(simplify(3_c % 1_c), 0_c));

    // Function unit tests
    static_assert(match(simplify(cosh(1_c)), Constant<CoshOp{}(1)>{}));
  };

  "Strict Ordering"_test = [] {
    Symbol a;
    Symbol b;
    Symbol c;

    static_assert(symbolicCompare(a, 1_c) == PartialOrdering::kLess);
    static_assert(symbolicCompare(1_c, a) == PartialOrdering::kGreater);

    // Expressions are less than symbols
    static_assert(symbolicCompare(a + b, a) == PartialOrdering::kLess);
    static_assert(symbolicCompare(a, a + b) == PartialOrdering::kGreater);

    // Constants
    static_assert(symbolicCompare(1_c, 2_c) == PartialOrdering::kLess);
    static_assert(symbolicCompare(2_c, 1_c) == PartialOrdering::kGreater);
    static_assert(symbolicCompare(1_c, 1_c) == PartialOrdering::kEqual);

    // Symbols are compared by which typeid is generated first
    static_assert(symbolicCompare(a, b) == PartialOrdering::kLess);
    static_assert(symbolicCompare(b, a) == PartialOrdering::kGreater);
    static_assert(symbolicCompare(a, a) == PartialOrdering::kEqual);

    // Expressions are compared by their operator
    static_assert(symbolicCompare(a + b, a - b) == PartialOrdering::kLess);
    static_assert(symbolicCompare(a - b, a + b) == PartialOrdering::kGreater);

    // And then by their operands
    static_assert(symbolicCompare(a + b, a + c) == PartialOrdering::kLess);
    static_assert(symbolicCompare(a + c, a + b) == PartialOrdering::kGreater);
  };

  "power identities"_test = [] {
    Symbol x;
    static_assert(match(simplify(pow(x, 0_c)), 1_c));
    static_assert(match(simplify(pow(x, 1_c)), x));
    static_assert(match(simplify(pow(1_c, x)), 1_c));
    static_assert(match(simplify(pow(0_c, x)), 0_c));
    static_assert(match(simplify(pow(x, 2_c)), pow(x, 2_c)));
    static_assert(match(simplify(pow(pow(x, 2_c), 3_c)), pow(x, 6_c)));
  };

  "addition identities"_test = [] {
    Symbol x;
    Symbol y;
    Symbol z;
    static_assert(match(simplify(0_c + x), x));
    static_assert(match(simplify(x + 0_c), x));
    static_assert(match(simplify(x + x), x * 2_c));
    static_assert(match(simplify(x * 3_c + x), x * 4_c));
    static_assert(match(simplify(x + x * 3_c), x * 4_c));
    static_assert(match(simplify(x * 2_c +  x * 3_c), x * 5_c));

    // static_assert(match(simplify(x + y), y + x));
    static_assert(symbolicLessThan(x, y));
    static_assert(symbolicLessThan(x, z));
    static_assert(symbolicLessThan(y, z));
    static_assert(match(simplify(z + y), y + z));
    static_assert(match(simplify(y + z), y + z));
    static_assert(match(simplify(x + (y + z)), (x + y) + z));
    static_assert(match(simplify(z + (x + y)), (x + y) + z));
    static_assert(match(simplify((z + y) + x), (x + y) + z));
    static_assert(match(simplify((z + x) + y), (x + y) + z));
  };

  "multiplication identities"_test = [] {
    Symbol x;
    Symbol y;
    static_assert(match(simplify(0_c * x), 0_c));
    static_assert(match(simplify(x * 0_c), 0_c));

    static_assert(match(simplify(x * 1_c), x));
    static_assert(match(simplify(1_c * x), x));

    static_assert(match(simplify(x * 2_c), x * 2_c));
    static_assert(match(simplify(2_c * x), x * 2_c));
    static_assert(match(simplify(10_c * (10_c * x)), x * 100_c));
    static_assert(match(simplify((10_c * x) * 10_c), x * 100_c));

    static_assert(match(simplify(pow(x, 2_c) * pow(x, 3_c)), pow(x, 5_c)));

    static_assert(match(simplify(y * x), x * y));
    static_assert(match(simplify(x * y), x * y));

    static_assert(match(simplify(x * x), pow(x, 2_c)));
    static_assert(match(simplify(x * x * x), pow(x, 3_c)));
    static_assert(match(simplify(x * x * y), pow(x, 2_c) * y));
    static_assert(match(simplify(x * y * y), x * pow(y, 2_c)));
    static_assert(match(simplify(x * y * x), pow(x, 2_c) * y));
    static_assert(match(simplify(x * y * x * y), pow(x, 2_c) * pow(y, 2_c)));

  };

  "subtraction identities"_test = [] {
    Symbol x;
    Symbol y;
    static_assert(match(simplify(x - 0_c), x));
    static_assert(match(simplify(0_c - x), x * Constant<-1>{}));

    static_assert(match(simplify(x - x), 0_c));
    static_assert(match(simplify(x - y), y * Constant<-1>{} + x));
    static_assert(match(simplify(y - x), x * Constant<-1>{} + y));
  };

  "division identities"_test = [] {
    Symbol x;
    Symbol y;
    static_assert(match(simplify(x / 1_c), x));
    static_assert(match(simplify(1_c / x), pow(x, Constant<-1>{})));
    static_assert(match(simplify(x / x), 1_c));
    static_assert(match(simplify(x / y), x * pow(y, Constant<-1>{})));
  };

  "logarithm identities"_test = [] {
    Symbol x;
    Symbol y;

    static_assert(match(simplify(log(1_c)), 0_c));

    static_assert(match(simplify(log(e)), 1_c));
    static_assert(match(simplify(log(x * y)), log(x) + log(y)));
    static_assert(match(simplify(log(pow(x, y))), log(x) * y));
    static_assert(match(simplify(log(x / y)), log(y) * Constant<-1>{} + log(x)));
    static_assert(match(simplify(log(pow(x, 2_c))), log(x) * 2_c));
  };

  "exponential identities"_test = [] {
    Symbol x;
    Symbol y;
    static_assert(match(simplify(exp(x * y)), pow(e, x * y)));
  };

  "sin identities"_test = [] {
    Symbol x;
    static_assert(match(simplify(sin(0_c)), 0_c));
    static_assert(match(simplify(sin(π * 0.5_c)), 1_c));
    static_assert(match(simplify(sin(π)), 0_c));
    static_assert(match(simplify(sin(π * 1.5_c)), Constant<-1>{}));
    // static_assert(match(simplify(sin(2_c * π)), 0_c));
    // static_assert(match(simplify(sin(x + π)), -sin(x)));
    // static_assert(match(simplify(sin(x + 2_c * π)), sin(x)));
    // static_assert(match(simplify(sin(-x)), -sin(x)));
  };

  Symbol x;
  Symbol y;
  std::println("{}", toString(x + y + 3.14_c).chars_);

  return 0;
}
