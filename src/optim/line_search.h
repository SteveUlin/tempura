#pragma once

namespace tempura {

// When given a 1D function, we want to make a "good enough" step to minimize
// the value. That is, we start at ϕ(0), given ϕ'(0) < 0. How big of a step α
// should we take to make ϕ(α) as small as possible.
//
// We want to avoid 2 things:
//   - Taking too small of steps and having slow convergence
//   - Taking too large of steps and overshooting the min value
//
// Idea! Take steps in proportion to the promise
//
// For small α, ϕ(α) ≈ ϕ(0) + α·ϕ'(0)

}  // namespace tempura
