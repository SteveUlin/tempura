#pragma once

// Expression-Graph DSL for Probabilistic Programming
//
// A minimal DSL that produces symbolic expressions for Bayesian models.
// The DSL builds a DAG of random variables; users evaluate and differentiate
// using standard symbolic3 machinery.
//
// Usage:
//
//   auto mu = Normal(0, 10);       // RandomVar with Normal prior
//   auto sigma = HalfNormal(5);    // RandomVar with HalfNormal prior
//   auto y = Normal(mu, sigma);    // RandomVar depending on mu, sigma
//
//   // Get symbolic joint log-probability
//   auto lp = jointLogProb(y);
//
//   // Evaluate at concrete values
//   double val = evaluate(lp, BinderPack{mu = 1.0, sigma = 2.0, y = 2.5});
//
//   // Compute gradients
//   auto dlp_dmu = diff(lp, mu);
//   double grad = evaluate(dlp_dmu, BinderPack{mu = 1.0, sigma = 2.0, y = 2.5});
//
// The DSL provides:
//   1. RandomVar / DeterministicVar - Nodes in the probabilistic DAG
//   2. Distribution factories - Normal(), HalfNormal(), Beta(), etc.
//   3. jointLogProb(rv...) - Traverse DAG, return symbolic log-prob
//   4. rv = value - Binding syntax for BinderPack
//   5. diff(expr, rv) - Differentiate w.r.t. a RandomVar

#include "bayes/graph/core.h"
#include "bayes/graph/distributions.h"
#include "bayes/graph/operators.h"
#include "bayes/graph/traversal.h"
#include "symbolic3/evaluate.h"
