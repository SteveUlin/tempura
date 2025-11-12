#include "symbolic3/symbolic3.h"

#include "unit.h"

using namespace tempura;  // For TestRegistry, StaticString, DisplayMode, _test
using namespace tempura::symbolic3;  // For DisplayTraits and Precedence

auto main() -> int {
  "DisplayTraits: Binary arithmetic operators have correct properties"_test =
      [] {
        // Addition
        static_assert(DisplayTraits<AddOp>::mode == DisplayMode::kInfix,
                      "AddOp mode");
        static_assert(DisplayTraits<AddOp>::precedence == Precedence::kAddition,
                      "AddOp precedence");

        // Subtraction
        static_assert(DisplayTraits<SubOp>::mode == DisplayMode::kInfix,
                      "SubOp mode");
        static_assert(DisplayTraits<SubOp>::precedence == Precedence::kAddition,
                      "SubOp precedence");

        // Multiplication
        static_assert(DisplayTraits<MulOp>::mode == DisplayMode::kInfix,
                      "MulOp mode");
        static_assert(
            DisplayTraits<MulOp>::precedence == Precedence::kMultiplication,
            "MulOp precedence");

        // Division
        static_assert(DisplayTraits<DivOp>::mode == DisplayMode::kInfix,
                      "DivOp mode");
        static_assert(
            DisplayTraits<DivOp>::precedence == Precedence::kMultiplication,
            "DivOp precedence");

        // Power
        static_assert(DisplayTraits<PowOp>::mode == DisplayMode::kInfix,
                      "PowOp mode");
        static_assert(DisplayTraits<PowOp>::precedence == Precedence::kPower,
                      "PowOp precedence");
      };

  "DisplayTraits: Unary operators have correct properties"_test = [] {
    // Negation
    static_assert(DisplayTraits<NegOp>::symbol == StaticString("-"),
                  "NegOp symbol");
    static_assert(DisplayTraits<NegOp>::mode == DisplayMode::kPrefix,
                  "NegOp mode");
    static_assert(DisplayTraits<NegOp>::precedence == Precedence::kUnary,
                  "NegOp precedence");
  };

  "DisplayTraits: Trigonometric functions have correct properties"_test = [] {
    // Sin
    static_assert(DisplayTraits<SinOp>::symbol == StaticString("sin"),
                  "SinOp symbol");
    static_assert(DisplayTraits<SinOp>::mode == DisplayMode::kPrefix,
                  "SinOp mode");
    static_assert(DisplayTraits<SinOp>::precedence == Precedence::kUnary,
                  "SinOp precedence");

    // Cos
    static_assert(DisplayTraits<CosOp>::symbol == StaticString("cos"),
                  "CosOp symbol");
    static_assert(DisplayTraits<CosOp>::mode == DisplayMode::kPrefix,
                  "CosOp mode");
    static_assert(DisplayTraits<CosOp>::precedence == Precedence::kUnary,
                  "CosOp precedence");

    // Tan
    static_assert(DisplayTraits<TanOp>::symbol == StaticString("tan"),
                  "TanOp symbol");
    static_assert(DisplayTraits<TanOp>::mode == DisplayMode::kPrefix,
                  "TanOp mode");
    static_assert(DisplayTraits<TanOp>::precedence == Precedence::kUnary,
                  "TanOp precedence");
  };

  "DisplayTraits: Hyperbolic functions have correct properties"_test = [] {
    // Sinh
    static_assert(DisplayTraits<SinhOp>::symbol == StaticString("sinh"),
                  "SinhOp symbol");
    static_assert(DisplayTraits<SinhOp>::mode == DisplayMode::kPrefix,
                  "SinhOp mode");
    static_assert(DisplayTraits<SinhOp>::precedence == Precedence::kUnary,
                  "SinhOp precedence");

    // Cosh
    static_assert(DisplayTraits<CoshOp>::symbol == StaticString("cosh"),
                  "CoshOp symbol");
    static_assert(DisplayTraits<CoshOp>::mode == DisplayMode::kPrefix,
                  "CoshOp mode");
    static_assert(DisplayTraits<CoshOp>::precedence == Precedence::kUnary,
                  "CoshOp precedence");

    // Tanh
    static_assert(DisplayTraits<TanhOp>::symbol == StaticString("tanh"),
                  "TanhOp symbol");
    static_assert(DisplayTraits<TanhOp>::mode == DisplayMode::kPrefix,
                  "TanhOp mode");
    static_assert(DisplayTraits<TanhOp>::precedence == Precedence::kUnary,
                  "TanhOp precedence");
  };

  "DisplayTraits: Exponential and logarithmic functions have correct "
  "properties"_test =
      [] {
        // Exp
        static_assert(DisplayTraits<ExpOp>::symbol == StaticString("exp"),
                      "ExpOp symbol");
        static_assert(DisplayTraits<ExpOp>::mode == DisplayMode::kPrefix,
                      "ExpOp mode");
        static_assert(DisplayTraits<ExpOp>::precedence == Precedence::kUnary,
                      "ExpOp precedence");

        // Log
        static_assert(DisplayTraits<LogOp>::symbol == StaticString("log"),
                      "LogOp symbol");
        static_assert(DisplayTraits<LogOp>::mode == DisplayMode::kPrefix,
                      "LogOp mode");
        static_assert(DisplayTraits<LogOp>::precedence == Precedence::kUnary,
                      "LogOp precedence");

        // Sqrt
        static_assert(DisplayTraits<SqrtOp>::symbol == StaticString("√"),
                      "SqrtOp symbol");
        static_assert(DisplayTraits<SqrtOp>::mode == DisplayMode::kPrefix,
                      "SqrtOp mode");
        static_assert(DisplayTraits<SqrtOp>::precedence == Precedence::kUnary,
                      "SqrtOp precedence");
      };

  "DisplayTraits: Mathematical constants have correct properties"_test = [] {
    // Pi
    static_assert(DisplayTraits<PiOp>::symbol == StaticString("π"),
                  "PiOp symbol");
    static_assert(DisplayTraits<PiOp>::mode == DisplayMode::kPrefix,
                  "PiOp mode");
    static_assert(DisplayTraits<PiOp>::precedence == Precedence::kAtomic,
                  "PiOp precedence");

    // E
    static_assert(DisplayTraits<EOp>::symbol == StaticString("e"),
                  "EOp symbol");
    static_assert(DisplayTraits<EOp>::mode == DisplayMode::kPrefix,
                  "EOp mode");
    static_assert(DisplayTraits<EOp>::precedence == Precedence::kAtomic,
                  "EOp precedence");
  };

  "DisplayTraits: Precedence hierarchy is correct"_test = [] {
    // Verify precedence ordering: atomic > unary > power > mul > add
    static_assert(Precedence::kAtomic > Precedence::kUnary,
                  "Atomic > Unary precedence");
    static_assert(Precedence::kUnary > Precedence::kPower,
                  "Unary > Power precedence");
    static_assert(Precedence::kPower > Precedence::kMultiplication,
                  "Power > Multiplication precedence");
    static_assert(Precedence::kMultiplication > Precedence::kAddition,
                  "Multiplication > Addition precedence");

    // Verify specific operators respect the hierarchy
    static_assert(DisplayTraits<MulOp>::precedence >
                      DisplayTraits<AddOp>::precedence,
                  "Mul precedence > Add precedence");
    static_assert(DisplayTraits<PowOp>::precedence >
                      DisplayTraits<MulOp>::precedence,
                  "Pow precedence > Mul precedence");
    static_assert(DisplayTraits<NegOp>::precedence >
                      DisplayTraits<PowOp>::precedence,
                  "Neg precedence > Pow precedence");
  };

  "DisplayTraits: Same-precedence operators are consistent"_test = [] {
    // Addition and subtraction have the same precedence
    static_assert(DisplayTraits<AddOp>::precedence ==
                      DisplayTraits<SubOp>::precedence,
                  "Add and Sub have same precedence");

    // Multiplication and division have the same precedence
    static_assert(DisplayTraits<MulOp>::precedence ==
                      DisplayTraits<DivOp>::precedence,
                  "Mul and Div have same precedence");

    // All trig functions have the same precedence
    static_assert(DisplayTraits<SinOp>::precedence ==
                      DisplayTraits<CosOp>::precedence,
                  "Sin and Cos have same precedence");
    static_assert(DisplayTraits<SinOp>::precedence ==
                      DisplayTraits<TanOp>::precedence,
                  "Sin and Tan have same precedence");

    // All hyperbolic functions have the same precedence
    static_assert(DisplayTraits<SinhOp>::precedence ==
                      DisplayTraits<CoshOp>::precedence,
                  "Sinh and Cosh have same precedence");
    static_assert(DisplayTraits<SinhOp>::precedence ==
                      DisplayTraits<TanhOp>::precedence,
                  "Sinh and Tanh have same precedence");

    // All transcendental functions share unary precedence
    static_assert(DisplayTraits<ExpOp>::precedence ==
                      DisplayTraits<LogOp>::precedence,
                  "Exp and Log have same precedence");
    static_assert(DisplayTraits<ExpOp>::precedence == Precedence::kUnary,
                  "Transcendental functions use unary precedence");
  };

  return TestRegistry::result();
}
