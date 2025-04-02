#pragma once

#include <cassert>
#include <concepts>
#include <cstddef>
#include <ranges>
#include <utility>

#include "matrix3/extents.h"

// GCC libstdc++ does not support <mdspan> yet. The following is based on
// mdspan but is:
//   - simplified
//   - explicitly supports matrix owning their underlying data
//   - uses the c++26 multidimensional bracket notation
//   - uses Google naming conventions
//
// The goal here is not to recreate mdspan but to write my own interface

// A Matrix is created from 3 sub elements
// - Extents
//   A compile time and runtime description of N-dimensional matrices
// - Layout
//   Takes N indices an produces a simplified IndexType, usually a reduction
//   to a single index.
// - Accessor
//   Links the IndexType from a Layout to some underlying data and produces the
//   actual result.

namespace tempura::matrix3 {

// --- Layouts ---


// --- Accessors ---


}  // namespace tempura::matrix3
