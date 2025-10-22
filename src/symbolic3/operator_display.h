#pragma once

#include "meta/function_objects.h"

// Display traits for symbolic operators
// Separates presentation concerns from operator evaluation logic
//
// Design principle: Operators are pure function objects (like std::plus).
// Display properties (symbol, precedence, notation style) are defined separately.

namespace tempura::symbolic3 {

// Import operator types from tempura namespace
using tempura::AddOp;
using tempura::SubOp;
using tempura::MulOp;
using tempura::DivOp;
using tempura::PowOp;
using tempura::NegOp;
using tempura::SinOp;
using tempura::CosOp;
using tempura::TanOp;
using tempura::SinhOp;
using tempura::CoshOp;
using tempura::TanhOp;
using tempura::ExpOp;
using tempura::LogOp;
using tempura::SqrtOp;
using tempura::PiOp;
using tempura::EOp;

// Precedence levels (higher = binds more tightly)
// Following standard mathematical precedence conventions
namespace Precedence {
constexpr int kAddition = 10;       // +, -
constexpr int kMultiplication = 20; // *, /
constexpr int kPower = 30;          // ^
constexpr int kUnary = 40;          // unary -, function calls
constexpr int kAtomic = 50;         // symbols, constants
}  // namespace Precedence

// Display traits template - default values for unknown operators
template <typename Op>
struct DisplayTraits {
  static constexpr StaticString symbol = "?";
  static constexpr DisplayMode mode = DisplayMode::kPrefix;
  static constexpr int precedence = 0;  // Lowest precedence by default
};

// ============================================================================
// Binary Arithmetic Operators
// ============================================================================

template <>
struct DisplayTraits<AddOp> {
  static constexpr StaticString symbol = "+";
  static constexpr DisplayMode mode = DisplayMode::kInfix;
  static constexpr int precedence = Precedence::kAddition;
};

template <>
struct DisplayTraits<SubOp> {
  static constexpr StaticString symbol = "-";
  static constexpr DisplayMode mode = DisplayMode::kInfix;
  static constexpr int precedence = Precedence::kAddition;
};

template <>
struct DisplayTraits<MulOp> {
  static constexpr StaticString symbol = "*";
  static constexpr DisplayMode mode = DisplayMode::kInfix;
  static constexpr int precedence = Precedence::kMultiplication;
};

template <>
struct DisplayTraits<DivOp> {
  static constexpr StaticString symbol = "/";
  static constexpr DisplayMode mode = DisplayMode::kInfix;
  static constexpr int precedence = Precedence::kMultiplication;
};

template <>
struct DisplayTraits<PowOp> {
  static constexpr StaticString symbol = "^";
  static constexpr DisplayMode mode = DisplayMode::kInfix;
  static constexpr int precedence = Precedence::kPower;
};

// ============================================================================
// Unary Operators
// ============================================================================

template <>
struct DisplayTraits<NegOp> {
  static constexpr StaticString symbol = "-";
  static constexpr DisplayMode mode = DisplayMode::kPrefix;
  static constexpr int precedence = Precedence::kUnary;
};

// ============================================================================
// Trigonometric Functions
// ============================================================================

template <>
struct DisplayTraits<SinOp> {
  static constexpr StaticString symbol = "sin";
  static constexpr DisplayMode mode = DisplayMode::kPrefix;
  static constexpr int precedence = Precedence::kUnary;
};

template <>
struct DisplayTraits<CosOp> {
  static constexpr StaticString symbol = "cos";
  static constexpr DisplayMode mode = DisplayMode::kPrefix;
  static constexpr int precedence = Precedence::kUnary;
};

template <>
struct DisplayTraits<TanOp> {
  static constexpr StaticString symbol = "tan";
  static constexpr DisplayMode mode = DisplayMode::kPrefix;
  static constexpr int precedence = Precedence::kUnary;
};

// ============================================================================
// Hyperbolic Functions
// ============================================================================

template <>
struct DisplayTraits<SinhOp> {
  static constexpr StaticString symbol = "sinh";
  static constexpr DisplayMode mode = DisplayMode::kPrefix;
  static constexpr int precedence = Precedence::kUnary;
};

template <>
struct DisplayTraits<CoshOp> {
  static constexpr StaticString symbol = "cosh";
  static constexpr DisplayMode mode = DisplayMode::kPrefix;
  static constexpr int precedence = Precedence::kUnary;
};

template <>
struct DisplayTraits<TanhOp> {
  static constexpr StaticString symbol = "tanh";
  static constexpr DisplayMode mode = DisplayMode::kPrefix;
  static constexpr int precedence = Precedence::kUnary;
};

// ============================================================================
// Exponential and Logarithmic Functions
// ============================================================================

template <>
struct DisplayTraits<ExpOp> {
  static constexpr StaticString symbol = "exp";
  static constexpr DisplayMode mode = DisplayMode::kPrefix;
  static constexpr int precedence = Precedence::kUnary;
};

template <>
struct DisplayTraits<LogOp> {
  static constexpr StaticString symbol = "log";
  static constexpr DisplayMode mode = DisplayMode::kPrefix;
  static constexpr int precedence = Precedence::kUnary;
};

template <>
struct DisplayTraits<SqrtOp> {
  static constexpr StaticString symbol = "√";
  static constexpr DisplayMode mode = DisplayMode::kPrefix;
  static constexpr int precedence = Precedence::kUnary;
};

// ============================================================================
// Mathematical Constants (zero-argument operations)
// ============================================================================

template <>
struct DisplayTraits<PiOp> {
  static constexpr StaticString symbol = "π";
  static constexpr DisplayMode mode = DisplayMode::kPrefix;
  static constexpr int precedence = Precedence::kAtomic;
};

template <>
struct DisplayTraits<EOp> {
  static constexpr StaticString symbol = "e";
  static constexpr DisplayMode mode = DisplayMode::kPrefix;
  static constexpr int precedence = Precedence::kAtomic;
};

}  // namespace tempura::symbolic3
