#pragma once

// Main header for symbolic3 combinator system
// Includes all necessary components for building and transforming symbolic
// expressions

#include "symbolic3/canonical.h"
#include "symbolic3/context.h"
#include "symbolic3/core.h"
#include "symbolic3/debug.h"
#include "symbolic3/derivative.h"
#include "symbolic3/evaluate.h"
#include "symbolic3/fraction.h"
#include "symbolic3/matching.h"
#include "symbolic3/operators.h"
#include "symbolic3/pattern_matching.h"
#include "symbolic3/simplify.h"
#include "symbolic3/strategy.h"
#include "symbolic3/to_string.h"
#include "symbolic3/transforms.h"
#include "symbolic3/traversal.h"

namespace tempura::symbolic3 {

// Re-export commonly used types and functions
using tempura::symbolic3::Constant;
using tempura::symbolic3::Expression;
using tempura::symbolic3::Symbol;

// Re-export context factories
using tempura::symbolic3::default_context;
using tempura::symbolic3::numeric_context;
using tempura::symbolic3::symbolic_context;

// Re-export common strategies
using tempura::symbolic3::ApplyAlgebraicRules;
using tempura::symbolic3::FoldConstants;
using tempura::symbolic3::Identity;

// Traversal strategy factories (template functions, use qualified names):
//   - tempura::symbolic3::fold(strategy)      - bottom-up transformation
//   - tempura::symbolic3::unfold(strategy)    - top-down transformation
//   - tempura::symbolic3::innermost(strategy) - apply at leaves first
//   - tempura::symbolic3::outermost(strategy) - apply at root first
//   - tempura::symbolic3::topdown(strategy)   - pre-order traversal
//   - tempura::symbolic3::bottomup(strategy)  - post-order traversal

// Re-export predefined pipelines
// Note: simplify is now the canonical function (alias for full_simplify)
using tempura::symbolic3::algebraic_simplify;
using tempura::symbolic3::algebraic_simplify_recursive;
using tempura::symbolic3::bottomup_simplify;
using tempura::symbolic3::full_simplify;  // Explicit name for the same thing
using tempura::symbolic3::simplify;       // CANONICAL: use this for most cases
using tempura::symbolic3::topdown_simplify;
using tempura::symbolic3::trig_aware_simplify;

// Legacy (not recommended for new code)
using tempura::symbolic3::simplify_bounded;  // Old fixed-iteration version

}  // namespace tempura::symbolic3
