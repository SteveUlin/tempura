#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <limits>

#include "quadature/gaussian.h"
#include "sequence.h"

namespace tempura::special {

// Error Function
//
// Technically erf and cerf are special cases of the incomplete gamma function.
// But it is common enough to warrant its own implementation.
//
//           ⌠x
// erf(x)  = ⎮ exp(-t²) dt
//           ⌡0
//
//
//                        ⌠∞
// cerf(x) = 1 - erf(x) = ⎮ exp(-t²) dt
//                        ⌡x
//
// Limits and Symmetry:
// erf(0) = 0,  erf(∞) = 1,  erf(-x) = -erf(x)
//
// cerf(0) = 1, cerf(∞) = 0, cerf(-x) = 2 - cerf(x)

}  // namespace tempura::special
