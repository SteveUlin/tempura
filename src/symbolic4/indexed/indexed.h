#pragma once

// ============================================================================
// indexed.h - Plate notation for symbolic4
// ============================================================================
//
// Umbrella header providing:
//   - Dimension tags (Dim<Tag>)
//   - Indexed symbols (IndexedSymbol<Id, DimTag>)
//   - Symbolic summation (SumOver<DimTag, Expr>)
//   - Indexed evaluation (evaluateIndexed)
//   - Indexed bindings (indexed(values))
//
// Example:
//   using namespace tempura::symbolic4;
//
//   struct Observations {};
//
//   auto alpha = halfNormal(5.0);
//   auto theta = plate<Observations>(beta(alpha, lit(3.0)));
//
//   // Total log-prob = Σᵢ log Beta(θ[i] | α, 3)
//   auto lp = jointLogProb(theta);
//
//   // Evaluate with indexed values
//   std::vector<double> theta_vals = {0.3, 0.5, 0.7};
//   double result = evaluateIndexed(lp, alpha = 2.0, theta = indexed(theta_vals));
//
// ============================================================================

#include "symbolic4/distributions/observed.h"
#include "symbolic4/indexed/data.h"
#include "symbolic4/indexed/dim.h"
#include "symbolic4/indexed/indexed_eval.h"
#include "symbolic4/indexed/sum_over.h"
