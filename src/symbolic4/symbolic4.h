#pragma once

// symbolic4: AST-as-types with pluggable interpreters
//
// Key design principles:
// - Types ARE the representation (no runtime AST data structures)
// - Everything is either Atom<Id, Effect> or Expression<Op, Args...>
// - Direct recursion with if constexpr for evaluation/toString
// - Strategy system (Stratego-inspired) for expression transforms
// - Identity tracking (let, IdSet) enables DAG representation
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

// User-defined literals: 42_c (double), 5_z (integer fraction), "1/2"_f (fraction)
#include "symbolic4/constants.h"

// Compressed storage: Pair, CompressedTuple
#include "symbolic4/compressed.h"

// Closed rational arithmetic: Fraction ⊕ Fraction → Fraction
// Must come before operators.h so these more-specific overloads are found first
#include "symbolic4/fraction_ops.h"

// Operator overloads: +, -, *, /, sin, cos, exp, log, pow, sqrt
#include "symbolic4/operators.h"

// Identity tracking: let, LetNode, IdSet
#include "symbolic4/let.h"

// Interpreters: evaluate, simplify, toString
#include "symbolic4/interpreter/eval.h"
#include "symbolic4/interpreter/simplify.h"
#include "symbolic4/interpreter/partial_eval_exact.h"
#include "symbolic4/interpreter/to_string.h"

// Strategy-based differentiation
#include "symbolic4/strategy/diff.h"

// Probabilistic programming: distributions, random variables, model
#include "symbolic4/distributions/distributions.h"

// MCMC inference: transforms, posterior, support inference
#include "symbolic4/mcmc/transforms.h"
#include "symbolic4/mcmc/support.h"
#include "symbolic4/mcmc/posterior.h"

// Unified model API
#include "symbolic4/model.h"

// Simplified inference API with auto-discovery
#include "symbolic4/infer.h"
