#pragma once

// bayes2 - Compile-time probabilistic programming DSL
//
// Usage:
//   auto mu = normal(0.0, 10.0);
//   auto sigma = halfNormal(5.0);
//   auto y = normal(mu, sigma);
//
//   auto lp = jointLogProb(y);
//   double val = evaluate(lp, BinderPack{mu = 1.0, sigma = 2.0, y = 2.5});

#include "bayes2/core.h"
#include "bayes2/distributions.h"
#include "bayes2/indexed.h"
#include "bayes2/operators.h"
#include "bayes2/reparam.h"
#include "bayes2/sample.h"
#include "bayes2/traversal.h"
