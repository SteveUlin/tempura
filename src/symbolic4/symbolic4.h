#pragma once

// symbolic4: AST-as-types with pluggable interpreters
//
// Key design principles:
// - Types ARE the representation (no runtime AST data structures)
// - Everything is either Atom<Id, Strategy> or Expression<Op, Args...>
// - Recursion schemes (fold, para) provide pluggable tree traversal
// - Identity tracking (let, fold_unique) enables DAG representation
//
// Usage:
//   #include "symbolic4/symbolic4.h"
//   using namespace tempura::symbolic4;
//
//   Symbol<struct X> x;
//   Symbol<struct Y> y;
//   auto expr = sin(x * x + y);
//   double result = evaluate(expr, x = 3.0, y = 4.0);
//   auto deriv = diff(expr, x);

// Core types: Atom, Expression, Constant, Fraction, Symbol, Literal
#include "symbolic4/core.h"

// Compressed storage: Pair, CompressedTuple
#include "symbolic4/compressed.h"

// Operator overloads: +, -, *, /, sin, cos, exp, log, pow, sqrt
#include "symbolic4/operators.h"

// Identity tracking: let, LetNode, IdSet
#include "symbolic4/let.h"

// Recursion schemes: fold (catamorphism), para (paramorphism)
#include "symbolic4/scheme/fold.h"
#include "symbolic4/scheme/para.h"
#include "symbolic4/scheme/fold_unique.h"

// Interpreters: evaluate, diff, toString
#include "symbolic4/interpreter/eval.h"
#include "symbolic4/interpreter/diff.h"
#include "symbolic4/interpreter/to_string.h"
