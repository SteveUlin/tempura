#include "autodiff/node.h"

#include <iostream>

#include "unit.h"

using namespace tempura;
using namespace autodiff;

auto main() -> int {
  "Addition"_test = [] {
    {
      Variable x;
      Variable y;
      auto z = x + y;
      const auto [val, dx, dy] = differentiate(z, Wrt{x, y}, At{1., 2.});
      expectNear(3., val);
      expectNear(1., dx);
      expectNear(1., dy);
    }
    {
      Variable x;
      Variable y;
      NodeExpr z = x;
      z += y;
      const auto [val, dx, dy] = differentiate(z, Wrt{x, y}, At{1., 2.});
      expectNear(3., val);
      expectNear(1., dx);
      expectNear(1., dy);
    }
  };

  "Addition with constant"_test = [] {
    {
      Variable x;
      auto z = x + 1.;
      const auto [val, dx] = differentiate(z, Wrt{x}, At{1.});
      expectNear(2., val);
      expectNear(1., dx);
    }
    {
      Variable x;
      auto z = 1. + x;
      const auto [val, dx] = differentiate(z, Wrt{x}, At{1.});
      expectNear(2., val);
      expectNear(1., dx);
    }
    {
      Variable x;
      NodeExpr z = x;
      z += 1.;
      const auto [val, dx] = differentiate(z, Wrt{x}, At{1.});
      expectNear(2., val);
      expectNear(1., dx);
    }
  };

  "Subtraction"_test = [] {
    {
      Variable x;
      Variable y;
      auto z = x - y;
      const auto [val, dx, dy] = differentiate(z, Wrt{x, y}, At{1., 2.});
      expectNear(-1., val);
      expectNear(1., dx);
      expectNear(-1., dy);
    }
    {
      Variable x;
      Variable y;
      NodeExpr z = x;
      z -= y;
      const auto [val, dx, dy] = differentiate(z, Wrt{x, y}, At{1., 2.});
      expectNear(-1., val);
      expectNear(1., dx);
      expectNear(-1., dy);
    }
  };

  "Subtraction with constant"_test = [] {
    {
      Variable x;
      auto z = x - 1.;
      const auto [val, dx] = differentiate(z, Wrt{x}, At{1.});
      expectNear(0., val);
      expectNear(1., dx);
    }
    {
      Variable x;
      auto z = 1. - x;
      const auto [val, dx] = differentiate(z, Wrt{x}, At{1.});
      expectNear(0., val);
      expectNear(-1., dx);
    }
    {
      Variable x;
      NodeExpr z = x;
      z -= 1.;
      const auto [val, dx] = differentiate(z, Wrt{x}, At{1.});
      expectNear(0., val);
      expectNear(1., dx);
    }
  };

  "Multiplication"_test = [] {
    {
      Variable x;
      Variable y;
      auto z = x * y;
      const auto [val, dx, dy] = differentiate(z, Wrt{x, y}, At{2., 3.});
      expectNear(6., val);
      expectNear(3., dx);
      expectNear(2., dy);
    }
    {
      Variable x;
      Variable y;
      NodeExpr z = x;
      z *= y;
      const auto [val, dx, dy] = differentiate(z, Wrt{x, y}, At{2., 3.});
      expectNear(6., val);
      expectNear(3., dx);
      expectNear(2., dy);
    }
  };

  "Multiplication with constant"_test = [] {
    {
      Variable x;
      auto z = x * 2.;
      const auto [val, dx] = differentiate(z, Wrt{x}, At{2.});
      expectNear(4., val);
      expectNear(2., dx);
    }
    {
      Variable x;
      auto z = 2. * x;
      const auto [val, dx] = differentiate(z, Wrt{x}, At{2.});
      expectNear(4., val);
      expectNear(2., dx);
    }
    {
      Variable x;
      NodeExpr z = x;
      z *= 2.;
      const auto [val, dx] = differentiate(z, Wrt{x}, At{2.});
      expectNear(4., val);
      expectNear(2., dx);
    }
  };

  "Division"_test = [] {
    {
      Variable x;
      Variable y;
      auto z = x / y;
      const auto [val, dx, dy] = differentiate(z, Wrt{x, y}, At{1., 2.});
      expectNear(0.5, val);
      expectNear(0.5, dx);
      expectNear(-0.25, dy);
    }
    {
      Variable x;
      Variable y;
      NodeExpr z = x;
      z /= y;
      const auto [val, dx, dy] = differentiate(z, Wrt{x, y}, At{1., 2.});
      expectNear(0.5, val);
      expectNear(0.5, dx);
      expectNear(-0.25, dy);
    }
  };

  "Division with constant"_test = [] {
    {
      Variable x;
      auto z = x / 2.;
      const auto [val, dx] = differentiate(z, Wrt{x}, At{2.});
      expectNear(1., val);
      expectNear(0.5, dx);
    }
    {
      Variable x;
      auto z = 2. / x;
      const auto [val, dx] = differentiate(z, Wrt{x}, At{2.});
      expectNear(1., val);
      expectNear(-0.5, dx);
    }
    {
      Variable x;
      NodeExpr z = x;
      z /= 2.;
      const auto [val, dx] = differentiate(z, Wrt{x}, At{2.});
      expectNear(1., val);
      expectNear(0.5, dx);
    }
  };

  return 0;
}
