#pragma once

#include "symbolic4/indexed/reduce_over.h"

// ============================================================================
// sum_over.h - Backwards compatibility wrapper
// ============================================================================
//
// SumOver<DimTag, Expr> is now an alias for ReduceOver<SumReduce, DimTag, Expr>.
// All types, traits, and factory functions are provided by reduce_over.h.
//
// This header exists for backwards compatibility — existing code that includes
// sum_over.h will continue to work unchanged.
//
// ============================================================================
