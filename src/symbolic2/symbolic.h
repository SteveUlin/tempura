#pragma once

// Tempura Symbolic 2.0 - Compile-time symbolic mathematics for C++26
//
// Design philosophy:
// - Zero runtime overhead: all computation in type system
// - Declarative rewrite rules using pattern matching
// - Unique type identity via stateless lambdas
// - Heterogeneous bindings for evaluation
//
// Example:
//   Symbol x, y;
//   auto f = x*x + 2_c*x + 1_c;
//   auto df_dx = simplify(diff(f, x));  // 2·x + 2
//   auto val = evaluate(f, BinderPack{x = 3});  // 16

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
