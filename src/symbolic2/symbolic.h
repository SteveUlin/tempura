#pragma once

// Tempura Symbolic 2.0 - A header-only C++26 library for compile-time symbolic
// mathematics and expression manipulation.
//
// This header provides a refactored, modular structure for easier maintenance:
//
// - core.h: Core types (Symbol, Constant, Expression, Symbolic concept)
// - binding.h: Type-value binding for evaluation (Binder, BinderPack)
// - constants.h: Constant literals (_c suffix)
// - accessors.h: Expression accessor functions (left, right, operand, getOp)
// - matching.h: Pattern matching for symbolic expressions
// - evaluate.h: Expression evaluation
// - operators.h: Operator overloads (+, -, *, /, sin, cos, etc.)
// - ordering.h: Symbolic expression comparison and ordering
// - simplify.h: Simplification rules and algorithms
// - to_string.h: String conversion utilities
//
// Example usage:
//   Symbol x;
//   Symbol y;
//   auto expr = x + y;
//   println("x + y = ", evaluate(expr, BinderPack{x = 1, y = 2}));

#include "symbolic2/accessors.h"
#include "symbolic2/binding.h"
#include "symbolic2/constants.h"
#include "symbolic2/core.h"
#include "symbolic2/evaluate.h"
#include "symbolic2/matching.h"
#include "symbolic2/operators.h"
#include "symbolic2/ordering.h"
#include "symbolic2/simplify.h"
#include "symbolic2/to_string.h"
