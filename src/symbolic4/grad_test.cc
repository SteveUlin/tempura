// Tests for Grad<Expr> -- typed-rank gradient objects via nesting
#include "symbolic4/grad.h"

#include "symbolic4/interpreter/eval.h"
#include "unit.h"

using namespace tempura;
using namespace tempura::symbolic4;

auto main() -> int {
  constexpr Symbol<struct X> x;
  constexpr Symbol<struct Y> y;

  // f = x^2 + x*y + y^2
  constexpr auto f = x * x + x * y + y * y;

  // =========================================================================
  // RANK 1: COVECTOR (gradient)
  // =========================================================================

  "grad rank 1: df/dx"_test = [&] {
    constexpr auto g = grad(f);
    // df/dx = 2x + y
    // At x=2, y=2: 2*2 + 2 = 6
    expectNear(6.0, evaluate(g[x], x = 2.0, y = 2.0));
  };

  "grad rank 1: df/dy"_test = [&] {
    constexpr auto g = grad(f);
    // df/dy = x + 2y
    // At x=2, y=2: 2 + 2*2 = 6
    expectNear(6.0, evaluate(g[y], x = 2.0, y = 2.0));
  };

  "grad rank 1: different point"_test = [&] {
    constexpr auto g = grad(f);
    // df/dx = 2x + y, at x=3, y=1: 6 + 1 = 7
    expectNear(7.0, evaluate(g[x], x = 3.0, y = 1.0));
    // df/dy = x + 2y, at x=3, y=1: 3 + 2 = 5
    expectNear(5.0, evaluate(g[y], x = 3.0, y = 1.0));
  };

  // =========================================================================
  // RANK 2: HESSIAN via grad(grad(f))
  // =========================================================================

  "grad rank 2: d2f/dx2"_test = [&] {
    constexpr auto H = grad(grad(f));
    // d2f/dx2 = 2 (constant)
    expectNear(2.0, evaluate(H[x, x], x = 0.0, y = 0.0));
    expectNear(2.0, evaluate(H[x, x], x = 5.0, y = 3.0));
  };

  "grad rank 2: d2f/dxdy"_test = [&] {
    constexpr auto H = grad(grad(f));
    // d2f/dxdy = 1 (constant)
    expectNear(1.0, evaluate(H[x, y], x = 0.0, y = 0.0));
  };

  "grad rank 2: d2f/dydx"_test = [&] {
    constexpr auto H = grad(grad(f));
    // d2f/dydx = 1 (symmetry)
    expectNear(1.0, evaluate(H[y, x], x = 0.0, y = 0.0));
  };

  "grad rank 2: d2f/dy2"_test = [&] {
    constexpr auto H = grad(grad(f));
    // d2f/dy2 = 2 (constant)
    expectNear(2.0, evaluate(H[y, y], x = 0.0, y = 0.0));
  };

  // =========================================================================
  // EXPLICIT ROW EXTRACTION: H.row(x) returns a Grad (rank-1 covector)
  // Note: H[x] is a compile error -- partial indexing must be explicit.
  // =========================================================================

  "row extraction: H.row(x) gives rank 1"_test = [&] {
    constexpr auto H = grad(grad(f));
    constexpr auto Hx = H.row(x);  // Grad<df/dx> -- explicit rank-1 covector
    static_assert(is_grad_v<decltype(Hx)>, "H.row(x) should return a Grad");

    // Hx[x] = d2f/dx2 = 2
    expectNear(2.0, evaluate(Hx[x], x = 0.0, y = 0.0));
    // Hx[y] = d2f/dxdy = 1
    expectNear(1.0, evaluate(Hx[y], x = 0.0, y = 0.0));
  };

  "row extraction: H.row(y) gives rank 1"_test = [&] {
    constexpr auto H = grad(grad(f));
    constexpr auto Hy = H.row(y);  // Grad<df/dy> -- explicit rank-1 covector
    static_assert(is_grad_v<decltype(Hy)>, "H.row(y) should return a Grad");

    // Hy[x] = d2f/dydx = 1
    expectNear(1.0, evaluate(Hy[x], x = 0.0, y = 0.0));
    // Hy[y] = d2f/dy2 = 2
    expectNear(2.0, evaluate(Hy[y], x = 0.0, y = 0.0));
  };

  // =========================================================================
  // HESSIAN CONVENIENCE
  // =========================================================================

  "hessian() convenience"_test = [&] {
    constexpr auto H = hessian(f);
    expectNear(2.0, evaluate(H[x, x], x = 0.0, y = 0.0));
    expectNear(1.0, evaluate(H[x, y], x = 0.0, y = 0.0));
    expectNear(1.0, evaluate(H[y, x], x = 0.0, y = 0.0));
    expectNear(2.0, evaluate(H[y, y], x = 0.0, y = 0.0));
  };

  // =========================================================================
  // TRANSCENDENTAL FUNCTIONS
  // =========================================================================

  "grad of transcendental"_test = [&] {
    constexpr auto g_expr = sin(x) * exp(y);
    constexpr auto g = grad(g_expr);
    // d/dx[sin(x)*exp(y)] = cos(x)*exp(y)
    // At x=0, y=0: cos(0)*exp(0) = 1*1 = 1
    expectNear(1.0, evaluate(g[x], x = 0.0, y = 0.0));
    // d/dy[sin(x)*exp(y)] = sin(x)*exp(y)
    // At x=0, y=0: sin(0)*exp(0) = 0*1 = 0
    expectNear(0.0, evaluate(g[y], x = 0.0, y = 0.0));
  };

  "hessian of transcendental"_test = [&] {
    constexpr auto g_expr = sin(x) * exp(y);
    constexpr auto H = hessian(g_expr);
    // d2/dx2[sin(x)*exp(y)] = -sin(x)*exp(y)
    // At x=0, y=0: 0
    expectNear(0.0, evaluate(H[x, x], x = 0.0, y = 0.0));
    // d2/dxdy[sin(x)*exp(y)] = cos(x)*exp(y)
    // At x=0, y=0: 1
    expectNear(1.0, evaluate(H[x, y], x = 0.0, y = 0.0));
    // d2/dy2[sin(x)*exp(y)] = sin(x)*exp(y)
    // At x=0, y=0: 0
    expectNear(0.0, evaluate(H[y, y], x = 0.0, y = 0.0));
  };

  // =========================================================================
  // TYPE CHECKS
  // =========================================================================

  "grad is not Symbolic"_test = [&] {
    constexpr auto g = grad(f);
    // Grad is a lookup object, not a SymbolicTag
    static_assert(!std::is_base_of_v<SymbolicTag, decltype(g)>);
    // But the result of indexing IS symbolic
    static_assert(std::is_base_of_v<SymbolicTag, decltype(g[x])>);
  };

  "grad of grad nesting"_test = [&] {
    constexpr auto g = grad(f);
    constexpr auto H = grad(g);
    static_assert(is_grad_v<decltype(H)>);
    static_assert(is_grad_v<decltype(H.row(x))>);  // row() returns Grad
    static_assert(!is_grad_v<decltype(H[x, y])>);  // full index returns scalar
  };

  return TestRegistry::result();
}
